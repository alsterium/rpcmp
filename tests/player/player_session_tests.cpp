#include "rpcmp/player/player_session.hpp"
#include "test_support.hpp"

#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {
using namespace rpcmp::player;
using rpcmp::contracts::TrackId;
using rpcmp::contracts::TransportState;
namespace api = rpcmp::contracts::v2;
constexpr TrackId kB2{0x47305869eca5f89aULL};
constexpr TrackId kB1{0x1379269c029ea5fcULL};

template <typename T> T required(const std::optional<T>& value) {
  if (!value)
    throw std::logic_error("missing scripted observation");
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
      throw std::logic_error("preparation ownership overlap");
    pending = request;
    ++begins;
    return true;
  }
  void cancel(std::uint64_t id) override {
    if (required(pending).operation_id != id)
      throw std::logic_error("wrong cancellation");
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
      throw std::logic_error("release without prepared storage");
    ready = false;
    ++releases;
  }
  std::optional<PreparationRequest> pending;
  PreparationProgress progress{PreparationProgress::Pending};
  bool ready{};
  unsigned begins{}, polls{}, cancels{}, releases{};
};
class Audio final : public AudioTransportPort {
public:
  bool supports_pause() const noexcept override {
    ++capability_reads;
    return pause_supported;
  }
  bool begin(const AudioControlRequest& request) override {
    if (pending)
      throw std::logic_error("overlapping audio control");
    pending = request;
    trace.emplace_back(request.kind, request.play_generation);
    return true;
  }
  AudioObservation observe() override {
    ++polls;
    const auto result = observation;
    observation.completion.reset();
    return result;
  }
  void emergency_silence() override { ++inhibits; }
  void ack(const std::uint64_t frame = 0) {
    const auto request = required(pending);
    pending.reset();
    observation.completion = AudioControlCompletion{request, AudioControlOutcome::Success, frame};
    observation.media_frame = frame;
    observation.play_generation =
        request.kind == AudioControlKind::Reset ? 0 : request.play_generation;
    if (request.kind == AudioControlKind::Reset)
      observation.ended = false;
  }
  std::optional<AudioControlRequest> pending;
  AudioObservation observation;
  std::vector<std::pair<AudioControlKind, std::uint64_t>> trace;
  mutable unsigned capability_reads{};
  unsigned polls{}, inhibits{};
  bool pause_supported{true};
};
struct Rig {
  explicit Rig(rpcmp::test::Suite& checks) : suite(checks) {
    const auto ticket = catalog.begin_open();
    RPCMP_CHECK(suite, bytes.size() == 1360 && ticket.ok());
    RPCMP_CHECK(suite, catalog.complete_open(ticket.generation, {bytes.data(), bytes.size()}).ok());
  }
  api::PlayerCommand command(api::CommandKind kind, TrackId track = kB2) {
    api::PlayerCommand command;
    command.command_id = ++id;
    command.kind = kind;
    if (kind == api::CommandKind::PlayTrack || kind == api::CommandKind::LoadTrack)
      command.selection = api::TrackSelection{catalog.status().generation, track};
    return command;
  }
  api::CommandResult accept(api::CommandKind kind, TrackId track = kB2) {
    const auto result = session.submit(command(kind, track));
    RPCMP_CHECK(suite, result.outcome == api::CommandOutcome::Accepted &&
                           result.reason == api::CommandReason::None);
    return result;
  }
  PlayerStepResult tick(bool publish = true) {
    const auto result = session.step(now++, publish);
    RPCMP_CHECK(suite, publish ? result.publication == PublicationResult::Ok && result.published
                               : !result.publication && !result.published);
    RPCMP_CHECK(suite, api::valid_player_snapshot(session.latest()));
    return result;
  }
  void boot() {
    static_cast<void>(tick());
    audio.ack();
    static_cast<void>(tick());
  }
  void finish_selection() {
    audio.ack();
    static_cast<void>(tick());
    preparation.progress = PreparationProgress::Ready;
    static_cast<void>(tick());
    audio.ack();
    static_cast<void>(tick());
  }
  void playing() {
    boot();
    static_cast<void>(accept(api::CommandKind::PlayTrack));
    static_cast<void>(tick());
    finish_selection();
  }
  auto calls() const {
    return std::make_tuple(preparation.begins, preparation.polls, preparation.cancels,
                           preparation.releases, audio.trace.size(), audio.polls, audio.inhibits,
                           audio.capability_reads);
  }
  rpcmp::test::Suite& suite;
  std::vector<std::uint8_t> bytes{fixture()};
  CatalogSession catalog;
  Preparation preparation;
  Audio audio;
  PlayerSession session{catalog, preparation, audio, {100, 20}};
  std::uint64_t id{}, now{};
};

void replay_and_identity(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  const auto initial_calls = rig.calls();
  RPCMP_CHECK(suite, (rig.session.latest().capabilities.bits & api::kTransportCommands) != 0);
  RPCMP_CHECK(suite, rig.session.submit(rig.command(api::CommandKind::PlayTrack)).reason ==
                         api::CommandReason::ResourceBusy);
  RPCMP_CHECK(suite, rig.calls() == initial_calls);
  rig.playing();
  const auto observed = rig.session.latest();
  const auto calls = rig.calls();
  auto unsupported = rig.command(api::CommandKind::Pause);
  unsupported.schema_version = 1;
  RPCMP_CHECK(suite,
              rig.session.submit(unsupported).reason == api::CommandReason::UnsupportedSchema);
  unsupported.schema_version = 2;
  unsupported.expected_snapshot_sequence = observed.sequence;
  const auto accepted = rig.session.submit(unsupported);
  RPCMP_CHECK(suite, accepted.outcome == api::CommandOutcome::Accepted);
  unsupported.kind = api::CommandKind::Stop;
  const auto duplicate = rig.session.submit(unsupported);
  RPCMP_CHECK(suite,
              duplicate.outcome == api::CommandOutcome::Duplicate &&
                  required(duplicate.original).outcome == api::CommandOutcome::Accepted &&
                  required(duplicate.original).observed_snapshot_sequence == observed.sequence);
  RPCMP_CHECK(suite, rig.calls() == calls && rig.session.latest().sequence == observed.sequence);
  static_cast<void>(rig.tick());
  RPCMP_CHECK(suite, required(rig.audio.pending).kind == AudioControlKind::Pause);
  rig.audio.ack();
  static_cast<void>(rig.tick());

  auto stale = rig.command(api::CommandKind::Resume);
  stale.expected_snapshot_sequence = observed.sequence;
  RPCMP_CHECK(suite, rig.session.submit(stale).reason == api::CommandReason::StaleSnapshotSequence);
  stale.expected_snapshot_sequence.reset();
  const auto rejected_replay = rig.session.submit(stale);
  RPCMP_CHECK(suite,
              rejected_replay.outcome == api::CommandOutcome::Duplicate &&
                  required(rejected_replay.original).outcome == api::CommandOutcome::Rejected &&
                  required(rejected_replay.original).reason ==
                      api::CommandReason::StaleSnapshotSequence);
  const auto invalid_calls = rig.calls();
  auto unknown = rig.command(api::CommandKind::PlayTrack, {999});
  RPCMP_CHECK(suite, rig.session.submit(unknown).reason == api::CommandReason::UnknownTrack);
  unknown = rig.command(api::CommandKind::PlayTrack);
  unknown.selection = api::TrackSelection{{2}, kB2};
  RPCMP_CHECK(suite, rig.session.submit(unknown).reason == api::CommandReason::StaleLibrary);
  auto malformed = rig.command(api::CommandKind::Stop);
  malformed.selection = api::TrackSelection{{1}, kB2};
  RPCMP_CHECK(suite, rig.session.submit(malformed).reason == api::CommandReason::MalformedRequest);
  malformed = rig.command(api::CommandKind::PlayTrack);
  malformed.selection.reset();
  RPCMP_CHECK(suite, rig.session.submit(malformed).reason == api::CommandReason::MalformedRequest);
  malformed = rig.command(api::CommandKind::PlayTrack);
  malformed.selection = api::TrackSelection{{0}, kB2};
  RPCMP_CHECK(suite, rig.session.submit(malformed).reason == api::CommandReason::MalformedRequest);
  malformed = rig.command(api::CommandKind::Stop);
  const std::uint8_t raw = 255;
  static_assert(sizeof(malformed.kind) == 1);
  std::memcpy(&malformed.kind, &raw, 1);
  RPCMP_CHECK(suite, rig.session.submit(malformed).reason == api::CommandReason::MalformedRequest);
  malformed.command_id = ++rig.id;
  malformed.expected_snapshot_sequence = observed.sequence;
  RPCMP_CHECK(suite,
              rig.session.submit(malformed).reason == api::CommandReason::StaleSnapshotSequence);
  malformed.command_id = 0;
  RPCMP_CHECK(suite, rig.session.submit(malformed).reason == api::CommandReason::InvalidCommandId);
  RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Paused &&
                         rig.session.latest().play_generation == observed.play_generation);
  RPCMP_CHECK(suite, rig.calls() == invalid_calls);
}

void queue_and_history(rpcmp::test::Suite& suite) {
  {
    Rig rig(suite);
    rig.playing();
    const auto before = rig.calls();
    const auto generation = rig.session.latest().play_generation;
    static_cast<void>(rig.accept(api::CommandKind::Pause));
    static_cast<void>(rig.accept(api::CommandKind::Resume));
    for (unsigned i = 2; i < 32; ++i)
      static_cast<void>(rig.accept(api::CommandKind::Play));
    RPCMP_CHECK(suite, rig.session.submit(rig.command(api::CommandKind::Pause)).reason ==
                           api::CommandReason::QueueFull);
    RPCMP_CHECK(suite, rig.session.submit(rig.command(api::CommandKind::Resume)).reason ==
                           api::CommandReason::InvalidState);
    RPCMP_CHECK(suite, rig.calls() == before);
    const auto starts = rig.audio.trace.size();
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.audio.trace.size() == starts &&
                           rig.session.latest().transport == TransportState::Playing &&
                           rig.session.latest().play_generation == generation);
    static_cast<void>(rig.accept(api::CommandKind::Pause));
    static_cast<void>(rig.tick());
    static_cast<void>(
        rig.accept(api::CommandKind::Resume)); // Resume while Pause ACK is outstanding.
    rig.audio.ack(40);
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, required(rig.audio.pending).kind == AudioControlKind::Resume &&
                           rig.session.latest().transport == TransportState::Paused);
    rig.audio.ack(40);
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Playing &&
                           rig.session.latest().position_frames == 40);
  }
  {
    Rig rig(suite);
    rig.boot();
    const auto first_sequence = rig.session.latest().sequence;
    for (unsigned i = 0; i < 64; ++i) {
      static_cast<void>(rig.accept(api::CommandKind::Stop));
      static_cast<void>(rig.tick());
    }
    api::PlayerCommand first{2, 1, std::nullopt, api::CommandKind::Stop, std::nullopt};
    const auto retained = rig.session.submit(first);
    RPCMP_CHECK(suite,
                retained.outcome == api::CommandOutcome::Duplicate &&
                    required(retained.original).observed_snapshot_sequence == first_sequence);
    static_cast<void>(rig.accept(api::CommandKind::Stop));
    RPCMP_CHECK(suite, rig.session.submit(first).reason == api::CommandReason::StaleCommandId);
    first.command_id = 2;
    RPCMP_CHECK(suite, rig.session.submit(first).outcome == api::CommandOutcome::Duplicate);
    static_cast<void>(rig.tick());
    first.command_id = std::numeric_limits<std::uint64_t>::max();
    RPCMP_CHECK(suite, rig.session.submit(first).outcome == api::CommandOutcome::Accepted);
    RPCMP_CHECK(suite, rig.session.submit(first).outcome == api::CommandOutcome::Duplicate);
    --first.command_id;
    RPCMP_CHECK(suite, rig.session.submit(first).reason == api::CommandReason::StaleCommandId);
  }
}

void cancellation_and_faults(rpcmp::test::Suite& suite) {
  {
    Rig rig(suite);
    rig.playing();
    const auto opening = rig.catalog.begin_open();
    RPCMP_CHECK(suite, opening.ok());
    RPCMP_CHECK(
        suite,
        rig.catalog.complete_open(opening.generation, {rig.bytes.data(), rig.bytes.size()}).ok());
    const auto before = rig.calls();
    RPCMP_CHECK(suite, rig.session.submit(rig.command(api::CommandKind::PlayTrack)).reason ==
                           api::CommandReason::ResourceBusy);
    RPCMP_CHECK(suite, rig.calls() == before);
    static_cast<void>(rig.tick());
    rig.audio.ack();
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Stopped &&
                           !rig.session.latest().track);
    static_cast<void>(rig.accept(api::CommandKind::PlayTrack));
    static_cast<void>(rig.tick());
    rig.finish_selection();
    RPCMP_CHECK(suite, rig.session.latest().library.generation == opening.generation &&
                           rig.session.latest().transport == TransportState::Playing);
  }
  {
    Rig rig(suite);
    rig.boot();
    const auto generation = rig.session.latest().play_generation;
    static_cast<void>(rig.accept(api::CommandKind::PlayTrack, kB2));
    static_cast<void>(rig.accept(api::CommandKind::PlayTrack, kB1));
    static_cast<void>(rig.accept(api::CommandKind::Stop));
    RPCMP_CHECK(suite, !rig.session.latest().track);
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.preparation.begins == 0 && rig.audio.trace.size() == 2 &&
                           rig.session.latest().play_generation == generation + 3);
    rig.audio.ack();
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Stopped &&
                           required(rig.session.latest().track).item.track_id == kB1);
  }
  {
    Rig rig(suite);
    rig.playing();
    const auto pause = rig.accept(api::CommandKind::Pause);
    RPCMP_CHECK(suite, rig.catalog.close().ok());
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite,
                required(rig.session.latest().error).code == api::PlaybackErrorCode::Library &&
                    required(rig.audio.pending).kind == AudioControlKind::Reset);
    api::PlayerCommand repeated{2, pause.command_id, std::nullopt, api::CommandKind::Play,
                                std::nullopt};
    RPCMP_CHECK(suite, required(rig.session.submit(repeated).original).outcome ==
                           api::CommandOutcome::Accepted);
    rig.audio.ack();
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, !rig.session.latest().track &&
                           rig.session.latest().transport == TransportState::Error);
  }
  {
    Rig rig(suite);
    rig.playing();
    static_cast<void>(rig.accept(api::CommandKind::Pause));
    rig.audio.observation.fault = true;
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite,
                required(rig.session.latest().error).code == api::PlaybackErrorCode::DeviceFault &&
                    required(rig.audio.pending).kind == AudioControlKind::Reset);
    rig.audio.observation.fault = false;
    rig.audio.ack();
    static_cast<void>(rig.tick());
    static_cast<void>(rig.accept(api::CommandKind::PlayTrack, kB1));
    static_cast<void>(rig.tick());
    rig.finish_selection();
    RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Playing &&
                           !rig.session.latest().error);
  }
  {
    Rig rig(suite);
    rig.playing();
    static_cast<void>(rig.accept(api::CommandKind::Pause));
    rig.audio.pause_supported = false;
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite,
                required(rig.session.latest().error).code == api::PlaybackErrorCode::DeviceFault &&
                    required(rig.audio.pending).kind == AudioControlKind::Reset);
  }
}

void aliases_end_and_clock(rpcmp::test::Suite& suite) {
  {
    Rig rig(suite);
    rig.playing();
    static_cast<void>(rig.accept(api::CommandKind::Pause));
    static_cast<void>(rig.tick());
    rig.audio.ack();
    static_cast<void>(rig.tick());
    rig.audio.pause_supported = false;
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.session.submit(rig.command(api::CommandKind::Play)).reason ==
                           api::CommandReason::UnsupportedCapability);
    RPCMP_CHECK(suite, rig.session.submit(rig.command(api::CommandKind::Resume)).reason ==
                           api::CommandReason::UnsupportedCapability);
  }
  {
    Rig rig(suite);
    rig.playing();
    rig.audio.observation.media_frame = 480;
    rig.audio.observation.ended = true;
    static_cast<void>(rig.accept(api::CommandKind::Stop));
    static_cast<void>(rig.tick());
    rig.audio.ack();
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.session.latest().transport == TransportState::Stopped &&
                           rig.session.latest().position_frames == 0);
    const auto before = rig.session.latest();
    const auto backwards = rig.session.step(0);
    RPCMP_CHECK(suite,
                backwards.publication == PublicationResult::Ok &&
                    required(rig.session.latest().error).code == api::PlaybackErrorCode::Clock &&
                    required(rig.session.latest().error).terminal && rig.audio.inhibits > 0);
    RPCMP_CHECK(suite, rig.session.latest().published_at_us == before.published_at_us &&
                           rig.session.latest().sequence == before.sequence + 1);
    RPCMP_CHECK(suite, rig.session.submit(rig.command(api::CommandKind::Stop)).reason ==
                           api::CommandReason::TerminalFailure);
  }
}

auto independent_trace(rpcmp::test::Suite& suite, bool frequent_publication) {
  Rig rig(suite);
  rig.playing();
  std::vector<std::uint64_t> positions;
  for (std::uint64_t i = 0; i < 120; ++i) {
    rig.audio.observation.media_frame = 800 * i;
    const auto state = rig.tick(frequent_publication || i % 17 == 0).state;
    for (unsigned read = 0; read < (frequent_publication ? 7U : 0U); ++read)
      static_cast<void>(rig.session.latest());
    positions.push_back(state.media_frame);
    RPCMP_CHECK(suite, state.media_frame == 800 * i);
  }
  return std::make_tuple(positions, rig.calls(), rig.audio.trace);
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  replay_and_identity(suite);
  queue_and_history(suite);
  cancellation_and_faults(suite);
  aliases_end_and_clock(suite);
  RPCMP_CHECK(suite, independent_trace(suite, false) == independent_trace(suite, true));
  return suite.finish("Player command session");
}
