#include "rpcmp/contracts/player_state_v2.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>

namespace {
using namespace rpcmp::contracts;
namespace api = rpcmp::contracts::v2;
static_assert(std::is_trivially_copyable_v<api::PlayerSnapshot>);
static_assert(!std::is_same_v<api::PlayerSnapshot, PlayerSnapshot>);

CatalogText text(const std::string_view source) {
  CatalogText result;
  if (source.size() > result.bytes.size())
    throw std::logic_error("invalid authored text");
  std::copy(source.begin(), source.end(), result.bytes.begin());
  result.length = static_cast<std::uint16_t>(source.size());
  return result;
}
template <typename T> T& required(std::optional<T>& value) {
  if (!value)
    throw std::logic_error("missing authored observation");
  return *value;
}
template <typename T> void invalid_tag(T& tag) {
  static_assert(sizeof(T) == 1);
  const std::uint8_t raw = 255;
  std::memcpy(&tag, &raw, 1);
}
api::PlayerSnapshot playing() {
  api::PlayerSnapshot result;
  result.sequence = 93;
  result.published_at_us = 200'000'000;
  result.capabilities.bits |= api::kStatePreservingPause;
  result.library = {1, {18}, CatalogPhase::Ready, CatalogFailure::None, 2, 4};
  result.transport = TransportState::Playing;
  result.projected = TransportState::Paused;
  result.track = api::SelectedTrack{{{91}, {44}, 1, text("演奏中の曲")}, text("作品")};
  result.play_generation = 72;
  result.position_frames = 96'000;
  result.prepared = true;
  result.pending_intent = api::PendingIntent{6, api::TransportIntentKind::Pause, std::nullopt};
  result.audio_control = api::PendingAudioControl{29, 72, api::AudioControlKind::Pause};
  return result;
}
class MockSource final : public api::SnapshotSource {
public:
  api::PlayerSnapshot latest() const noexcept override { return state; }
  api::PlayerSnapshot state;
};
} // namespace

int main() {
  rpcmp::test::Suite suite;
  RPCMP_CHECK(suite, api::valid_player_snapshot({}));
  RPCMP_CHECK(suite, api::valid_player_snapshot(playing()));
  auto independent = playing();
  independent.capabilities.bits |= 1ULL << 63U;
  independent.preparation = api::PendingPreparation{22, 71, {{17}, {32}}, true};
  RPCMP_CHECK(suite, api::valid_player_snapshot(independent));
  MockSource source;
  source.state = independent;
  const api::SnapshotSource& reader = source;
  auto copy = reader.latest();
  required(copy.track).item.title.bytes[0] = 'x';
  copy.position_frames = 0;
  RPCMP_CHECK(suite, reader.latest().position_frames == 96'000);
  RPCMP_CHECK(suite, (reader.latest().capabilities.bits & (1ULL << 63U)) != 0);
  RPCMP_CHECK(suite, source.state.track && source.state.track->item.title.bytes[0] != 'x');

  const auto reject = [&](const auto& mutate) {
    auto value = playing();
    mutate(value);
    RPCMP_CHECK(suite, !api::valid_player_snapshot(value));
  };
  reject([](auto& s) { s.schema_version = 1; });
  reject([](auto& s) { s.frame_rate = 60'000; });
  reject([](auto& s) { s.capabilities.bits = 0; });
  reject([](auto& s) { s.library.track_count = 301; });
  reject([](auto& s) { invalid_tag(s.transport); });
  reject([](auto& s) { invalid_tag(s.projected); });
  reject([](auto& s) { s.track.reset(); });
  reject([](auto& s) { s.play_generation = 0; });
  reject([](auto& s) { required(s.track).item.track_id = {}; });
  reject([](auto& s) { required(s.track).item.album_id = {}; });
  reject([](auto& s) { required(s.track).item.track_ordinal = 4; });
  reject([](auto& s) { required(s.track).album_name.length = 0; });
  reject([](auto& s) { required(s.track).item.title.length = 65535; });
  reject([](auto& s) { required(s.track).item.title.bytes[0] = '\0'; });
  reject([](auto& s) { required(s.track).item.title.bytes[0] = static_cast<char>(0xFF); });
  reject([](auto& s) { required(s.track).item.title.truncated = true; });
  reject([](auto& s) { s.transport = TransportState::Stopped; });
  reject([](auto& s) { required(s.pending_intent).command_id = 0; });
  reject([](auto& s) { invalid_tag(required(s.pending_intent).kind); });
  reject([](auto& s) { required(s.pending_intent).selection = api::TrackSelection{{18}, {91}}; });
  reject([](auto& s) { required(s.pending_intent).kind = api::TransportIntentKind::PlayTrack; });
  reject([](auto& s) { required(s.audio_control).operation_id = 0; });
  reject([](auto& s) { required(s.audio_control).play_generation = 0; });
  reject([](auto& s) { invalid_tag(required(s.audio_control).kind); });
  reject([](auto& s) { s.preparation = api::PendingPreparation{1, 0, {{18}, {91}}, false}; });
  reject([](auto& s) { s.preparation = api::PendingPreparation{1, 72, {{0}, {91}}, false}; });
  reject([](auto& s) { s.transport = TransportState::Error; });
  reject([](auto& s) { s.projected = TransportState::Error; });
  reject([](auto& s) { s.error = api::PlaybackError{api::PlaybackErrorCode::DeviceFault, false}; });

  for (const bool terminal : {false, true}) {
    auto error = playing();
    error.transport = error.projected = TransportState::Error;
    error.error = api::PlaybackError{api::PlaybackErrorCode::DeviceFault, terminal};
    RPCMP_CHECK(suite, api::valid_player_snapshot(error));
    invalid_tag(required(error.error).code);
    RPCMP_CHECK(suite, !api::valid_player_snapshot(error));
  }
  auto boundary = playing();
  required(boundary.track).item.title = text(std::string(96, 'a'));
  required(boundary.track).item.title.truncated = true;
  boundary.sequence = boundary.published_at_us = std::numeric_limits<std::uint64_t>::max();
  RPCMP_CHECK(suite, api::valid_player_snapshot(boundary));
  PlayerSnapshot old;
  old.schema_version = 2;
  RPCMP_CHECK(suite, validate_snapshot(old).error == ContractError::UnsupportedSchema);
  return suite.finish("Player state schema 2 contracts");
}
