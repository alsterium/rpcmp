#include "rpcmp/platform/pocket/mdx_backend.hpp"
#include "rpcmp/player/player_session.hpp"
#include "rpcmp/utility/writer.hpp"
#include "test_support.hpp"

#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace pocket = rpcmp::platform::pocket;
namespace player = rpcmp::player;
namespace api = rpcmp::contracts::v2;
using rpcmp::contracts::TransportState;

struct Clock final : pocket::SoundClock {
  std::uint64_t now{};
  std::uint64_t now_us() noexcept override { return now; }
};

// Scripted copied mailboxes, not an RTL/audio simulator. Control effects and
// queue consumption are explicit stimuli; capture samples at its submission.
struct Sound final : pocket::IMmio32 {
  std::array<std::uint32_t, 256> words{};
  std::array<bool, 4> pending{}, hold{};
  std::vector<std::array<std::uint32_t, 12>> offers, accepted;
  std::vector<unsigned> controls;
  std::deque<player::MdxOutputRecord> records;
  std::optional<player::MdxOutputRecord> latest;
  player::MediaEnvelopeSnapshot media{};
  std::uint64_t epoch{}, prepared{}, output_prefix{}, next_sequence{1};
  unsigned queued{}, inhibits{}, violations{}, writes{}, reads{};
  bool inhibited{true}, fault{}, active{}, quiescent{true}, closed_feed{};
  Sound() {
    words[0] = 0x52534d31;
    words[1] = 0x10001;
    words[2] = 15;
    words[3] = 0x880;
    words[6] = 12288000;
    words[7] = 48000;
  }
  static unsigned shift(unsigned slot) { return slot == 3 ? 12 : slot * 2; }
  void put(std::uintptr_t offset, std::uint64_t value) {
    words[offset / 4] = static_cast<std::uint32_t>(value);
    words[offset / 4 + 1] = static_cast<std::uint32_t>(value >> 32U);
  }
  std::uint64_t get(std::uintptr_t offset) const {
    return words[offset / 4] | (static_cast<std::uint64_t>(words[offset / 4 + 1]) << 32U);
  }
  std::uint32_t read(std::uintptr_t address) noexcept override {
    ++reads;
    if (address < 0x40000400 || address >= 0x40000800 || address % 4 != 0) {
      ++violations;
      return 0;
    }
    return words[(address - 0x40000400) / 4];
  }
  void write(std::uintptr_t address, std::uint32_t value) noexcept override {
    ++writes;
    if (address < 0x40000400 || address >= 0x40000800 || address % 4 != 0) {
      ++violations;
      return;
    }
    const auto offset = address - 0x40000400;
    if (offset == 0x10) {
      ++inhibits;
      inhibited = true;
      active = false;
      return;
    }
    constexpr std::array<std::uintptr_t, 4> submit{0x44, 0xb0, 0x110, 0x31c};
    constexpr std::array<std::uintptr_t, 4> release{0x48, 0xb4, 0x114, 0x320};
    for (unsigned i = 0; i < 4; ++i) {
      if (offset == release[i]) {
        if ((words[3] & (2U << shift(i))) == 0)
          ++violations;
        words[3] &= ~(3U << shift(i));
        return;
      }
      if (offset == submit[i]) {
        if ((words[3] & (3U << shift(i))) != 0)
          ++violations;
        words[3] |= 1U << shift(i);
        pending[i] = true;
        switch (i) {
        case 0:
          control();
          break;
        case 1:
          feed();
          break;
        case 2:
          capture();
          break;
        case 3:
          journal();
          break;
        default:
          break;
        }
        return;
      }
    }
    words[offset / 4] = value;
  }
  void deliver() {
    for (unsigned i = 0; i < 4; ++i)
      if (pending[i] && !hold[i]) {
        words[3] |= 2U << shift(i);
        pending[i] = false;
      }
  }
  void control() {
    const auto kind = words[0x30 / 4];
    controls.push_back(kind);
    const auto generation = get(0x28);
    std::uint32_t result = 1;
    const auto frame = kind == 0 ? 0 : (kind == 1 ? 1 : media.frame);
    if (kind == 0) {
      ++epoch;
      prepared = generation;
      media = {};
      queued = 0;
      active = false;
      inhibited = false;
      fault = false;
      quiescent = true;
      records.clear();
      latest.reset();
      next_sequence = 1;
      output_prefix = 0;
    } else if (generation != prepared || inhibited || (kind == 1 && queued == 0)) {
      result = 4;
    } else {
      media.play_generation = generation;
      media.frame = frame;
      media.policy_revision = get(0x34);
      media.target = words[0x3c / 4] ? std::optional<std::uint32_t>{words[0x40 / 4]} : std::nullopt;
      if (kind != 4)
        media.paused = kind == 2;
      media.gain = 240000;
      active = true;
      quiescent = false;
    }
    put(0x180, get(0x20));
    put(0x188, generation);
    put(0x190, get(0x34));
    words[0x198 / 4] = kind;
    words[0x19c / 4] = words[0x3c / 4];
    words[0x1a0 / 4] = words[0x40 / 4];
    put(0x1a4, frame);
    words[0x1ac / 4] = result;
    put(0x1b0, epoch);
    put(0x1b8, prepared);
  }
  void feed() {
    std::array<std::uint32_t, 12> item{};
    for (std::size_t i = 0; i < item.size(); ++i)
      item[i] = words[0x80 / 4 + i];
    offers.push_back(item);
    std::uint32_t result = 1;
    if (get(0x80) != prepared || get(0x88) != epoch)
      result = 5;
    else if (inhibited || closed_feed || media.end != player::MediaEnd::None)
      result = 4;
    else if (queued == 64)
      result = 2;
    else {
      ++queued;
      accepted.push_back(item);
    }
    words[0xb8 / 4] = result;
    put(0xbc, get(0x80));
    put(0xc4, get(0x88));
  }
  void capture() {
    for (std::size_t i = 0x200 / 4; i <= 0x280 / 4; ++i)
      words[i] = 0;
    put(0x200, get(0x100));
    put(0x208, get(0x108));
    put(0x210, epoch);
    put(0x218, prepared);
    put(0x220, media.play_generation);
    put(0x228, media.frame);
    put(0x230, media.policy_revision);
    put(0x238, media.completed_loops);
    words[0x240 / 4] = media.target.value_or(0);
    words[0x244 / 4] = (media.target ? 1U : 0U) | (media.paused ? 2U : 0U) |
                       (quiescent ? 20U : 0U) | (active ? 32U : 0U) | (fault ? 64U : 0U) |
                       (inhibited ? 128U : 0U);
    words[0x248 / 4] = static_cast<unsigned>(media.phase) |
                       (static_cast<unsigned>(media.end) << 2U) |
                       (static_cast<unsigned>(media.failure) << 5U);
    words[0x24c / 4] = media.gain;
    words[0x250 / 4] = media.ramp_elapsed;
    words[0x254 / 4] = queued;
    put(0x258, output_prefix);
    words[0x268 / 4] =
        output_prefix && !media.paused && media.end == player::MediaEnd::None ? 3 : 0;
    words[0x280 / 4] = get(0x108) == epoch ? 1 : 3;
  }
  void record(std::uintptr_t offset, const player::MdxOutputRecord& value) {
    put(offset, value.sequence);
    put(offset + 8, value.generation);
    put(offset + 16, value.frame);
    put(offset + 24, value.prefix);
    words[(offset + 32) / 4] = value.natural_end ? 1 : 0;
  }
  void journal() {
    for (std::size_t i = 0x340 / 4; i <= 0x3b8 / 4; ++i)
      words[i] = 0;
    put(0x340, get(0x300));
    put(0x348, get(0x308));
    put(0x350, epoch);
    words[0x35c / 4] = static_cast<std::uint32_t>(records.size());
    put(0x368, next_sequence);
    words[0x370 / 4] = (records.empty() ? 0U : 1U) | (latest ? 2U : 0U);
    if (!records.empty())
      record(0x374, records.front());
    if (latest)
      record(0x398, *latest);
    std::uint32_t result = 1;
    if (get(0x308) != epoch)
      result = 4;
    else if (records.empty())
      result = 2;
    else if (words[0x318 / 4] != 0) {
      if (get(0x310) != records.front().sequence)
        result = 4;
      else
        records.pop_front();
    }
    words[0x358 / 4] = result;
  }
};

std::vector<std::uint8_t> authored_loop(unsigned extra_writes) {
  std::vector<std::uint8_t> track{0xff, 0xc8, 0xfe, 0x28, 0x3c, 0xfe, 0x30, 0, 0xfe, 8, 0x78};
  for (unsigned i = 0; i < extra_writes; ++i)
    track.insert(track.end(), {0xfe, 0x1b, static_cast<std::uint8_t>(i % 4)});
  track.insert(track.end(), {0, 0xf1}); // One rest tick, then loop to the first byte.
  const auto back = static_cast<std::uint16_t>(0U - (track.size() + 2U));
  track.push_back(static_cast<std::uint8_t>(back >> 8U));
  track.push_back(static_cast<std::uint8_t>(back));
  std::vector<std::uint8_t> mdx{'T', 0x0d, 0x0a, 0x1a, 0};
  const auto word = [&mdx](std::size_t value) {
    mdx.push_back(static_cast<std::uint8_t>(value >> 8U));
    mdx.push_back(static_cast<std::uint8_t>(value));
  };
  word(20 + track.size() + 16); // One zeroed voice after the nine tracks.
  word(20);
  for (std::size_t i = 0; i < 8; ++i)
    word(20 + track.size() + i * 2);
  mdx.insert(mdx.end(), track.begin(), track.end());
  for (unsigned i = 0; i < 8; ++i)
    mdx.insert(mdx.end(), {0xf1, 0});
  mdx.insert(mdx.end(), 27, 0);
  return mdx;
}

std::vector<std::uint8_t> library_bytes(bool finite, unsigned extra_writes) {
  std::ifstream input(std::string{RPCMP_SOURCE_DIR} + "/tests/fixtures/mdx/oracle-fm.mdx.hex");
  std::string pair;
  std::vector<std::uint8_t> mdx;
  char c{};
  while (input.get(c)) {
    if (c == '\r' || c == '\n' || c == ' ')
      continue;
    pair += c;
    if (pair.size() == 2) {
      mdx.push_back(static_cast<std::uint8_t>(std::stoul(pair, nullptr, 16)));
      pair.clear();
    }
  }
  if (!input.eof() || mdx.empty() || !pair.empty())
    throw std::logic_error("authored MDX fixture could not be read");
  if (!finite)
    mdx = authored_loop(extra_writes);
  rpcmp::utility::NormalizedLibrary library;
  library.blobs = {{rpcmp::library::kMdxFourcc, mdx}};
  rpcmp::utility::NormalizedTrack track;
  track.format = rpcmp::library::kMdxFourcc;
  track.title = "Authored FM";
  track.album = "Test";
  library.tracks.push_back(track);
  const auto written = rpcmp::utility::write_album_rpcmlib(library, {{".", "Test", {0}}});
  if (!written.ok())
    throw std::logic_error("authored library build failed: " +
                           std::to_string(static_cast<unsigned>(written.error)));
  return written.bytes;
}
struct Rig {
  explicit Rig(rpcmp::test::Suite& checks, bool finite = false, unsigned extra_writes = 0)
      : suite(checks), bytes(library_bytes(finite, extra_writes)), client(sound, clock),
        backend(std::make_unique<pocket::PocketMdxBackend>(catalog, client)),
        controller(catalog, *backend, *backend, {100000, 10000}) {
    const auto opened = catalog.begin_open();
    RPCMP_CHECK(suite, catalog.borrow_library(opened.generation) == nullptr);
    RPCMP_CHECK(suite, catalog.complete_open(opened.generation, {bytes.data(), bytes.size()}).ok());
    RPCMP_CHECK(suite, catalog.borrow_library(opened.generation) != nullptr &&
                           catalog.borrow_library({opened.generation.value + 1}) == nullptr);
    const auto albums = catalog.albums({1, opened.generation, 0, 16});
    const auto tracks = catalog.tracks(albums.items[0].album_id, {1, opened.generation, 0, 16});
    selection = {opened.generation, tracks.items[0].track_id};
    RPCMP_CHECK(suite, backend->initialize() == pocket::SoundSubmit::Accepted);
  }
  void step(const player::TransportBatch& batch = {}) {
    sound.deliver();
    clock.now += 10;
    static_cast<void>(controller.step(clock.now, batch));
  }
  void command(player::TransportIntentKind kind) {
    player::TransportBatch batch;
    batch.count = 1;
    batch.intents[0] = {
        ++command_id, kind,
        kind == player::TransportIntentKind::PlayTrack ? selection : player::PlaybackSelection{}};
    step(batch);
  }
  void boot() {
    for (unsigned i = 0; i < 8; ++i)
      step();
    RPCMP_CHECK(suite, controller.snapshot().transport == TransportState::Stopped);
  }
  void play() {
    boot();
    command(player::TransportIntentKind::PlayTrack);
    for (unsigned i = 0; i < 200; ++i)
      step();
    RPCMP_CHECK(suite, controller.snapshot().transport == TransportState::Playing &&
                           controller.snapshot().failure == player::TransportFailure::None);
  }
  void steps(unsigned count) {
    for (unsigned i = 0; i < count; ++i)
      step();
  }
  api::PerformanceHistorySnapshot history() const {
    api::PerformanceHistorySnapshot result;
    backend->copy_to(result);
    return result;
  }
  rpcmp::test::Suite& suite;
  std::vector<std::uint8_t> bytes;
  Clock clock;
  Sound sound;
  pocket::SoundMmioClient client;
  player::CatalogSession catalog;
  std::unique_ptr<pocket::PocketMdxBackend> backend;
  player::TransportController controller;
  player::PlaybackSelection selection;
  std::uint64_t command_id{};
};

void lifecycle(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  api::PerformanceHistorySnapshot history;
  const auto reads = rig.sound.reads;
  const auto writes = rig.sound.writes;
  rig.backend->copy_to(history);
  RPCMP_CHECK(suite, api::valid_performance_history(history) &&
                         history.availability == api::PerformanceAvailability::Waiting);
  RPCMP_CHECK(suite, rig.sound.reads == reads && rig.sound.writes == writes);
  rig.play();
  RPCMP_CHECK(suite, rig.sound.accepted.size() == 64 && rig.sound.offers.size() > 64);
  RPCMP_CHECK(suite, rig.sound.offers.at(64) == rig.sound.offers.back());
  const auto position = rig.controller.snapshot().media_frame;
  rig.command(player::TransportIntentKind::Pause);
  for (unsigned i = 0; i < 8; ++i)
    rig.step();
  RPCMP_CHECK(suite, rig.controller.snapshot().transport == TransportState::Paused &&
                         rig.controller.snapshot().media_frame == position);
  rig.command(player::TransportIntentKind::Resume);
  for (unsigned i = 0; i < 8; ++i)
    rig.step();
  RPCMP_CHECK(suite, rig.controller.snapshot().transport == TransportState::Playing &&
                         rig.controller.snapshot().failure == player::TransportFailure::None);
  rig.command(player::TransportIntentKind::Stop);
  for (unsigned i = 0; i < 8; ++i)
    rig.step();
  RPCMP_CHECK(suite, rig.controller.snapshot().transport == TransportState::Stopped &&
                         rig.controller.snapshot().silence_confirmed && rig.sound.queued == 0);
  RPCMP_CHECK(suite, rig.sound.violations == 0 && rig.sound.inhibits == 0);
}

void finite_and_partial_prefill(rpcmp::test::Suite& suite) {
  Rig finite(suite, true);
  finite.play();
  RPCMP_CHECK(suite, !finite.sound.accepted.empty() && finite.sound.accepted.size() < 64 &&
                         finite.sound.accepted == finite.sound.offers);
  // More than 64 raw writes in the first driver tick. Start must not wait for
  // its marker; the retained suffix streams after space becomes available.
  Rig partial(suite, false, 70);
  partial.play();
  for (const auto& item : partial.sound.accepted)
    RPCMP_CHECK(suite, item[10] == 0);
  partial.sound.queued = 0;
  partial.steps(50);
  RPCMP_CHECK(suite, partial.sound.accepted.size() > 70 &&
                         partial.controller.snapshot().failure == player::TransportFailure::None);
}

void old_captures(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.play();
  for (const auto kind : {player::TransportIntentKind::Pause, player::TransportIntentKind::Resume,
                          player::TransportIntentKind::SetPolicy}) {
    rig.sound.hold[2] = true; // A copied pre-command capture remains borrowed.
    player::TransportBatch batch;
    batch.count = 1;
    batch.intents[0] = {++rig.command_id, kind, {}};
    if (kind == player::TransportIntentKind::SetPolicy)
      batch.intents[0].policy =
          api::PlaybackPolicy{api::PlaybackOrder::AlbumOrder, api::RepeatMode::Counted, 3};
    rig.step(batch);
    rig.steps(4); // Control ACK precedes the obsolete capture response.
    RPCMP_CHECK(suite, !rig.controller.snapshot().audio_control &&
                           rig.controller.snapshot().failure == player::TransportFailure::None);
    rig.sound.hold[2] = false;
    rig.steps(4);
    const auto state = rig.controller.snapshot();
    RPCMP_CHECK(suite, state.failure == player::TransportFailure::None &&
                           state.transport == (kind == player::TransportIntentKind::Pause
                                                   ? TransportState::Paused
                                                   : TransportState::Playing));
    if (kind == player::TransportIntentKind::SetPolicy)
      RPCMP_CHECK(suite, state.policy.media && state.policy.media->applied.target == 3U);
  }
  RPCMP_CHECK(suite, rig.sound.inhibits == 0 && rig.sound.violations == 0);
}

void held_history_and_loss(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.play();
  std::uint64_t marker = 0;
  for (std::size_t i = 0; i < rig.sound.accepted.size(); ++i) {
    if (rig.sound.accepted[i][10] == 1) {
      marker = i + 1;
      break;
    }
  }
  if (!marker)
    throw std::logic_error("authored loop must have a prefetched marker");
  // Script a real-output boundary independently of the MDX source clock.
  const player::MdxOutputRecord record{rig.sound.prepared, rig.sound.epoch, 1, 100, marker};
  rig.sound.records.push_back(record);
  rig.sound.latest = record;
  rig.sound.next_sequence = 2;
  rig.sound.output_prefix = marker;
  rig.sound.media.frame = 101;
  rig.steps(8);
  const auto playing = rig.history();
  RPCMP_CHECK(suite, api::valid_performance_history(playing) &&
                         playing.availability == api::PerformanceAvailability::Available &&
                         playing.channels[0].key_on == true && playing.count != 0);
  for (std::uint16_t i = 0; i < playing.count; ++i)
    RPCMP_CHECK(suite, playing.events[i].change.at_frame == 100);
  rig.command(player::TransportIntentKind::Pause);
  rig.steps(10);
  const auto paused = rig.history();
  RPCMP_CHECK(suite, api::valid_performance_history(paused) &&
                         paused.availability == api::PerformanceAvailability::Available &&
                         paused.events == playing.events && paused.channels == playing.channels &&
                         paused.observed_through_frame == 101);
  rig.command(player::TransportIntentKind::Resume);
  rig.steps(8);
  rig.sound.hold[3] = true;
  rig.steps(110); // Only journal exceeds its 1 ms watchdog.
  RPCMP_CHECK(suite, rig.history().availability == api::PerformanceAvailability::Waiting &&
                         rig.controller.snapshot().failure == player::TransportFailure::None &&
                         rig.sound.inhibits == 0);
  rig.sound.hold[3] = false;
  rig.steps(8);
  RPCMP_CHECK(suite, rig.history().availability == api::PerformanceAvailability::Available &&
                         rig.history().events == playing.events && rig.sound.violations == 0);
}

void cancellation_and_old_epoch(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.boot();
  rig.command(player::TransportIntentKind::PlayTrack);
  rig.steps(10);
  RPCMP_CHECK(suite, rig.controller.snapshot().preparation.has_value());
  const auto previous_epoch = rig.sound.epoch;
  rig.sound.hold[1] = true;
  rig.sound.hold[2] = true;
  rig.command(player::TransportIntentKind::PlayTrack);
  rig.steps(8);
  RPCMP_CHECK(suite, rig.sound.epoch > previous_epoch &&
                         rig.client.busy(pocket::SoundChannel::Feed) &&
                         rig.controller.snapshot().failure == player::TransportFailure::None);
  rig.sound.hold[1] = false;
  rig.sound.hold[2] = false;
  rig.steps(200);
  RPCMP_CHECK(suite, rig.controller.snapshot().transport == TransportState::Playing &&
                         rig.controller.snapshot().failure == player::TransportFailure::None &&
                         rig.sound.violations == 0 && rig.sound.inhibits == 0);
}

void critical_timeout_recovery(rpcmp::test::Suite& suite) {
  for (const unsigned channel : {0U, 1U, 2U}) {
    Rig rig(suite);
    rig.play();
    rig.sound.hold[channel] = true;
    if (channel == 0)
      rig.command(player::TransportIntentKind::Pause);
    rig.steps(110);
    RPCMP_CHECK(suite, rig.controller.snapshot().failure != player::TransportFailure::None &&
                           rig.sound.inhibits != 0 &&
                           rig.history().availability == api::PerformanceAvailability::Waiting);
    // The late response drains ownership; it cannot turn the failed operation
    // into success. A subsequent explicit Reset recovers the transport.
    rig.sound.hold[channel] = false;
    rig.steps(10);
    RPCMP_CHECK(suite, rig.controller.snapshot().silence_confirmed &&
                           !rig.controller.snapshot().terminal && rig.sound.violations == 0);
    rig.command(player::TransportIntentKind::PlayTrack);
    rig.steps(200);
    RPCMP_CHECK(suite, rig.controller.snapshot().transport == TransportState::Playing &&
                           rig.controller.snapshot().failure == player::TransportFailure::None);
  }
}

void closed_feed(rpcmp::test::Suite& suite) {
  for (const bool natural : {false, true}) {
    Rig rig(suite);
    rig.play();
    rig.sound.hold[2] = true;
    rig.sound.closed_feed = true;
    if (natural) {
      rig.sound.media.end = player::MediaEnd::NaturalEnd;
      rig.sound.media.gain = 0;
      rig.sound.media.frame = 2000;
      rig.sound.active = false;
    }
    rig.steps(4); // Closed is visible while the old nonterminal capture is held.
    RPCMP_CHECK(suite, rig.controller.snapshot().failure == player::TransportFailure::None);
    rig.sound.hold[2] = false;
    rig.steps(8);
    if (natural) {
      RPCMP_CHECK(suite, rig.controller.snapshot().failure == player::TransportFailure::None &&
                             rig.controller.snapshot().transport == TransportState::Ended &&
                             rig.sound.inhibits == 0);
    } else {
      RPCMP_CHECK(suite,
                  rig.controller.snapshot().failure == player::TransportFailure::DeviceFault &&
                      rig.sound.inhibits != 0);
    }
    RPCMP_CHECK(suite, rig.sound.violations == 0);
  }
}

void publication_independence(rpcmp::test::Suite& suite) {
  const auto run = [&suite](bool publish, bool lose_journal) {
    Rig rig(suite);
    player::PlayerSession session(rig.catalog, *rig.backend, *rig.backend, {100000, 10000}, nullptr,
                                  rig.backend.get());
    const auto step = [&] {
      rig.sound.deliver();
      rig.clock.now += 10;
      return session.step(rig.clock.now, publish);
    };
    for (unsigned i = 0; i < 8; ++i)
      static_cast<void>(step());
    RPCMP_CHECK(suite, session.submit({2, 1, std::nullopt, api::CommandKind::PlayTrack,
                                       api::TrackSelection{rig.selection.library_generation,
                                                           rig.selection.track_id}})
                               .outcome == api::CommandOutcome::Accepted);
    for (unsigned i = 0; i < 500; ++i) {
      rig.sound.hold[3] = lose_journal && i < 400;
      if (i > 200 && i % 10 == 0)
        rig.sound.queued = 32;
      const auto result = step();
      RPCMP_CHECK(suite, result.state.failure == player::TransportFailure::None);
      if (publish) {
        const auto reads = rig.sound.reads;
        const auto writes = rig.sound.writes;
        RPCMP_CHECK(suite, api::valid_player_snapshot(session.latest()));
        static_cast<void>(rig.history());
        RPCMP_CHECK(suite, rig.sound.reads == reads && rig.sound.writes == writes);
      }
    }
    RPCMP_CHECK(suite, rig.sound.violations == 0 && rig.sound.inhibits == 0);
    return rig.sound.offers;
  };
  const auto absent = run(false, false);
  RPCMP_CHECK(suite,
              absent.size() > 300 && run(true, false) == absent && run(true, true) == absent);
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  try {
    lifecycle(suite);
    finite_and_partial_prefill(suite);
    old_captures(suite);
    held_history_and_loss(suite);
    cancellation_and_old_epoch(suite);
    critical_timeout_recovery(suite);
    closed_feed(suite);
    publication_independence(suite);
  } catch (const std::exception& error) {
    std::cerr << "Backend fixture failed: " << error.what() << '\n';
    return 1;
  }
  std::cout << "Pocket MDX backend bytes=" << sizeof(pocket::PocketMdxBackend) << '\n';
  return suite.finish("Pocket MDX backend");
}
