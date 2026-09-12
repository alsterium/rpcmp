#include "rpcmp/player/playback_transport.hpp"
#include "test_support.hpp"

#include <cstring>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace rpcmp::player;
using rpcmp::contracts::TrackId;
using rpcmp::contracts::TransportState;
constexpr TrackId kB2{0x47305869eca5f89aULL};
constexpr TrackId kB1{0x1379269c029ea5fcULL};

template <typename T> T required(const std::optional<T>& value) {
  if (!value)
    throw std::logic_error("required transport observation is absent");
  return *value;
}

std::vector<std::uint8_t> fixture() {
  std::ifstream input(std::string{RPCMP_SOURCE_DIR} + "/tests/fixtures/rpcmlib/album.rpcmlib.hex");
  std::string pair;
  std::vector<std::uint8_t> bytes;
  char c = 0;
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

class ScriptedPreparation final : public PreparationPort {
public:
  explicit ScriptedPreparation(std::vector<std::string>& trace) : trace_(trace) {}
  bool begin(const PreparationRequest& request) override {
    if (pending || ready || (sound_using && *sound_using))
      ++violations;
    trace_.push_back("Prepare");
    if (reject_begin) {
      reject_begin = false;
      return false;
    }
    pending = request;
    return true;
  }
  void cancel(const std::uint64_t operation_id) override {
    if (!pending || pending->operation_id != operation_id)
      ++violations;
    ++cancels;
    trace_.push_back("Cancel");
  }
  PreparationProgress poll(const std::uint64_t operation_id) override {
    if (!pending || pending->operation_id != operation_id)
      ++violations;
    const auto value = progress;
    progress = PreparationProgress::Pending;
    if (value == PreparationProgress::Ready || value == PreparationProgress::Failed ||
        value == PreparationProgress::Cancelled) {
      pending.reset();
      ready = value == PreparationProgress::Ready;
    }
    return value;
  }
  void release() override {
    if (!ready || pending || (sound_using && *sound_using))
      ++violations;
    ready = false;
    ++releases;
    trace_.push_back("Release");
  }
  std::optional<PreparationRequest> pending;
  PreparationProgress progress{PreparationProgress::Pending};
  const bool* sound_using{};
  bool ready{};
  bool reject_begin{};
  unsigned violations{};
  unsigned cancels{};
  unsigned releases{};

private:
  std::vector<std::string>& trace_;
};

class ScriptedAudio final : public AudioTransportPort {
public:
  ScriptedAudio(std::vector<std::string>& trace, const ScriptedPreparation& preparation)
      : trace_(trace), preparation_(preparation) {}
  bool supports_pause() const noexcept override { return pause_supported; }
  bool begin(const AudioControlRequest& request) override {
    if (pending)
      ++violations;
    const char* name = "Reset";
    switch (request.kind) {
    case AudioControlKind::Reset:
      break;
    case AudioControlKind::Start:
      name = "Start";
      break;
    case AudioControlKind::Pause:
      name = "Pause";
      break;
    case AudioControlKind::Resume:
      name = "Resume";
      break;
    }
    trace_.push_back(name);
    if (reject_begin) {
      reject_begin = false;
      return false;
    }
    if (request.kind == AudioControlKind::Start) {
      if (!preparation_.ready)
        ++violations;
      sound_using = true;
    }
    pending = request;
    return true;
  }
  AudioObservation observe() override {
    ++observations;
    const auto result = observation;
    observation.completion.reset();
    return result;
  }
  void emergency_silence() override {
    inhibited = true;
    ++inhibits;
  }
  void complete(const AudioControlOutcome outcome = AudioControlOutcome::Success,
                const std::uint64_t frame = 0) {
    if (!pending) {
      ++violations;
      return;
    }
    observation.completion = AudioControlCompletion{*pending, outcome, frame};
    if (outcome == AudioControlOutcome::Success) {
      if (pending->kind == AudioControlKind::Reset) {
        sound_using = false;
        inhibited = false;
        observation.play_generation = 0;
        observation.media_frame = 0;
        observation.ended = false;
      } else {
        observation.play_generation = pending->play_generation;
        observation.media_frame = frame;
      }
    }
    pending.reset();
  }
  AudioObservation observation{};
  std::optional<AudioControlRequest> pending;
  bool pause_supported{true};
  bool sound_using{};
  bool inhibited{};
  bool reject_begin{};
  unsigned violations{};
  unsigned observations{};
  unsigned inhibits{};

private:
  std::vector<std::string>& trace_;
  const ScriptedPreparation& preparation_;
};

class Rig {
public:
  explicit Rig(rpcmp::test::Suite& suite, const TransportTiming timing = {100, 20},
               const TransportCounters counters = {})
      : bytes(fixture()), preparation(trace), audio(trace, preparation),
        controller(catalog, preparation, audio, timing, counters), suite_(suite) {
    preparation.sound_using = &audio.sound_using;
    const auto load = catalog.begin_open();
    RPCMP_CHECK(suite_, catalog.complete_open(load.generation, {bytes.data(), bytes.size()}).ok());
  }
  ~Rig() {
    RPCMP_CHECK(suite_, preparation.violations == 0);
    RPCMP_CHECK(suite_, audio.violations == 0);
  }
  Rig(const Rig&) = delete;
  Rig& operator=(const Rig&) = delete;
  TransportIntent intent(const TransportIntentKind kind, const TrackId track = {}) {
    PlaybackSelection selection;
    if (kind == TransportIntentKind::PlayTrack || kind == TransportIntentKind::LoadTrack)
      selection = {catalog.status().generation, track};
    return {++command_id, kind, selection};
  }
  TransportStepResult tick(const std::initializer_list<TransportIntent> intents = {}) {
    TransportBatch batch;
    for (const auto& item : intents)
      batch.intents[batch.count++] = item;
    return controller.step(now++, batch);
  }
  void accept(const TransportIntentKind kind, const TrackId track = {}) {
    RPCMP_CHECK(suite_, tick({intent(kind, track)}).decisions[0].accepted());
  }
  void ack(const std::uint64_t frame = 0) {
    audio.complete(AudioControlOutcome::Success, frame);
    static_cast<void>(tick());
  }
  void boot() {
    static_cast<void>(tick());
    RPCMP_CHECK(suite_, state().transport == TransportState::Loading);
    ack();
    RPCMP_CHECK(suite_, state().transport == TransportState::Stopped && !state().selection);
  }
  void prepare(const bool play = true, const TrackId track = kB2) {
    accept(play ? TransportIntentKind::PlayTrack : TransportIntentKind::LoadTrack, track);
    ack();
    RPCMP_CHECK(suite_, preparation.pending.has_value());
    preparation.progress = PreparationProgress::Ready;
    static_cast<void>(tick());
  }
  void playing() {
    boot();
    prepare();
    ack();
  }
  TransportSnapshot state() const { return controller.snapshot(); }
  std::vector<std::uint8_t> bytes;
  CatalogSession catalog;
  std::vector<std::string> trace;
  ScriptedPreparation preparation;
  ScriptedAudio audio;
  TransportController controller;
  std::uint64_t now{};
  std::uint64_t command_id{};

private:
  rpcmp::test::Suite& suite_;
};

void basic_transport(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.playing();
  RPCMP_CHECK(suite, rig.trace == std::vector<std::string>({"Reset", "Reset", "Prepare", "Start"}));
  RPCMP_CHECK(suite,
              rig.state().transport == TransportState::Playing && !rig.state().pending_intent);
  const auto generation = rig.state().play_generation;
  rig.audio.observation.media_frame = 24'000;
  static_cast<void>(rig.tick());
  rig.accept(TransportIntentKind::Pause);
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Playing &&
                         rig.state().projected == TransportState::Paused);
  const auto pause_id = required(rig.state().audio_control).request.operation_id;
  rig.accept(TransportIntentKind::Pause);
  RPCMP_CHECK(suite, required(rig.state().audio_control).request.operation_id == pause_id);
  rig.accept(TransportIntentKind::Resume); // Valid against pending Pause's projection.
  rig.ack(24'000);
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Paused &&
                         required(rig.audio.pending).kind == AudioControlKind::Resume);
  rig.ack(24'000);
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Playing &&
                         rig.state().media_frame == 24'000);
  RPCMP_CHECK(suite, rig.state().play_generation == generation);
  rig.accept(TransportIntentKind::Play);
  RPCMP_CHECK(suite, !rig.audio.pending && rig.state().play_generation == generation);
  rig.accept(TransportIntentKind::Stop);
  RPCMP_CHECK(suite,
              rig.state().transport == TransportState::Loading && rig.preparation.releases == 0);
  rig.audio.observation.ended = true; // Old generation cannot auto-advance after Stop.
  static_cast<void>(rig.tick());
  rig.ack();
  RPCMP_CHECK(suite,
              rig.state().transport == TransportState::Stopped && rig.state().media_frame == 0);
  RPCMP_CHECK(suite,
              required(rig.state().selection).track_id == kB2 && rig.preparation.releases == 1);
  const auto stopped_generation = rig.state().play_generation;
  rig.accept(TransportIntentKind::Stop);
  RPCMP_CHECK(suite, !rig.audio.pending && rig.state().play_generation == stopped_generation);
  rig.accept(TransportIntentKind::Play);
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Loading &&
                         rig.state().play_generation > stopped_generation);
}

void prepared_and_admission(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.boot();
  RPCMP_CHECK(suite, rig.tick({rig.intent(TransportIntentKind::Play)}).decisions[0].rejection ==
                         TransportRejection::InvalidState);
  rig.prepare(false);
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Stopped && rig.state().prepared &&
                         !rig.audio.pending);
  const auto generation = rig.state().play_generation;
  const auto batch =
      rig.tick({rig.intent(TransportIntentKind::Play), rig.intent(TransportIntentKind::Pause),
                rig.intent(TransportIntentKind::Resume)});
  RPCMP_CHECK(suite, batch.count == 3 && batch.decisions[0].accepted() &&
                         batch.decisions[1].accepted() && batch.decisions[2].accepted());
  rig.ack();
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Playing &&
                         rig.state().play_generation == generation);
  const auto trace = rig.trace;
  auto stale = rig.intent(TransportIntentKind::PlayTrack, kB1);
  --stale.selection.library_generation.value;
  RPCMP_CHECK(suite, rig.tick({stale}).decisions[0].rejection == TransportRejection::StaleLibrary);
  RPCMP_CHECK(suite,
              rig.tick({rig.intent(TransportIntentKind::PlayTrack, {0})}).decisions[0].rejection ==
                  TransportRejection::UnknownTrack);
  RPCMP_CHECK(suite, rig.trace == trace && required(rig.state().selection).track_id == kB2);
  auto malformed = rig.intent(TransportIntentKind::Stop);
  malformed.selection.track_id = kB1;
  RPCMP_CHECK(suite, rig.tick({malformed}).decisions[0].rejection == TransportRejection::Malformed);
  malformed = rig.intent(TransportIntentKind::Stop);
  malformed.command_id = 0;
  RPCMP_CHECK(suite, rig.tick({malformed}).decisions[0].rejection == TransportRejection::Malformed);
  malformed.command_id = 999;
  const std::uint8_t invalid_kind = 255;
  static_assert(sizeof(malformed.kind) == sizeof(invalid_kind));
  std::memcpy(&malformed.kind, &invalid_kind, sizeof(invalid_kind));
  RPCMP_CHECK(suite, rig.tick({malformed}).decisions[0].rejection == TransportRejection::Malformed);
  rig.audio.pause_supported = false;
  for (const auto kind :
       {TransportIntentKind::Pause, TransportIntentKind::Resume, TransportIntentKind::TogglePause})
    RPCMP_CHECK(suite, rig.tick({rig.intent(kind)}).decisions[0].rejection ==
                           TransportRejection::UnsupportedPause);
  TransportBatch oversized;
  oversized.count = 65535;
  const auto before = rig.audio.observations;
  const auto rejected = rig.controller.step(rig.now++, oversized);
  RPCMP_CHECK(suite,
              rejected.batch_error == TransportRejection::BatchTooLarge && rejected.count == 0);
  RPCMP_CHECK(suite, rig.audio.observations == before + 1);
}

void cancellations(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.boot();
  rig.accept(TransportIntentKind::PlayTrack, kB2);
  const auto old_reset = required(rig.audio.pending);
  rig.accept(TransportIntentKind::PlayTrack, kB1);
  const auto second_generation = rig.state().play_generation;
  rig.ack(); // Old reset ACK requires a fresh reset; preparation cannot begin yet.
  RPCMP_CHECK(suite, !rig.preparation.pending &&
                         required(rig.audio.pending).kind == AudioControlKind::Reset);
  RPCMP_CHECK(suite, required(rig.audio.pending).operation_id != old_reset.operation_id &&
                         required(rig.audio.pending).play_generation == second_generation);
  rig.ack();
  rig.accept(TransportIntentKind::PlayTrack, kB2);
  RPCMP_CHECK(suite, rig.preparation.cancels == 1);
  rig.ack();
  RPCMP_CHECK(suite, required(rig.preparation.pending).selection.track_id == kB1);
  rig.preparation.progress = PreparationProgress::Ready; // Cancellation races with success.
  static_cast<void>(rig.tick());
  RPCMP_CHECK(suite, rig.preparation.releases == 1 &&
                         required(rig.preparation.pending).selection.track_id == kB2);
  RPCMP_CHECK(suite, rig.preparation.cancels == 1 && !rig.audio.pending);
  rig.accept(TransportIntentKind::Stop);
  rig.preparation.progress = PreparationProgress::Failed; // Old parse failure is discarded.
  rig.ack();
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Stopped &&
                         rig.state().failure == TransportFailure::None);
  RPCMP_CHECK(suite, rig.trace == std::vector<std::string>({"Reset", "Reset", "Reset", "Prepare",
                                                            "Cancel", "Reset", "Release", "Prepare",
                                                            "Cancel", "Reset"}));

  const auto fast = rig.tick({rig.intent(TransportIntentKind::PlayTrack, kB2),
                              rig.intent(TransportIntentKind::PlayTrack, kB1),
                              rig.intent(TransportIntentKind::Stop)});
  RPCMP_CHECK(suite, fast.decisions[0].accepted() && fast.decisions[1].accepted() &&
                         fast.decisions[2].accepted());
  rig.ack();
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Stopped &&
                         required(rig.state().selection).track_id == kB1 &&
                         !rig.preparation.pending);
  rig.accept(TransportIntentKind::PlayTrack, kB2);
  for (const auto kind : {TransportIntentKind::Play, TransportIntentKind::Pause,
                          TransportIntentKind::Resume, TransportIntentKind::TogglePause})
    RPCMP_CHECK(suite, rig.tick({rig.intent(kind)}).decisions[0].rejection ==
                           TransportRejection::InvalidState);
}

void end_and_catalog(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.playing();
  rig.audio.observation.media_frame = 70;
  static_cast<void>(rig.tick());
  rig.accept(TransportIntentKind::Pause);
  rig.audio.complete(AudioControlOutcome::Success, 80);
  rig.audio.observation.ended = true;
  static_cast<void>(rig.tick());
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Paused && !rig.audio.pending);
  static_cast<void>(rig.tick());
  rig.accept(TransportIntentKind::Play);
  rig.ack(80);
  RPCMP_CHECK(suite, required(rig.audio.pending).kind == AudioControlKind::Reset);
  rig.ack();
  RPCMP_CHECK(suite,
              rig.state().transport == TransportState::Ended && rig.state().media_frame == 80);
  rig.accept(TransportIntentKind::Play);
  rig.ack();
  rig.preparation.progress = PreparationProgress::Ready;
  static_cast<void>(rig.tick());
  rig.ack();
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Playing);
  const auto reload = rig.catalog.begin_open();
  static_cast<void>(rig.tick());
  RPCMP_CHECK(suite, !rig.state().selection && rig.state().transport == TransportState::Loading);
  RPCMP_CHECK(suite,
              rig.tick({rig.intent(TransportIntentKind::PlayTrack, kB1)}).decisions[0].rejection ==
                  TransportRejection::LibraryUnavailable);
  rig.ack();
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Loading && !rig.audio.sound_using);
  RPCMP_CHECK(
      suite,
      rig.catalog.complete_open(reload.generation, {rig.bytes.data(), rig.bytes.size()}).ok());
  static_cast<void>(rig.tick());
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Stopped && !rig.state().selection);
  RPCMP_CHECK(suite, rig.catalog.close().ok());
  static_cast<void>(rig.tick());
  rig.ack();
  std::vector<std::uint8_t>().swap(rig.bytes); // Both ports are now quiescent.
  RPCMP_CHECK(suite, rig.state().transport == TransportState::Empty && !rig.state().selection);
}

void failures_and_deadlines(rpcmp::test::Suite& suite) {
  {
    Rig rig(suite);
    rig.playing();
    rig.accept(TransportIntentKind::Pause);
    rig.audio.observation.fault = true;
    const auto rejected = rig.tick({rig.intent(TransportIntentKind::PlayTrack, kB1)});
    RPCMP_CHECK(suite, rejected.decisions[0].rejection == TransportRejection::Busy);
    RPCMP_CHECK(suite, rig.state().failure == TransportFailure::DeviceFault && rig.audio.inhibited);
    rig.audio.complete(
        AudioControlOutcome::Failed); // Failure is shared even after generation change.
    rig.audio.observation.fault = false;
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().failure == TransportFailure::AudioControl &&
                           required(rig.audio.pending).kind == AudioControlKind::Reset);
    rig.ack();
    RPCMP_CHECK(suite, rig.state().transport == TransportState::Error &&
                           rig.state().silence_confirmed && !rig.state().terminal);
    rig.prepare(true, kB1);
    rig.ack();
    RPCMP_CHECK(suite, rig.state().transport == TransportState::Playing);
  }
  {
    Rig rig(suite);
    rig.playing();
    rig.accept(TransportIntentKind::Stop);
    rig.audio.complete(AudioControlOutcome::Failed);
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().terminal &&
                           rig.state().failure == TransportFailure::ResetFailed &&
                           rig.audio.inhibited);
    RPCMP_CHECK(suite, rig.tick({rig.intent(TransportIntentKind::Stop)}).decisions[0].rejection ==
                           TransportRejection::TerminalFailure);
    RPCMP_CHECK(suite,
                rig.state().transport == TransportState::Error && rig.preparation.releases == 0);
  }
  {
    Rig rig(suite);
    rig.boot();
    rig.accept(TransportIntentKind::PlayTrack, kB2);
    rig.ack();
    const auto started = required(rig.state().preparation).started_at_us;
    static_cast<void>(rig.controller.step(started + 99));
    RPCMP_CHECK(suite, rig.state().failure == TransportFailure::None);
    static_cast<void>(rig.controller.step(started + 100));
    RPCMP_CHECK(suite, rig.state().failure == TransportFailure::PreparationTimeout &&
                           rig.preparation.cancels == 1);
    rig.now = started + 101;
    rig.ack();
    RPCMP_CHECK(
        suite, rig.tick({rig.intent(TransportIntentKind::PlayTrack, kB1)}).decisions[0].rejection ==
                   TransportRejection::Busy);
    rig.preparation.progress = PreparationProgress::Ready;
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().transport == TransportState::Error && !rig.audio.pending &&
                           rig.preparation.releases == 1);
    rig.accept(TransportIntentKind::PlayTrack, kB1);
    RPCMP_CHECK(suite, rig.state().transport == TransportState::Loading);
  }
  {
    Rig rig(suite);
    static_cast<void>(rig.tick());
    const auto start = required(rig.state().audio_control).started_at_us;
    static_cast<void>(rig.controller.step(start + 19));
    RPCMP_CHECK(suite, !rig.state().terminal);
    static_cast<void>(rig.controller.step(start + 20));
    RPCMP_CHECK(suite, rig.state().terminal &&
                           rig.state().failure == TransportFailure::AudioTimeout &&
                           rig.audio.pending);
    rig.audio.complete();
    static_cast<void>(rig.controller.step(start + 21));
    RPCMP_CHECK(suite, rig.state().terminal && rig.audio.inhibited &&
                           rig.state().transport == TransportState::Error);
  }
  {
    Rig rig(suite);
    static_cast<void>(rig.tick());
    rig.audio.complete();
    static_cast<void>(rig.controller.step(20)); // ACK exactly at deadline is accepted.
    RPCMP_CHECK(suite, !rig.state().terminal && rig.state().transport == TransportState::Stopped);
    rig.now = 21;
    rig.accept(TransportIntentKind::PlayTrack, kB2);
    rig.ack();
    const auto start = required(rig.state().preparation).started_at_us;
    rig.preparation.progress = PreparationProgress::Ready;
    static_cast<void>(rig.controller.step(start + 100));
    RPCMP_CHECK(suite, rig.state().failure == TransportFailure::None &&
                           required(rig.audio.pending).kind == AudioControlKind::Start);
  }
}

void protocol_and_limits(rpcmp::test::Suite& suite) {
  {
    Rig rig(suite);
    rig.playing();
    rig.audio.observation.media_frame = 10;
    static_cast<void>(rig.tick());
    rig.accept(TransportIntentKind::Pause);
    rig.ack(10);
    rig.audio.observation.media_frame = 11;
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().failure == TransportFailure::Protocol && rig.state().terminal &&
                           rig.audio.inhibited);
  }
  {
    Rig rig(suite);
    static_cast<void>(rig.tick());
    const auto current = required(rig.audio.pending);
    auto old = current;
    ++old.operation_id;
    rig.audio.observation.completion = AudioControlCompletion{old, AudioControlOutcome::Success, 0};
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite,
                required(rig.state().audio_control).request.operation_id == current.operation_id);
    auto wrong = current;
    ++wrong.play_generation;
    rig.audio.observation.completion =
        AudioControlCompletion{wrong, AudioControlOutcome::Success, 0};
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().terminal && rig.state().failure == TransportFailure::Protocol);
  }
  {
    Rig rig(suite);
    rig.boot();
    static_cast<void>(rig.controller.step(0));
    RPCMP_CHECK(suite, rig.state().terminal && rig.state().failure == TransportFailure::Clock);
  }
  for (const auto timing : {TransportTiming{0, 20}, TransportTiming{100, 0}}) {
    Rig rig(suite, timing);
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().terminal &&
                           rig.state().failure == TransportFailure::InvalidConfiguration &&
                           rig.audio.inhibited);
  }
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  for (const auto counters : {TransportCounters{maximum, 0}, TransportCounters{0, maximum}}) {
    Rig rig(suite, {100, 20}, counters);
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().terminal &&
                           rig.state().failure == TransportFailure::ResourceExhausted &&
                           !rig.audio.pending);
  }
  {
    Rig rig(suite);
    static_cast<void>(rig.controller.step(maximum - 5));
    rig.audio.complete();
    static_cast<void>(rig.controller.step(maximum));
    RPCMP_CHECK(suite, rig.state().transport == TransportState::Stopped && !rig.state().terminal);
  }
}

void failure_ownership(rpcmp::test::Suite& suite) {
  {
    Rig rig(suite);
    rig.boot();
    const auto loading = rig.catalog.begin_open();
    static_cast<void>(rig.tick());
    rig.audio.observation.fault = true;
    static_cast<void>(rig.tick());
    rig.audio.observation.fault = false;
    rig.ack(); // Older reset; the fault's generation still needs its own reset.
    rig.ack();
    RPCMP_CHECK(suite, rig.state().transport == TransportState::Error);
    RPCMP_CHECK(
        suite,
        rig.catalog.complete_open(loading.generation, {rig.bytes.data(), rig.bytes.size()}).ok());
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().transport == TransportState::Error &&
                           rig.state().failure == TransportFailure::DeviceFault);
    rig.accept(TransportIntentKind::PlayTrack, kB2);
    RPCMP_CHECK(suite, rig.state().failure == TransportFailure::None);
  }
  {
    Rig rig(suite);
    static_cast<void>(rig.tick());
    static_cast<void>(rig.controller.step(20));
    rig.audio.complete(AudioControlOutcome::Success,
                       1); // Invalid late Reset ACK must not lift inhibit.
    static_cast<void>(rig.controller.step(21));
    RPCMP_CHECK(suite, rig.state().terminal && rig.audio.inhibited);
  }
  {
    Rig rig(suite);
    rig.playing();
    rig.accept(TransportIntentKind::Stop);
    const auto deadline = required(rig.state().audio_control).started_at_us + 20;
    static_cast<void>(rig.controller.step(deadline));
    RPCMP_CHECK(suite, rig.preparation.releases == 0 && rig.audio.inhibited);
    rig.audio.complete();
    static_cast<void>(rig.controller.step(deadline + 1));
    RPCMP_CHECK(suite,
                rig.preparation.releases == 1 && rig.state().terminal && rig.audio.inhibited);
  }
}

void additional_port_edges(rpcmp::test::Suite& suite) {
  {
    Rig rig(suite);
    rig.boot();
    rig.prepare(); // T has prepared; its Start is still in flight.
    rig.accept(TransportIntentKind::PlayTrack, kB1);
    rig.ack(77);
    RPCMP_CHECK(suite,
                rig.state().transport == TransportState::Loading && rig.state().media_frame == 0);
    RPCMP_CHECK(suite,
                required(rig.state().selection).track_id == kB1 && rig.preparation.releases == 0);
    RPCMP_CHECK(suite, required(rig.audio.pending).kind == AudioControlKind::Reset);
    rig.ack();
    RPCMP_CHECK(suite, rig.preparation.releases == 1 &&
                           required(rig.preparation.pending).selection.track_id == kB1);
    rig.accept(TransportIntentKind::Stop);
    rig.preparation.progress = PreparationProgress::Cancelled;
    rig.ack();
    RPCMP_CHECK(suite,
                rig.state().transport == TransportState::Stopped && !rig.state().preparation);
  }
  {
    Rig rig(suite);
    rig.boot();
    rig.accept(TransportIntentKind::PlayTrack, kB2);
    rig.preparation.reject_begin = true;
    rig.ack();
    RPCMP_CHECK(suite,
                rig.state().failure == TransportFailure::Preparation && !rig.preparation.pending);
    static_cast<void>(rig.tick());
    rig.ack();
    rig.accept(TransportIntentKind::PlayTrack, kB1);
    rig.ack();
    rig.preparation.progress = PreparationProgress::Failed;
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().failure == TransportFailure::Preparation &&
                           required(rig.audio.pending).kind == AudioControlKind::Reset);
    rig.ack();
    RPCMP_CHECK(suite, rig.state().transport == TransportState::Error && !rig.state().terminal);
  }
  {
    Rig rig(suite);
    rig.boot();
    rig.accept(TransportIntentKind::PlayTrack, kB2);
    rig.ack();
    rig.audio.reject_begin = true;
    rig.preparation.progress = PreparationProgress::Ready;
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().failure == TransportFailure::AudioControl &&
                           !rig.audio.pending && rig.preparation.releases == 0);
    static_cast<void>(rig.tick());
    rig.ack();
    RPCMP_CHECK(suite, rig.preparation.releases == 1 && rig.state().silence_confirmed);
  }
  {
    Rig rig(suite);
    rig.audio.reject_begin = true;
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.state().terminal &&
                           rig.state().failure == TransportFailure::ResetFailed &&
                           !rig.audio.pending);
    const auto trace = rig.trace;
    static_cast<void>(rig.tick());
    RPCMP_CHECK(suite, rig.trace == trace);
  }
  {
    Rig rig(suite);
    rig.playing();
    rig.accept(TransportIntentKind::Pause);
    rig.ack(50);
    rig.accept(TransportIntentKind::Resume);
    rig.ack(51);
    RPCMP_CHECK(suite, rig.state().terminal && rig.state().failure == TransportFailure::Protocol);
  }
  {
    Rig rig(suite);
    rig.playing();
    TransportBatch batch;
    batch.count = 32;
    for (auto& intent : batch.intents)
      intent = rig.intent(TransportIntentKind::Play);
    const auto trace = rig.trace;
    const auto result = rig.controller.step(rig.now++, batch);
    RPCMP_CHECK(suite, result.count == 32 && result.batch_error == TransportRejection::None);
    for (const auto& decision : result.decisions)
      RPCMP_CHECK(suite, decision.accepted());
    RPCMP_CHECK(suite, rig.trace == trace);
    rig.audio.observation.media_frame = 500;
    rig.audio.observation.ended = true;
    rig.accept(TransportIntentKind::Stop); // Command wins over same-boundary natural end.
    rig.ack();
    RPCMP_CHECK(suite,
                rig.state().transport == TransportState::Stopped && rig.state().media_frame == 0);
  }
  {
    Rig rig(suite);
    static_cast<void>(rig.tick());
    TransportBatch oversized;
    oversized.count = 33;
    const auto result = rig.controller.step(20, oversized);
    RPCMP_CHECK(suite, result.batch_error == TransportRejection::BatchTooLarge &&
                           rig.state().failure == TransportFailure::AudioTimeout);
  }
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  basic_transport(suite);
  prepared_and_admission(suite);
  cancellations(suite);
  end_and_catalog(suite);
  failures_and_deadlines(suite);
  protocol_and_limits(suite);
  failure_ownership(suite);
  additional_port_edges(suite);
  return suite.finish("Core playback transport");
}
