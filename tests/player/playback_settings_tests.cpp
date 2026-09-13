#include "rpcmp/library/container.hpp"
#include "rpcmp/player/player_session.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <deque>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace rpcmp::player;
namespace api = rpcmp::contracts::v2;
using rpcmp::contracts::TransportState;
using Restore = api::SettingsRestore;
using Save = api::SettingsSave;
using Error = api::SettingsError;
constexpr SettingsTiming timing{1'000, 1'000, 100};
constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
template <typename T> T& required(std::optional<T>& value) {
  if (!value)
    throw std::logic_error("missing scripted value");
  return *value;
}
template <typename T> T required(const std::optional<T>& value) {
  if (!value)
    throw std::logic_error("missing scripted value");
  return *value;
}
api::PlaybackPolicy counted(std::uint32_t count, bool shuffle = false) {
  return {shuffle ? api::PlaybackOrder::ShuffleLibrary : api::PlaybackOrder::AlbumOrder,
          api::RepeatMode::Counted, count};
}
SettingsSlot record(std::uint64_t sequence, api::PlaybackPolicy policy = {}) {
  SettingsSlot slot{SettingsSlotState::Bytes, {}, 64};
  if (!encode_settings({sequence, policy}, slot.bytes))
    throw std::logic_error("bad authored record");
  return slot;
}
void repair_crc(SettingsSlot& slot) {
  const auto crc = rpcmp::library::crc32({slot.bytes.data(), 60});
  for (unsigned i = 0; i < 4; ++i)
    slot.bytes[60 + i] = static_cast<std::uint8_t>(crc >> (8 * i));
}
class Storage final : public PlaybackSettingsPort {
public:
  bool begin(const SettingsRequest& request) override {
    if (!quiet || active)
      throw std::logic_error("overlapping storage");
    requests.push_back(request);
    if (reject_begin)
      return false;
    active = request;
    quiet = false;
    return true;
  }
  std::optional<SettingsCompletion> poll() override {
    ++polls;
    if (completions.empty())
      return std::nullopt;
    const auto result = completions.front();
    completions.pop_front();
    return result;
  }
  void cancel(std::uint64_t id) override {
    if ((active && active->id != id) || (!active && !quiet))
      throw std::logic_error("wrong cancel owner");
    ++cancels;
  }
  bool quiescent(std::uint64_t) override {
    ++quiet_checks;
    return quiet;
  }
  void release() {
    quiet = true;
    active.reset();
  }
  void finish(bool durable = true, SettingsIoResult outcome = SettingsIoResult::Success) {
    const auto r = required(active);
    SettingsCompletion c;
    c.id = r.id;
    c.operation = r.operation;
    c.revision = r.revision;
    c.slot = r.slot;
    c.result = outcome;
    c.slots = slots;
    if (r.operation == SettingsOperation::Commit) {
      slots[r.slot] = {SettingsSlotState::Bytes, r.bytes, 64};
      c.readback = slots[r.slot];
      c.durable = durable;
    }
    completions.push_back(c);
    release();
  }
  std::array<SettingsSlot, 2> slots{};
  std::optional<SettingsRequest> active;
  std::deque<SettingsCompletion> completions;
  std::vector<SettingsRequest> requests;
  unsigned polls{}, cancels{}, quiet_checks{};
  bool quiet{true}, reject_begin{};
};
void tick(rpcmp::test::Suite& suite, PlaybackSettings& settings, std::uint64_t now) {
  settings.step(now);
  RPCMP_CHECK(suite, api::valid_settings_observation(settings.snapshot()));
}
void boot(rpcmp::test::Suite& suite, PlaybackSettings& settings, Storage& port) {
  RPCMP_CHECK(suite, port.requests.empty() && !settings.ready());
  tick(suite, settings, 0);
  port.finish();
  tick(suite, settings, 1);
  RPCMP_CHECK(suite, settings.ready());
}

void records(rpcmp::test::Suite& suite) {
  // Independently packed with Python struct + zlib, not the production encoder.
  constexpr char authored[] = "52505331010040000700000000000000010201000500000000000000000000000000"
                              "0000000000000000000000000000000000000000000000000000abb5281a";
  SettingsSlot golden{SettingsSlotState::Bytes, {}, 64};
  for (std::size_t i = 0; i < golden.bytes.size(); ++i)
    golden.bytes[i] =
        static_cast<std::uint8_t>(std::stoul(std::string(authored + i * 2, 2), nullptr, 16));
  RPCMP_CHECK(suite, record(7, counted(5, true)).bytes == golden.bytes);
  SettingsRecord output;
  RPCMP_CHECK(suite, decode_settings(golden, output) == SettingsDecode::Valid &&
                         output.sequence == 7 && output.policy == counted(5, true));
  constexpr std::array<std::uint8_t, 9> check{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  RPCMP_CHECK(suite, rpcmp::library::crc32({check.data(), check.size()}) == 0xcbf43926);
  for (std::uint16_t length : std::array<std::uint16_t, 6>{0, 5, 6, 63, 65, 65535}) {
    auto bad = golden;
    bad.length = length;
    RPCMP_CHECK(suite, decode_settings(bad, output) == SettingsDecode::Invalid);
  }
  for (const std::size_t offset :
       std::array<std::size_t, 11>{0, 6, 8, 16, 17, 18, 19, 20, 24, 59, 60}) {
    auto bad = golden;
    if (offset == 8 || offset == 20)
      bad.bytes[offset] = 0;
    else
      bad.bytes[offset] = 0xff;
    if (offset != 60)
      repair_crc(bad);
    RPCMP_CHECK(suite,
                decode_settings(bad, output) == SettingsDecode::Invalid && output.sequence == 7);
  }
  auto future = golden;
  future.bytes[4] = 2;
  future.length = 6;
  RPCMP_CHECK(suite, decode_settings(future, output) == SettingsDecode::Unsupported);
  auto stray_count = record(1);
  stray_count.bytes[20] = 1;
  repair_crc(stray_count);
  RPCMP_CHECK(suite, decode_settings(stray_count, output) == SettingsDecode::Invalid);
  for (const auto count : {1U, 2U, 4U, std::numeric_limits<std::uint32_t>::max()})
    RPCMP_CHECK(suite,
                decode_settings(record(maximum, counted(count)), output) == SettingsDecode::Valid &&
                    output.policy.count == count);
  auto bytes = golden.bytes;
  RPCMP_CHECK(suite, !encode_settings({0, {}}, bytes) && bytes == golden.bytes);
  RPCMP_CHECK(suite, !encode_settings({1, counted(0)}, bytes) && bytes == golden.bytes);
}

void restoration(rpcmp::test::Suite& suite) {
  struct Case {
    SettingsSlot a, b;
    Restore restore;
    Save save;
    api::PlaybackPolicy policy;
    std::uint8_t destination;
  };
  auto corrupt = record(10);
  corrupt.bytes[60] ^= 1;
  auto unknown = record(10);
  unknown.bytes[4] = 2;
  const std::array cases{
      Case{{}, {}, Restore::Missing, Save::NotSaved, {}, 0},
      Case{record(7), record(8, counted(3)), Restore::Restored, Save::Saved, counted(3), 0},
      Case{record(8, counted(5)), record(7), Restore::Restored, Save::Saved, counted(5), 1},
      Case{record(7), record(7), Restore::Restored, Save::Saved, {}, 1},
      Case{record(7), {}, Restore::Restored, Save::Saved, {}, 1},
      Case{record(7), corrupt, Restore::Recovered, Save::Saved, {}, 1},
      Case{corrupt, {}, Restore::Invalid, Save::Failed, {}, 0},
      Case{record(7), record(7, counted(3)), Restore::Conflict, Save::Failed, {}, 0},
      Case{record(7, counted(5)),
           {SettingsSlotState::IoError},
           Restore::IoError,
           Save::Failed,
           {},
           0},
      Case{record(7, counted(5)), unknown, Restore::Unsupported, Save::Unavailable, counted(5), 1},
      Case{{SettingsSlotState::IoError}, unknown, Restore::Unsupported, Save::Unavailable, {}, 0}};
  for (const auto& item : cases) {
    Storage port;
    port.slots = {item.a, item.b};
    PlaybackSettings settings({&port, timing});
    boot(suite, settings, port);
    RPCMP_CHECK(suite, settings.snapshot().restore == item.restore &&
                           settings.snapshot().save == item.save &&
                           settings.policy() == item.policy);
    tick(suite, settings, 1'000'000);
    RPCMP_CHECK(suite, port.requests.size() == 1);
    settings.observe(counted(9), 2, 1'000'000);
    tick(suite, settings, 1'250'000);
    if (item.restore == Restore::Unsupported) {
      RPCMP_CHECK(suite,
                  port.requests.size() == 1 && settings.snapshot().save == Save::Unavailable);
    } else if (item.restore == Restore::IoError) {
      RPCMP_CHECK(suite, required(port.active).operation == SettingsOperation::Read);
    } else {
      RPCMP_CHECK(suite, required(port.active).operation == SettingsOperation::Commit &&
                             required(port.active).slot == item.destination);
    }
  }
  // Simulate a power loss at every prefix of replacing slot B. A remains intact.
  const auto old = record(7, counted(3));
  const auto next = record(8, counted(5));
  for (std::size_t written = 0; written <= 64; ++written) {
    Storage port;
    port.slots = {old, record(6)};
    std::copy_n(next.bytes.begin(), written, port.slots[1].bytes.begin());
    const bool complete = port.slots[1].bytes == next.bytes;
    PlaybackSettings settings({&port, timing});
    boot(suite, settings, port);
    RPCMP_CHECK(suite, settings.policy() == (complete ? counted(5) : counted(3)) &&
                           port.slots[0].bytes == old.bytes);
  }
}

void revisions_and_failures(rpcmp::test::Suite& suite) {
  Storage port;
  PlaybackSettings settings({&port, timing});
  boot(suite, settings, port);
  settings.observe(counted(3), 2, 2);
  tick(suite, settings, 250'001);
  RPCMP_CHECK(suite, port.requests.size() == 1 && settings.snapshot().save == Save::Pending);
  tick(suite, settings, 250'002);
  const auto first = required(port.active);
  settings.observe(counted(5), 3, 250'003);
  RPCMP_CHECK(suite,
              required(port.active).revision == 2 && required(port.active).bytes == first.bytes);
  port.finish();
  tick(suite, settings, 250'004);
  RPCMP_CHECK(suite, settings.snapshot().persisted_revision == 2 &&
                         settings.snapshot().save == Save::Pending);
  tick(suite, settings, 500'003);
  RPCMP_CHECK(suite, required(port.active).revision == 3 && required(port.active).slot == 1);
  port.finish();
  tick(suite, settings, 500'004);
  RPCMP_CHECK(suite, settings.snapshot().persisted_revision == 3 &&
                         settings.snapshot().save == Save::Saved &&
                         settings.snapshot().restore == Restore::Missing);
  settings.observe(counted(5), 3, 500'005);
  tick(suite, settings, 800'005);
  RPCMP_CHECK(suite, port.requests.size() == 3);
  settings.observe(counted(4), 4, 800'006);
  tick(suite, settings, 1'050'006);
  port.finish(false);
  tick(suite, settings, 1'050'007);
  RPCMP_CHECK(suite, settings.snapshot().save == Save::Failed &&
                         settings.snapshot().error == Error::NotDurable &&
                         settings.snapshot().persisted_revision == 3);
  tick(suite, settings, 2'000'000);
  RPCMP_CHECK(suite, port.requests.size() == 4);
  settings.observe(counted(9), 5, 2'000'001);
  tick(suite, settings, 2'250'001);
  RPCMP_CHECK(suite, required(port.active).operation == SettingsOperation::Read);
  port.finish();
  tick(suite, settings, 2'250'002);
  RPCMP_CHECK(suite,
              settings.policy() == counted(9) && settings.snapshot().persisted_revision == 3);
  tick(suite, settings, 2'250'003);
  SettingsRecord retried;
  RPCMP_CHECK(suite, decode_settings({SettingsSlotState::Bytes, required(port.active).bytes, 64},
                                     retried) == SettingsDecode::Valid &&
                         retried.sequence == 4 && retried.policy == counted(9));
  port.finish();
  port.completions.front().readback.bytes[20] ^= 1;
  tick(suite, settings, 2'250'004);
  RPCMP_CHECK(suite, settings.snapshot().error == Error::ReadbackMismatch &&
                         settings.snapshot().save == Save::Failed);
}

void timeouts_and_bounds(rpcmp::test::Suite& suite) {
  Storage port;
  PlaybackSettings settings({&port, timing});
  tick(suite, settings, 0);
  const auto old_read = required(port.active);
  tick(suite, settings, 1000);
  RPCMP_CHECK(suite, settings.ready() && settings.snapshot().restore == Restore::TimedOut &&
                         port.cancels == 1);
  settings.observe(counted(3), 2, 1001);
  tick(suite, settings, 1099);
  RPCMP_CHECK(suite, port.requests.size() == 1);
  port.finish();
  tick(suite, settings, 1100);
  tick(suite, settings, 251001);
  RPCMP_CHECK(suite, required(port.active).operation == SettingsOperation::Read &&
                         required(port.active).id != old_read.id);
  tick(suite, settings, 251002); // Drains the old read without applying it.
  RPCMP_CHECK(suite,
              settings.policy() == counted(3) && settings.snapshot().restore == Restore::TimedOut);
  port.finish();
  tick(suite, settings, 251003);
  tick(suite, settings, 251004);
  RPCMP_CHECK(suite, required(port.active).operation == SettingsOperation::Commit);
  tick(suite, settings, 252004);
  RPCMP_CHECK(suite, port.cancels == 2 && settings.snapshot().error == Error::Timeout);
  tick(suite, settings, 252104);
  RPCMP_CHECK(suite, settings.snapshot().save == Save::Unavailable &&
                         settings.snapshot().error == Error::NotQuiescent);
  port.finish();
  settings.observe(counted(5), 3, 300000);
  tick(suite, settings, 999999);
  RPCMP_CHECK(suite, port.requests.size() == 3 && settings.snapshot().policy_revision == 3);

  Storage exhausted;
  PlaybackSettings no_ids({&exhausted, timing, maximum});
  tick(suite, no_ids, 0);
  RPCMP_CHECK(suite, no_ids.ready() && no_ids.snapshot().error == Error::RequestExhausted &&
                         exhausted.requests.empty());
  Storage full;
  full.slots[0] = record(maximum);
  PlaybackSettings no_sequence({&full, timing});
  boot(suite, no_sequence, full);
  no_sequence.observe(counted(3), 2, 2);
  tick(suite, no_sequence, 250002);
  RPCMP_CHECK(suite, no_sequence.snapshot().error == Error::SequenceExhausted &&
                         full.requests.size() == 1);
  Storage clock;
  PlaybackSettings reversed({&clock, timing});
  boot(suite, reversed, clock);
  tick(suite, reversed, 0);
  RPCMP_CHECK(suite, reversed.snapshot().error == Error::Clock &&
                         reversed.snapshot().save == Save::Unavailable);
  PlaybackSettings invalid({&clock, {}});
  RPCMP_CHECK(suite, invalid.ready() && invalid.snapshot().error == Error::InvalidConfiguration);
  Storage rejecting;
  rejecting.reject_begin = true;
  PlaybackSettings rejected({&rejecting, timing});
  tick(suite, rejected, 0);
  RPCMP_CHECK(suite, rejected.ready() && rejected.snapshot().restore == Restore::IoError &&
                         !rejecting.active);

  Storage protocol;
  PlaybackSettings mismatch({&protocol, timing});
  tick(suite, mismatch, 0);
  const auto r = required(protocol.active);
  SettingsCompletion bad;
  bad.id = r.id;
  bad.revision = 1;
  protocol.completions.push_back(bad);
  tick(suite, mismatch, 1);
  RPCMP_CHECK(suite, mismatch.snapshot().error == Error::Protocol && protocol.cancels == 1);
}

class Preparation final : public PreparationPort {
public:
  bool begin(const PreparationRequest&) override {
    ++calls;
    return true;
  }
  void cancel(std::uint64_t) override { ++calls; }
  PreparationProgress poll(std::uint64_t) override {
    ++calls;
    return PreparationProgress::Ready;
  }
  void release() override { ++calls; }
  unsigned calls{};
};
class Audio final : public AudioTransportPort {
public:
  bool supports_pause() const noexcept override { return true; }
  bool supports_policy() const noexcept override { return supported; }
  bool begin(const AudioControlRequest& request) override {
    pending = request;
    trace.push_back(request.kind);
    return true;
  }
  AudioObservation observe() override {
    ++calls;
    observation.completion.reset();
    if (pending) {
      const auto r = *pending;
      pending.reset();
      if (r.kind == AudioControlKind::Reset)
        observation = {};
      else {
        auto& media = observation.media.emplace();
        const auto repeat = required(r.repeat);
        media.play_generation = r.play_generation;
        media.frame = observation.media_frame;
        media.policy_revision = repeat.revision;
        media.target = repeat.target;
        media.gain = 240000;
        if (r.kind == AudioControlKind::Pause)
          paused = true;
        if (r.kind == AudioControlKind::Start || r.kind == AudioControlKind::Resume)
          paused = false;
        media.paused = paused;
        observation.play_generation = r.play_generation;
      }
      observation.completion =
          AudioControlCompletion{r, AudioControlOutcome::Success, observation.media_frame};
    }
    return observation;
  }
  void emergency_silence() override { ++inhibits; }
  std::optional<AudioControlRequest> pending;
  AudioObservation observation{};
  std::vector<AudioControlKind> trace;
  unsigned calls{}, inhibits{};
  bool supported{true}, paused{};
};
class Random final : public RandomSource {
  bool next(std::uint32_t& value) override {
    value = 42;
    return true;
  }
};
std::vector<std::uint8_t> fixture() {
  std::ifstream input(std::string{RPCMP_SOURCE_DIR} + "/tests/fixtures/rpcmlib/album.rpcmlib.hex");
  std::vector<std::uint8_t> bytes;
  std::string pair;
  char c{};
  while (input.get(c)) {
    if (c == '\r' || c == '\n')
      continue;
    pair += c;
    if (pair.size() == 2) {
      bytes.push_back(static_cast<std::uint8_t>(std::stoul(pair, nullptr, 16)));
      pair.clear();
    }
  }
  return bytes;
}
void session_connection(rpcmp::test::Suite& suite) {
  std::vector<AudioControlKind> dense;
  for (const bool publish : {true, false}) {
    Storage port;
    port.slots[0] = record(7, counted(5, true));
    Preparation preparation;
    Audio audio;
    Random random;
    CatalogSession catalog;
    const auto bytes = fixture();
    const auto ticket = catalog.begin_open();
    RPCMP_CHECK(suite, catalog.complete_open(ticket.generation, {bytes.data(), bytes.size()}).ok());
    PlayerSession session(catalog, preparation, audio, {1'000'000, 1'000'000}, &random, nullptr,
                          {&port, timing});
    const auto step = [&](std::uint64_t now) {
      const auto r = session.step(now, publish);
      RPCMP_CHECK(suite, !r.state.terminal && r.state.failure == TransportFailure::None &&
                             (!publish || r.published));
      RPCMP_CHECK(suite, api::valid_player_snapshot(session.latest()));
      return r.state;
    };
    api::PlayerCommand play;
    play.command_id = 1;
    play.kind = api::CommandKind::PlayTrack;
    play.selection = api::TrackSelection{ticket.generation, {0x47305869eca5f89aULL}};
    step(0);
    RPCMP_CHECK(suite, session.submit(play).reason == api::CommandReason::ResourceBusy);
    const auto polls = port.polls;
    for (int i = 0; i < 10; ++i)
      static_cast<void>(session.latest());
    RPCMP_CHECK(suite, port.polls == polls && audio.trace.size() == 1);
    port.finish();
    const auto restored = step(1);
    RPCMP_CHECK(suite, !restored.selection && restored.policy.desired == counted(5, true) &&
                           restored.policy.revision == 1 && !restored.policy.last_command_id);
    play.command_id = 2;
    RPCMP_CHECK(suite, session.submit(play).outcome == api::CommandOutcome::Accepted);
    step(2);
    step(3);
    step(4);
    const auto playing = step(5);
    RPCMP_CHECK(suite, playing.transport == TransportState::Playing);
    api::PlayerCommand pause;
    pause.command_id = 3;
    pause.kind = api::CommandKind::Pause;
    RPCMP_CHECK(suite, session.submit(pause).outcome == api::CommandOutcome::Accepted);
    step(6);
    RPCMP_CHECK(suite, step(7).transport == TransportState::Paused);
    api::PlayerCommand policy;
    policy.command_id = 4;
    policy.kind = api::CommandKind::SetPlaybackPolicy;
    policy.policy = counted(3, true);
    RPCMP_CHECK(suite, session.submit(policy).outcome == api::CommandOutcome::Accepted);
    step(8);
    step(9);
    step(250008);
    RPCMP_CHECK(suite, required(port.active).operation == SettingsOperation::Commit);
    port.finish(false);
    const auto failed = step(250009);
    RPCMP_CHECK(suite, failed.transport == TransportState::Paused &&
                           failed.media_frame == playing.media_frame &&
                           failed.policy.desired == counted(3, true) && audio.inhibits == 0);
    if (publish) {
      dense = audio.trace;
      auto snapshot = session.latest();
      RPCMP_CHECK(suite, required(snapshot.settings).save == Save::Failed &&
                             required(snapshot.settings).error == Error::NotDurable &&
                             required(snapshot.settings).persisted_revision == 1);
      required(snapshot.settings).policy_revision = 999;
      RPCMP_CHECK(suite, !api::valid_player_snapshot(snapshot) &&
                             api::valid_player_snapshot(session.latest()));
    } else {
      RPCMP_CHECK(suite, audio.trace == dense && session.latest().sequence == 0);
    }
  }
  Storage port;
  port.slots[0] = record(7, counted(5, true));
  Preparation prep;
  Audio audio;
  CatalogSession catalog;
  PlayerSession unsupported(catalog, prep, audio, {1000, 1000}, nullptr, nullptr, {&port, timing});
  static_cast<void>(unsupported.step(0));
  port.finish();
  static_cast<void>(unsupported.step(1));
  const auto view = unsupported.latest();
  RPCMP_CHECK(suite, api::valid_player_snapshot(view) &&
                         required(view.settings).error == Error::UnsupportedBackend &&
                         required(view.policy).desired == api::PlaybackPolicy{} && !view.error);
}
void completion_races(rpcmp::test::Suite& suite) {
  // A result queued at the deadline is already too late to restore settings.
  Storage late;
  late.slots[0] = record(7, counted(5));
  PlaybackSettings expired({&late, timing});
  tick(suite, expired, 0);
  late.finish();
  tick(suite, expired, 1000);
  RPCMP_CHECK(suite, expired.snapshot().restore == Restore::TimedOut &&
                         expired.policy() == api::PlaybackPolicy{});
  tick(suite, expired, 1001);
  RPCMP_CHECK(suite, late.cancels == 1 && late.requests.size() == 1);

  for (unsigned fault = 0; fault < 5; ++fault) {
    Storage port;
    PlaybackSettings settings({&port, timing});
    boot(suite, settings, port);
    settings.observe(counted(3), 2, 2);
    tick(suite, settings, 250002);
    const auto request = required(port.active);
    port.finish();
    auto& c = port.completions.front();
    if (fault == 0)
      c.operation = SettingsOperation::Read;
    if (fault == 1)
      ++c.revision;
    if (fault == 2)
      c.slot = 2;
    if (fault == 3) {
      port.quiet = false;
      port.active = request;
    }
    if (fault == 4)
      c.readback.length = 65;
    tick(suite, settings, 250003);
    RPCMP_CHECK(suite, settings.snapshot().save == Save::Failed &&
                           !settings.snapshot().persisted_revision &&
                           settings.snapshot().error ==
                               (fault == 4 ? Error::ReadbackMismatch : Error::Protocol));
  }
  Storage busy;
  PlaybackSettings coalesced({&busy, {1000, 1000000, 100}});
  boot(suite, coalesced, busy);
  coalesced.observe(counted(3), 2, 2);
  tick(suite, coalesced, 250002);
  const auto captured = required(busy.active);
  for (std::uint64_t revision = 3; revision <= 10003; ++revision)
    coalesced.observe(counted(revision % 2 == 0 ? 3 : 5), revision, 250000 + revision);
  RPCMP_CHECK(suite, busy.requests.size() == 2 && required(busy.active).bytes == captured.bytes &&
                         coalesced.snapshot().policy_revision == 10003);
  busy.finish(false);
  tick(suite, coalesced, 260004);
  tick(suite, coalesced, 1000000);
  RPCMP_CHECK(suite, busy.requests.size() == 2 && coalesced.snapshot().save == Save::Failed);
  coalesced.observe(counted(4), 10004, 1000001);
  tick(suite, coalesced, 1250001);
  RPCMP_CHECK(suite, required(busy.active).operation == SettingsOperation::Read);
  busy.slots[1] = {SettingsSlotState::Unsupported};
  busy.finish();
  tick(suite, coalesced, 1250002);
  RPCMP_CHECK(suite, coalesced.snapshot().restore == Restore::Missing &&
                         coalesced.snapshot().error == Error::Unsupported &&
                         coalesced.policy() == counted(4));

  Storage last;
  PlaybackSettings last_id({&last, timing, maximum - 1});
  boot(suite, last_id, last);
  RPCMP_CHECK(suite, last.requests[0].id == maximum);
  last_id.observe(counted(3), 2, 2);
  tick(suite, last_id, 250002);
  RPCMP_CHECK(suite,
              last.requests.size() == 1 && last_id.snapshot().error == Error::RequestExhausted);
  Storage wall;
  PlaybackSettings near_max({&wall, timing});
  tick(suite, near_max, maximum - 2);
  wall.finish();
  tick(suite, near_max, maximum - 1);
  near_max.observe(counted(3), 2, maximum - 1);
  tick(suite, near_max, maximum);
  RPCMP_CHECK(suite, wall.requests.size() == 1 && near_max.snapshot().save == Save::Pending);

  auto invalid = coalesced.snapshot();
  invalid.persisted_revision = 0;
  RPCMP_CHECK(suite, !api::valid_settings_observation(invalid));
  invalid.persisted_revision = invalid.policy_revision + 1;
  RPCMP_CHECK(suite, !api::valid_settings_observation(invalid));
  invalid.persisted_revision.reset();
  invalid.save = Save::Saved;
  invalid.error.reset();
  RPCMP_CHECK(suite, !api::valid_settings_observation(invalid));
}

void change_at_deadline(rpcmp::test::Suite& suite) {
  Storage port;
  Preparation preparation;
  Audio audio;
  CatalogSession catalog;
  PlayerSession session(catalog, preparation, audio, {1'000'000, 1'000'000}, nullptr, nullptr,
                        {&port, timing});
  static_cast<void>(session.step(0));
  port.finish();
  static_cast<void>(session.step(1));
  api::PlayerCommand command;
  command.command_id = 1;
  command.kind = api::CommandKind::SetPlaybackPolicy;
  command.policy = counted(3);
  RPCMP_CHECK(suite, session.submit(command).outcome == api::CommandOutcome::Accepted);
  static_cast<void>(session.step(2));
  command.command_id = 2;
  command.policy = counted(5);
  RPCMP_CHECK(suite, session.submit(command).outcome == api::CommandOutcome::Accepted);
  static_cast<void>(session.step(250002));
  RPCMP_CHECK(suite, port.requests.size() == 1 &&
                         required(session.latest().settings).save == Save::Pending);
  static_cast<void>(session.step(500002));
  RPCMP_CHECK(suite, required(port.active).revision == 3);
  SettingsRecord saved;
  RPCMP_CHECK(suite, decode_settings({SettingsSlotState::Bytes, required(port.active).bytes, 64},
                                     saved) == SettingsDecode::Valid &&
                         saved.policy == counted(5));
  auto invalid = required(session.latest().settings);
  invalid.policy_revision = 999;
  SnapshotPublisher publisher(catalog);
  const auto state = session.step(500003, false).state;
  RPCMP_CHECK(suite, publisher.publish(1, state, &invalid) == PublicationResult::Ok);
  const auto public_state = publisher.latest();
  RPCMP_CHECK(suite, api::valid_player_snapshot(public_state) && !public_state.error &&
                         required(public_state.settings).error == Error::Protocol);
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  records(suite);
  restoration(suite);
  revisions_and_failures(suite);
  timeouts_and_bounds(suite);
  session_connection(suite);
  change_at_deadline(suite);
  completion_races(suite);
  static_assert(sizeof(PlaybackSettings) <= 1024);
  return suite.finish("Persistent playback settings");
}
