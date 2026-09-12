#include "rpcmp/player/snapshot_publisher.hpp"
#include "test_support.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace rpcmp::player;
using rpcmp::contracts::TransportState;
namespace api = rpcmp::contracts::v2;
constexpr rpcmp::contracts::TrackId kB2{0x47305869eca5f89aULL};
constexpr rpcmp::contracts::AlbumId kBeta{0x072692aa06258cebULL};

template <typename T> T required(const std::optional<T>& value) {
  if (!value)
    throw std::logic_error("missing test observation");
  return *value;
}
std::string_view text(const rpcmp::contracts::CatalogText& value) {
  return {value.bytes.data(), value.length};
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
class Preparation final : public PreparationPort {
public:
  bool begin(const PreparationRequest&) override {
    ++calls;
    return true;
  }
  void cancel(std::uint64_t) override { ++calls; }
  PreparationProgress poll(std::uint64_t) override {
    ++calls;
    const auto result = progress;
    progress = PreparationProgress::Pending;
    return result;
  }
  void release() override { ++calls; }
  PreparationProgress progress{PreparationProgress::Pending};
  unsigned calls{};
};
class Audio final : public AudioTransportPort {
public:
  bool supports_pause() const noexcept override { return true; }
  bool begin(const AudioControlRequest& value) override {
    if (pending)
      throw std::logic_error("overlapping control");
    pending = value;
    ++calls;
    return true;
  }
  AudioObservation observe() override {
    ++calls;
    const auto result = observation;
    observation.completion.reset();
    return result;
  }
  void emergency_silence() override { ++calls; }
  void ack(const std::uint64_t frame = 0) {
    const auto request = required(pending);
    pending.reset();
    observation = {false, request.play_generation, frame, false,
                   AudioControlCompletion{request, AudioControlOutcome::Success, frame}};
  }
  std::optional<AudioControlRequest> pending;
  AudioObservation observation;
  unsigned calls{};
};
struct Rig {
  explicit Rig(rpcmp::test::Suite& checks) : suite(checks) {
    const auto opening = catalog.begin_open();
    RPCMP_CHECK(suite, bytes.size() == 1360 && opening.ok());
    RPCMP_CHECK(suite,
                catalog.complete_open(opening.generation, {bytes.data(), bytes.size()}).ok());
  }
  void tick(const std::optional<TransportIntentKind> kind = std::nullopt) {
    TransportBatch batch;
    if (kind) {
      PlaybackSelection selected;
      if (*kind == TransportIntentKind::PlayTrack || *kind == TransportIntentKind::LoadTrack)
        selected = {catalog.status().generation, kB2};
      batch.intents[0] = {++command_id, *kind, selected};
      batch.count = 1;
    }
    const auto result = transport.step(now, batch);
    if (kind)
      RPCMP_CHECK(suite, result.decisions[0].accepted());
    RPCMP_CHECK(suite, publisher.publish(now++, transport.snapshot()) == PublicationResult::Ok);
    RPCMP_CHECK(suite, api::valid_player_snapshot(publisher.latest()));
  }
  void playing() {
    tick();
    audio.ack();
    tick();
    tick(TransportIntentKind::PlayTrack);
    audio.ack();
    tick();
    preparation.progress = PreparationProgress::Ready;
    tick();
    audio.ack();
    tick();
  }
  rpcmp::test::Suite& suite;
  std::vector<std::uint8_t> bytes{fixture()};
  CatalogSession catalog;
  Preparation preparation;
  Audio audio;
  TransportController transport{catalog, preparation, audio, {100, 20}};
  SnapshotPublisher publisher{catalog};
  std::uint64_t now{};
  std::uint64_t command_id{};
};

class CountingCatalog final : public rpcmp::contracts::CatalogReader {
public:
  explicit CountingCatalog(const CatalogSession& session) : session_(session) {}
  rpcmp::contracts::CatalogStatus status() const noexcept override { return session_.status(); }
  rpcmp::contracts::CatalogAlbumPage
  albums(const rpcmp::contracts::CatalogPageQuery& query) const override {
    ++page_reads;
    return session_.albums(query);
  }
  rpcmp::contracts::CatalogTrackPage
  tracks(rpcmp::contracts::AlbumId album,
         const rpcmp::contracts::CatalogPageQuery& query) const override {
    ++page_reads;
    auto page = session_.tracks(album, query);
    if (corrupt && page.header.count != 0)
      page.items[0].title.length = 65535;
    return page;
  }
  mutable unsigned page_reads{};
  bool corrupt{};

private:
  const CatalogSession& session_;
};

void metadata_cache(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.playing();
  CountingCatalog reader(rig.catalog);
  SnapshotPublisher publisher(reader);
  RPCMP_CHECK(suite, publisher.publish(rig.now, rig.transport.snapshot()) == PublicationResult::Ok);
  RPCMP_CHECK(suite, reader.page_reads == 2);
  RPCMP_CHECK(suite, publisher.publish(rig.now, rig.transport.snapshot()) == PublicationResult::Ok);
  RPCMP_CHECK(suite, reader.page_reads == 2 && publisher.latest().sequence == 2);
  reader.corrupt = true;
  SnapshotPublisher malformed(reader);
  RPCMP_CHECK(suite, malformed.publish(rig.now, rig.transport.snapshot()) ==
                         PublicationResult::InvalidObservation);
  RPCMP_CHECK(suite, malformed.latest().sequence == 0 && !malformed.latest().track);
  reader.corrupt = false;
  const auto opening = rig.catalog.begin_open();
  RPCMP_CHECK(suite, opening.ok());
  RPCMP_CHECK(
      suite,
      rig.catalog.complete_open(opening.generation, {rig.bytes.data(), rig.bytes.size()}).ok());
  rig.tick();
  rig.audio.ack();
  rig.tick();
  rig.tick(TransportIntentKind::LoadTrack);
  rig.audio.ack();
  rig.tick();
  rig.preparation.progress = PreparationProgress::Ready;
  rig.tick();
  const auto previous_reads = reader.page_reads;
  RPCMP_CHECK(suite, publisher.publish(rig.now, rig.transport.snapshot()) == PublicationResult::Ok);
  RPCMP_CHECK(suite, reader.page_reads == previous_reads + 2 &&
                         publisher.latest().library.generation == opening.generation);
  RPCMP_CHECK(suite, publisher.latest().transport == TransportState::Stopped &&
                         publisher.latest().prepared);
}

void lifecycle(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  RPCMP_CHECK(suite, rig.publisher.latest().sequence == 0);
  rig.playing();
  const api::SnapshotSource& reader = rig.publisher;
  auto playing = reader.latest();
  RPCMP_CHECK(suite, playing.transport == TransportState::Playing && playing.prepared);
  RPCMP_CHECK(suite, required(playing.track).item.track_id == kB2 &&
                         required(playing.track).item.album_id == kBeta &&
                         required(playing.track).item.track_ordinal == 0);
  RPCMP_CHECK(suite, text(required(playing.track).item.title) == "B2" &&
                         text(required(playing.track).album_name) == "Beta");
  RPCMP_CHECK(suite, !playing.pending_intent && !playing.audio_control);
  const auto audio_calls = rig.audio.calls;
  const auto preparation_calls = rig.preparation.calls;
  for (unsigned i = 0; i < 120; ++i) {
    auto copy = reader.latest();
    copy.track.reset();
    copy.position_frames = 99;
    RPCMP_CHECK(suite, reader.latest().track.has_value() && reader.latest().position_frames == 0);
  }
  RPCMP_CHECK(suite, rig.audio.calls == audio_calls && rig.preparation.calls == preparation_calls);
  rig.audio.observation.media_frame = 123;
  rig.tick(TransportIntentKind::Pause);
  const auto waiting = reader.latest();
  RPCMP_CHECK(suite, waiting.transport == TransportState::Playing &&
                         waiting.projected == TransportState::Paused);
  RPCMP_CHECK(suite, required(waiting.pending_intent).kind == api::TransportIntentKind::Pause &&
                         required(waiting.audio_control).kind == api::AudioControlKind::Pause);
  rig.audio.ack(125);
  rig.tick();
  RPCMP_CHECK(suite, reader.latest().transport == TransportState::Paused &&
                         reader.latest().position_frames == 125);
  rig.now += 100'000;
  rig.tick();
  RPCMP_CHECK(suite,
              reader.latest().position_frames == 125 && reader.latest().published_at_us > 100'000);
  rig.tick(TransportIntentKind::Resume);
  rig.audio.ack(125);
  rig.tick();
  rig.tick(TransportIntentKind::Stop);
  RPCMP_CHECK(suite, reader.latest().transport == TransportState::Loading &&
                         !reader.latest().silence_confirmed);
  rig.audio.ack();
  rig.tick();
  RPCMP_CHECK(suite, reader.latest().transport == TransportState::Stopped &&
                         reader.latest().silence_confirmed && reader.latest().position_frames == 0);
  RPCMP_CHECK(suite, playing.transport == TransportState::Playing); // Older copy stays complete.

  const auto before = reader.latest();
  RPCMP_CHECK(suite, rig.catalog.close().ok());
  RPCMP_CHECK(suite, rig.publisher.publish(rig.now, rig.transport.snapshot()) ==
                         PublicationResult::CatalogChanged);
  RPCMP_CHECK(suite,
              reader.latest().sequence == before.sequence && reader.latest().track.has_value());
  rig.tick();
  RPCMP_CHECK(suite, !reader.latest().track &&
                         reader.latest().library.phase == rpcmp::contracts::CatalogPhase::Empty);
  rig.audio.ack();
  rig.tick();
  std::vector<std::uint8_t>().swap(rig.bytes);
  RPCMP_CHECK(suite, text(required(before.track).item.title) == "B2");
}

void failure_and_limits(rpcmp::test::Suite& suite) {
  Rig rig(suite);
  rig.playing();
  rig.tick(TransportIntentKind::Stop);
  rig.now += 20;
  rig.tick();
  const auto failed = rig.publisher.latest();
  RPCMP_CHECK(suite, required(failed.error).terminal &&
                         required(failed.error).code == api::PlaybackErrorCode::AudioTimeout &&
                         !failed.silence_confirmed && failed.audio_control.has_value());
  RPCMP_CHECK(suite, rig.publisher.publish(0, rig.transport.snapshot()) ==
                         PublicationResult::ClockReversed);
  RPCMP_CHECK(suite, rig.publisher.latest().sequence == failed.sequence);
  auto invalid = rig.transport.snapshot();
  invalid.selection = PlaybackSelection{rig.catalog.status().generation, {999}};
  RPCMP_CHECK(suite,
              rig.publisher.publish(rig.now, invalid) == PublicationResult::InvalidObservation);
  RPCMP_CHECK(suite, rig.publisher.latest().sequence == failed.sequence);
  invalid = rig.transport.snapshot();
  invalid.terminal = true;
  invalid.failure = TransportFailure::None;
  RPCMP_CHECK(suite,
              rig.publisher.publish(rig.now, invalid) == PublicationResult::InvalidObservation);
  const auto max = std::numeric_limits<std::uint64_t>::max();
  SnapshotPublisher near_limit(rig.catalog, max - 1);
  RPCMP_CHECK(suite, near_limit.publish(max, rig.transport.snapshot()) == PublicationResult::Ok);
  RPCMP_CHECK(suite, near_limit.latest().sequence == max);
  RPCMP_CHECK(suite, near_limit.publish(max, rig.transport.snapshot()) ==
                         PublicationResult::SequenceExhausted);
  RPCMP_CHECK(suite, near_limit.latest().sequence == max);

  // Fault can retain the same prepared generation until a late reset quiesces it.
  Rig terminal(suite);
  terminal.playing();
  auto clock_failure = terminal.transport.step(0);
  RPCMP_CHECK(suite, clock_failure.count == 0 && terminal.transport.snapshot().terminal);
  RPCMP_CHECK(suite, terminal.catalog.close().ok());
  terminal.tick();
  RPCMP_CHECK(suite, !terminal.publisher.latest().track && !terminal.publisher.latest().prepared &&
                         required(terminal.publisher.latest().error).terminal);
}

std::vector<std::uint64_t> playback_trace(rpcmp::test::Suite& suite, const bool read_often) {
  Rig rig(suite);
  rig.playing();
  std::vector<std::uint64_t> trace;
  for (std::uint64_t i = 0; i < 120; ++i) {
    rig.audio.observation.media_frame = 800 * i;
    rig.tick();
    if (read_often) {
      for (unsigned n = 0; n < 7; ++n)
        static_cast<void>(rig.publisher.latest());
    }
    const auto s = rig.publisher.latest();
    trace.insert(trace.end(), {s.sequence, s.published_at_us, s.position_frames, s.play_generation,
                               rig.audio.calls, rig.preparation.calls});
    RPCMP_CHECK(suite, s.position_frames == 800 * i);
  }
  return trace;
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  lifecycle(suite);
  metadata_cache(suite);
  failure_and_limits(suite);
  RPCMP_CHECK(suite, playback_trace(suite, false) == playback_trace(suite, true));
  return suite.finish("Player snapshot publisher");
}
