#include "rpcmp/player/player_session.hpp"
#include "test_support.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
using namespace rpcmp::player;
using rpcmp::contracts::TransportState;
namespace api = rpcmp::contracts::v2;
constexpr rpcmp::contracts::TrackId kTrack{0x47305869eca5f89aULL};

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
class Preparation final : public PreparationPort {
public:
  bool begin(const PreparationRequest& request) override {
    if (pending || ready)
      throw std::logic_error("overlapping preparation");
    pending = request;
    ++begins;
    return true;
  }
  void cancel(std::uint64_t id) override {
    if (required(pending).operation_id != id)
      throw std::logic_error("wrong cancellation");
    progress = PreparationProgress::Cancelled;
    ++cancels;
  }
  PreparationProgress poll(std::uint64_t id) override {
    if (required(pending).operation_id != id)
      throw std::logic_error("wrong preparation poll");
    ++polls;
    const auto result = progress;
    progress = PreparationProgress::Pending;
    if (result != PreparationProgress::Pending) {
      pending.reset();
      ready = result == PreparationProgress::Ready;
    }
    return result;
  }
  void release() override {
    if (!ready)
      throw std::logic_error("release without ownership");
    ready = false;
    ++releases;
  }
  std::optional<PreparationRequest> pending;
  PreparationProgress progress{PreparationProgress::Pending};
  bool ready{};
  unsigned begins{}, polls{}, cancels{}, releases{};
};
// This port runs the real envelope with an independently authored, zero-delay
// frame mapping. It proves the control connection, not a Pocket audio adapter.
class Audio final : public AudioTransportPort {
public:
  bool supports_pause() const noexcept override {
    ++reads;
    return true;
  }
  bool supports_policy() const noexcept override {
    ++reads;
    return supported;
  }
  bool begin(const AudioControlRequest& request) override {
    if (pending)
      throw std::logic_error("overlapping audio control");
    trace.push_back(request);
    if (reject_begin)
      return false;
    pending = request;
    return true;
  }
  AudioObservation observe() override {
    ++reads;
    const auto result = observation;
    observation.completion.reset();
    return result;
  }
  void emergency_silence() override { ++inhibits; }
  void copy_media() {
    observation.media = envelope.snapshot();
    observation.play_generation = observation.media->play_generation;
    observation.media_frame = observation.media->frame;
    observation.ended = observation.media->end != MediaEnd::None;
  }
  void ack() {
    const auto request = required(pending);
    pending.reset();
    std::uint64_t at = envelope.snapshot().frame;
    if (request.kind == AudioControlKind::Reset) {
      observation = {};
      at = 0;
    } else if (request.kind == AudioControlKind::Start) {
      const auto repeat = required(request.repeat);
      if (envelope.begin(request.play_generation, repeat.target, repeat.revision) !=
              MediaAdmission::Accepted ||
          envelope.enqueue({request.play_generation, 1, 0, 10, 0, false}) !=
              MediaAdmission::Accepted ||
          envelope.enqueue({request.play_generation, 2, 10, 1'000'000, loops, finite}) !=
              MediaAdmission::Accepted)
        throw std::logic_error("invalid authored media intervals");
      at = 0;
      copy_media();
    } else {
      const auto repeat = required(request.repeat);
      MediaBoundaryControl control{false, MediaControlAction::Keep,
                                   MediaRepeatUpdate{repeat.revision, repeat.target},
                                   request.play_generation};
      if (request.kind == AudioControlKind::Pause)
        control.action = MediaControlAction::Pause;
      else if (request.kind == AudioControlKind::Resume)
        control.action = MediaControlAction::Resume;
      const auto result = envelope.advance({12'000, -12'000}, control);
      if (result.control != MediaControlDisposition::Applied)
        throw std::logic_error("audio rejected admitted control");
      copy_media();
    }
    observation.completion = AudioControlCompletion{request, AudioControlOutcome::Success, at};
  }
  void frames(std::uint32_t count) {
    for (std::uint32_t i = 0; i < count; ++i)
      last = envelope.advance({12'000, -12'000});
    copy_media();
  }
  MediaLoopEnvelope envelope;
  std::optional<AudioControlRequest> pending;
  AudioObservation observation;
  MediaFrameResult last;
  std::vector<AudioControlRequest> trace;
  mutable unsigned reads{};
  unsigned inhibits{};
  std::uint64_t loops{2};
  bool supported{true}, finite{}, reject_begin{};
};
struct Rig {
  explicit Rig(rpcmp::test::Suite& checks) : suite(checks) {
    const auto ticket = catalog.begin_open();
    RPCMP_CHECK(suite, bytes.size() == 1360 && ticket.ok());
    RPCMP_CHECK(suite, catalog.complete_open(ticket.generation, {bytes.data(), bytes.size()}).ok());
  }
  api::PlayerCommand command(api::CommandKind kind) {
    api::PlayerCommand result;
    result.command_id = ++id;
    result.kind = kind;
    if (kind == api::CommandKind::PlayTrack)
      result.selection = api::TrackSelection{catalog.status().generation, kTrack};
    return result;
  }
  api::PlayerCommand policy(std::optional<std::uint32_t> count) {
    auto result = command(api::CommandKind::SetPlaybackPolicy);
    result.policy =
        api::PlaybackPolicy{api::PlaybackOrder::AlbumOrder,
                            count ? api::RepeatMode::Counted : api::RepeatMode::RepeatOne, count};
    return result;
  }
  void accept(const api::PlayerCommand& request) {
    RPCMP_CHECK(suite, session.submit(request).outcome == api::CommandOutcome::Accepted);
  }
  void tick(bool publish = true) {
    const auto result = session.step(now++, publish);
    RPCMP_CHECK(suite, publish ? result.published && result.publication == PublicationResult::Ok
                               : !result.published && !result.publication);
    RPCMP_CHECK(suite, api::valid_player_snapshot(session.latest()));
  }
  void boot() {
    tick();
    audio.ack();
    tick();
  }
  void starting() {
    boot();
    accept(command(api::CommandKind::PlayTrack));
    tick();
    audio.ack();
    tick();
    preparation.progress = PreparationProgress::Ready;
    tick();
  }
  void playing() {
    starting();
    audio.ack();
    tick();
  }
  api::PlaybackPolicyObservation state() const { return required(session.latest().policy); }
  api::PlaybackMediaObservation media() const { return required(state().media); }
  auto calls() const {
    return std::make_tuple(audio.reads, audio.inhibits, audio.trace.size(), preparation.begins,
                           preparation.polls, preparation.cancels, preparation.releases);
  }
  rpcmp::test::Suite& suite;
  std::vector<std::uint8_t> bytes{fixture()};
  CatalogSession catalog;
  Preparation preparation;
  Audio audio;
  PlayerSession session{catalog, preparation, audio, {100, 20}};
  std::uint64_t id{}, now{};
};

void asynchronous_revisions(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.starting();
  const auto generation = rig.session.latest().play_generation;
  const auto first = rig.policy(3);
  const auto before = rig.calls();
  rig.accept(first);
  for (unsigned i = 0; i < 5; ++i)
    static_cast<void>(rig.session.latest());
  RPCMP_CHECK(suite, rig.calls() == before && rig.state().revision == 1);
  rig.tick();
  RPCMP_CHECK(suite, rig.state().revision == 2 && rig.state().last_command_id == first.command_id &&
                         !rig.state().media &&
                         required(required(rig.audio.pending).repeat).revision == 1);
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, rig.media().applied.revision == 1 && rig.media().applied.target == 2U &&
                         required(rig.audio.pending).kind == AudioControlKind::SetPolicy);
  rig.accept(rig.policy(5));
  rig.tick();
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, rig.media().applied.revision == 2 && rig.media().applied.target == 3U &&
                         rig.state().revision == 3 &&
                         required(required(rig.audio.pending).repeat).revision == 3);
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, rig.media().applied.revision == 3 && rig.media().applied.target == 5U &&
                         rig.session.latest().play_generation == generation);
  const auto no_op = rig.policy(5);
  rig.accept(no_op);
  rig.tick();
  RPCMP_CHECK(suite, rig.state().revision == 3 && !rig.audio.pending);
  RPCMP_CHECK(suite, rig.session.submit(no_op).outcome == api::CommandOutcome::Duplicate);
  rig.accept(rig.command(api::CommandKind::Stop));
  rig.tick();
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, rig.state().desired.count == 5U && !rig.state().media &&
                         rig.session.latest().transport == TransportState::Stopped);
  static_cast<void>(rig.catalog.close());
  rig.tick();
  RPCMP_CHECK(suite, rig.state().desired.count == 5U && rig.state().revision == 3);
}

void fade_pause_restore(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.playing();
  rig.audio.frames(11);
  rig.tick();
  RPCMP_CHECK(suite, rig.media().completed_loops == 2U &&
                         rig.media().phase == api::PlaybackPhase::Fading &&
                         rig.media().ramp_elapsed == 1 && rig.media().gain == 239'999);
  rig.accept(rig.command(api::CommandKind::Pause));
  rig.accept(rig.policy(3));
  rig.tick();
  RPCMP_CHECK(suite, required(rig.audio.pending).kind == AudioControlKind::Pause &&
                         required(required(rig.audio.pending).repeat).target == 3U);
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, rig.media().position_frames == 11 && rig.media().applied.revision == 2 &&
                         rig.media().phase == api::PlaybackPhase::Fading);
  rig.audio.frames(1000);
  rig.tick();
  RPCMP_CHECK(suite, rig.media().position_frames == 11 && rig.media().gain == 239'999);
  rig.accept(rig.command(api::CommandKind::Resume));
  rig.tick();
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, rig.media().phase == api::PlaybackPhase::RestoringGain &&
                         rig.media().ramp_duration == 960 && rig.media().ramp_elapsed == 1);
  rig.audio.frames(960);
  rig.tick();
  RPCMP_CHECK(suite,
              rig.media().phase == api::PlaybackPhase::Steady && rig.media().gain == 240'000);
  rig.accept(rig.policy(1));
  rig.tick();
  const auto anchor = rig.media().position_frames;
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite,
              rig.media().phase == api::PlaybackPhase::Fading && rig.media().ramp_elapsed == 1);
  rig.audio.frames(239'999);
  rig.tick();
  RPCMP_CHECK(suite, rig.media().position_frames == anchor + 240'000 && rig.media().gain == 0 &&
                         rig.session.latest().transport == TransportState::Playing);
  rig.audio.frames(1);
  rig.tick();
  RPCMP_CHECK(suite, rig.media().end == api::PlaybackEndReason::LoopLimit &&
                         rig.session.latest().projected == TransportState::Ended);
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Ended &&
                         rig.media().position_frames == anchor + 240'000);
}

void finite_restart(rpcmp::test::Suite& suite) {
  for (const bool unlimited : {false, true}) {
    Rig rig(suite);
    rig.audio.finite = true;
    rig.starting();
    rig.accept(rig.policy(unlimited ? std::nullopt : std::optional<std::uint32_t>{5}));
    rig.tick();
    rig.audio.ack();
    rig.tick();
    rig.audio.ack();
    rig.tick();
    const auto generation = rig.session.latest().play_generation;
    rig.audio.frames(20);
    rig.tick();
    if (unlimited) {
      RPCMP_CHECK(suite, rig.session.latest().play_generation == generation + 1 &&
                             !rig.session.latest().pending_intent && !rig.state().media);
      rig.audio.ack();
      rig.tick();
      rig.preparation.progress = PreparationProgress::Ready;
      rig.tick();
      rig.audio.ack();
      rig.tick();
      RPCMP_CHECK(suite, rig.preparation.begins == 2 && rig.preparation.releases == 1 &&
                             rig.session.latest().transport == TransportState::Playing &&
                             rig.media().position_frames == 0 && !rig.media().applied.target);
    } else {
      rig.audio.ack();
      rig.tick();
      RPCMP_CHECK(suite, rig.preparation.begins == 1 &&
                             rig.session.latest().transport == TransportState::Ended &&
                             rig.media().position_frames == 10 &&
                             rig.media().end == api::PlaybackEndReason::NaturalEnd);
    }
  }
}

void admission_and_queue(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.boot();
  const auto reject = [&](api::PlayerCommand command, api::CommandReason reason) {
    const auto before = rig.calls();
    RPCMP_CHECK(suite, rig.session.submit(command).reason == reason && rig.calls() == before);
  };
  auto malformed = rig.policy(0);
  reject(malformed, api::CommandReason::MalformedRequest);
  malformed = rig.policy(3);
  required(malformed.policy).repeat = api::RepeatMode::Default;
  reject(malformed, api::CommandReason::MalformedRequest);
  malformed = rig.policy(3);
  malformed.selection = api::TrackSelection{{1}, kTrack};
  reject(malformed, api::CommandReason::MalformedRequest);
  malformed = rig.policy(3);
  malformed.kind = api::CommandKind::Stop;
  reject(malformed, api::CommandReason::MalformedRequest);
  malformed = rig.command(api::CommandKind::SetPlaybackPolicy);
  reject(malformed, api::CommandReason::MalformedRequest);
  auto unsupported = rig.policy(3);
  required(unsupported.policy).order = api::PlaybackOrder::ShuffleLibrary;
  reject(unsupported, api::CommandReason::UnsupportedCapability);
  for (std::uint32_t i = 0; i < 32; ++i)
    rig.accept(rig.policy(i + 1));
  reject(rig.policy(99), api::CommandReason::QueueFull);
  rig.tick();
  RPCMP_CHECK(suite,
              rig.state().revision == 33 && rig.state().desired.count == 32U && !rig.audio.pending);
  rig.accept(rig.policy(32));
  rig.tick();
  RPCMP_CHECK(suite, rig.state().revision == 33);
  rig.audio.supported = false;
  rig.tick();
  reject(rig.policy(3), api::CommandReason::UnsupportedCapability);
  RPCMP_CHECK(suite, (rig.session.latest().capabilities.bits & api::kRepeatControl) == 0 &&
                         rig.state().desired.count == 32U);
}

void failures(rpcmp::test::Suite& suite) {
  for (unsigned scenario = 0; scenario < 7; ++scenario) {
    Rig rig(suite);
    rig.playing();
    rig.accept(rig.policy(3));
    rig.tick();
    if (scenario == 0) {
      rig.audio.ack();
      required(required(rig.audio.observation.completion).request.repeat).target = 5;
    } else if (scenario == 1) {
      rig.audio.ack();
      required(rig.audio.observation.media).policy_revision = 1;
    } else if (scenario == 2) {
      rig.audio.observation.media.reset();
    } else if (scenario == 3) {
      rig.audio.supported = false;
    } else if (scenario == 4) {
      rig.now += 20;
    } else if (scenario == 5) {
      rig.audio.ack();
      required(rig.audio.observation.completion).outcome = AudioControlOutcome::Failed;
    } else {
      rig.audio.observation.play_generation = 1;
      rig.audio.observation.fault = true;
    }
    rig.tick();
    RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Error &&
                           rig.audio.inhibits != 0 && rig.state().desired.count == 3U &&
                           !rig.state().media);
    const auto error = required(rig.session.latest().error);
    const auto expected = scenario == 3 || scenario == 6 ? api::PlaybackErrorCode::DeviceFault
                          : scenario == 4                ? api::PlaybackErrorCode::AudioTimeout
                          : scenario == 5                ? api::PlaybackErrorCode::AudioControl
                                                         : api::PlaybackErrorCode::Protocol;
    RPCMP_CHECK(suite, error.code == expected);
    const auto result = rig.session.submit(rig.policy(5));
    RPCMP_CHECK(suite, error.terminal  ? result.reason == api::CommandReason::TerminalFailure
                       : scenario == 3 ? result.reason == api::CommandReason::UnsupportedCapability
                                       : result.outcome == api::CommandOutcome::Accepted);
  }
}

void counter_boundaries(rpcmp::test::Suite& suite) {
  CatalogSession catalog;
  Preparation preparation;
  Audio audio;
  TransportController controller{
      catalog, preparation, audio, {100, 20}, {0, 0, std::numeric_limits<std::uint64_t>::max()}};
  static_cast<void>(controller.step(0));
  audio.ack();
  static_cast<void>(controller.step(1));
  TransportBatch batch;
  batch.count = 1;
  batch.intents[0] = {1, TransportIntentKind::SetPolicy, {}, api::PlaybackPolicy{}};
  RPCMP_CHECK(suite, controller.step(2, batch).decisions[0].accepted());
  batch.intents[0].policy =
      api::PlaybackPolicy{api::PlaybackOrder::AlbumOrder, api::RepeatMode::Counted, 3};
  static_cast<void>(controller.step(3, batch));
  RPCMP_CHECK(suite, controller.snapshot().terminal &&
                         controller.snapshot().failure == TransportFailure::ResourceExhausted &&
                         controller.snapshot().policy.revision ==
                             std::numeric_limits<std::uint64_t>::max());
}

void held_end_and_stale_media(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.audio.finite = true;
  rig.playing();
  rig.accept(rig.policy(std::nullopt));
  rig.tick();
  rig.audio.ack();
  rig.tick();
  rig.accept(rig.command(api::CommandKind::Pause));
  rig.tick();
  // The source ends after the Pause request, before its boundary ACK.
  rig.audio.frames(11);
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Paused &&
                         rig.media().end == api::PlaybackEndReason::RepeatOne &&
                         rig.preparation.begins == 1);
  rig.accept(rig.policy(5));
  rig.tick();
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Paused &&
                         rig.media().position_frames == 10 && rig.media().applied.target == 5U);
  rig.accept(rig.command(api::CommandKind::Resume));
  rig.tick();
  rig.audio.ack();
  rig.tick();
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Ended &&
                         rig.preparation.begins == 1);

  Rig stale(suite);
  stale.playing();
  const auto old = stale.audio.observation;
  stale.accept(stale.command(api::CommandKind::PlayTrack));
  stale.tick();
  stale.audio.ack();
  stale.tick();
  stale.preparation.progress = PreparationProgress::Ready;
  stale.tick();
  stale.audio.ack();
  stale.tick();
  const auto current = stale.audio.observation;
  stale.audio.observation = old;
  stale.audio.observation.ended = true;
  stale.tick();
  RPCMP_CHECK(suite, stale.session.latest().transport == TransportState::Playing &&
                         stale.media().play_generation == current.play_generation &&
                         stale.media().position_frames == 0);
  stale.audio.observation = current;
  stale.audio.frames(10);
  stale.tick();
  stale.accept(stale.command(api::CommandKind::Pause));
  stale.tick();
  stale.audio.ack();
  stale.tick();
  stale.accept(stale.policy(5));
  stale.tick();
  stale.audio.ack();
  ++required(stale.audio.observation.completion).media_frame;
  stale.tick();
  RPCMP_CHECK(suite,
              required(stale.session.latest().error).code == api::PlaybackErrorCode::Protocol);
}

void additional_boundaries(rpcmp::test::Suite& suite) {
  Rig simultaneous(suite);
  simultaneous.playing();
  simultaneous.audio.supported = false;
  static_cast<void>(simultaneous.catalog.close());
  simultaneous.tick();
  const auto simultaneous_error = simultaneous.session.latest().error;
  RPCMP_CHECK(suite, simultaneous_error &&
                         simultaneous_error->code == api::PlaybackErrorCode::DeviceFault);

  Rig overflow(suite);
  overflow.audio.loops = static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1;
  overflow.playing();
  overflow.audio.frames(11);
  overflow.tick();
  RPCMP_CHECK(suite, overflow.media().loop_count_overflow && !overflow.media().completed_loops);

  Rig rejection(suite);
  rejection.playing();
  rejection.audio.reject_begin = true;
  rejection.accept(rejection.policy(5));
  rejection.tick();
  RPCMP_CHECK(suite, required(rejection.session.latest().error).code ==
                             api::PlaybackErrorCode::AudioControl &&
                         rejection.audio.inhibits != 0);

  Rig changed(suite);
  changed.boot();
  changed.accept(changed.policy(5));
  changed.audio.supported = false;
  changed.tick();
  RPCMP_CHECK(suite,
              changed.state().revision == 1 && required(changed.session.latest().error).code ==
                                                   api::PlaybackErrorCode::DeviceFault);

  Rig late(suite);
  late.playing();
  late.accept(late.policy(5));
  late.tick();
  const auto expected = required(late.audio.pending);
  late.audio.observation.completion =
      AudioControlCompletion{expected, AudioControlOutcome::Success, 0};
  --required(late.audio.observation.completion).request.operation_id;
  late.tick();
  RPCMP_CHECK(suite, late.media().applied.revision == 1 && late.audio.pending &&
                         !late.session.latest().error);
  late.now += 20;
  late.tick();
  late.audio.ack();
  late.tick();
  RPCMP_CHECK(suite,
              required(late.session.latest().error).code == api::PlaybackErrorCode::AudioTimeout &&
                  required(late.session.latest().error).terminal && !late.state().media);
}

std::vector<std::tuple<AudioControlKind, std::uint64_t, std::uint64_t>> trace(Rig& rig,
                                                                              bool publish) {
  rig.playing();
  rig.audio.frames(500);
  rig.tick(publish);
  rig.accept(rig.policy(std::nullopt));
  rig.tick(publish);
  rig.audio.ack();
  rig.tick(publish);
  rig.audio.frames(1000);
  rig.tick(publish);
  rig.tick();
  RPCMP_CHECK(rig.suite, rig.media().position_frames == 1501 && rig.media().gain == 240'000);
  std::vector<std::tuple<AudioControlKind, std::uint64_t, std::uint64_t>> result;
  result.reserve(rig.audio.trace.size());
  for (const auto& request : rig.audio.trace)
    result.emplace_back(request.kind, request.play_generation,
                        request.repeat ? request.repeat->revision : 0);
  return result;
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  asynchronous_revisions(suite);
  fade_pause_restore(suite);
  finite_restart(suite);
  admission_and_queue(suite);
  failures(suite);
  counter_boundaries(suite);
  held_end_and_stale_media(suite);
  additional_boundaries(suite);
  Rig frequent(suite), sparse(suite);
  RPCMP_CHECK(suite, trace(frequent, true) == trace(sparse, false));
  return suite.finish("Player repeat policy connection");
}
