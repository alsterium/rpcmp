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
  auto performance = playing();
  performance.capabilities.bits |= api::kPerformanceHistory;
  performance.performance_history.emplace();
  auto& performance_value = required(performance.performance_history);
  performance_value.play_generation = performance.play_generation;
  performance_value.observed_through_frame = performance.position_frames;
  performance_value.channels = api::unknown_performance_channels();
  RPCMP_CHECK(suite, api::valid_player_snapshot(performance));
  const auto reject_performance = [&](auto mutate) {
    auto value = performance;
    mutate(value);
    RPCMP_CHECK(suite, !api::valid_player_snapshot(value));
  };
  reject_performance([](auto& s) { s.capabilities.bits &= ~api::kPerformanceHistory; });
  reject_performance([](auto& s) { s.performance_history.reset(); });
  reject_performance([](auto& s) { required(s.performance_history).version = 2; });
  reject_performance([](auto& s) { required(s.performance_history).count = 257; });
  reject_performance([](auto& s) { required(s.performance_history).play_generation--; });
  reject_performance([](auto& s) { required(s.performance_history).observed_through_frame--; });
  performance_value.availability = api::PerformanceAvailability::Available;
  RPCMP_CHECK(suite, api::valid_player_snapshot(performance));
  performance.transport = performance.projected = TransportState::Stopped;
  performance.position_frames = performance_value.observed_through_frame = 0;
  RPCMP_CHECK(suite, !api::valid_player_snapshot(performance));
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

  auto policy = playing();
  policy.capabilities.bits |= api::kPolicyObservations | api::kRepeatControl;
  policy.policy = api::PlaybackPolicyObservation{
      {api::PlaybackOrder::AlbumOrder, api::RepeatMode::Counted, 3},
      5,
      12,
      api::PlaybackMediaObservation{
          72, 96'000, {4, 2}, 2, false, api::PlaybackPhase::Fading, 120'000, 120'000, 240'000}};
  RPCMP_CHECK(suite, api::valid_player_snapshot(policy));
  const auto reject_policy = [&](const auto& mutate) {
    auto copy_policy = policy;
    mutate(copy_policy);
    RPCMP_CHECK(suite, !api::valid_player_snapshot(copy_policy));
  };
  reject_policy([](auto& s) { s.capabilities.bits &= ~api::kPolicyObservations; });
  reject_policy([](auto& s) { s.policy.reset(); });
  reject_policy([](auto& s) { required(s.policy).revision = 0; });
  reject_policy([](auto& s) { required(s.policy).last_command_id = 0; });
  reject_policy([](auto& s) { required(s.policy).desired.count = 0; });
  reject_policy([](auto& s) { invalid_tag(required(s.policy).desired.order); });
  reject_policy([](auto& s) { invalid_tag(required(s.policy).desired.repeat); });
  reject_policy([](auto& s) { required(required(s.policy).media).applied.revision = 6; });
  reject_policy([](auto& s) { required(required(s.policy).media).applied.revision = 5; });
  reject_policy([](auto& s) { required(required(s.policy).media).position_frames = 0; });
  reject_policy([](auto& s) { required(required(s.policy).media).play_generation = 71; });
  reject_policy([](auto& s) { required(required(s.policy).media).loop_count_overflow = true; });
  reject_policy([](auto& s) { required(required(s.policy).media).completed_loops.reset(); });
  reject_policy([](auto& s) { required(required(s.policy).media).gain = 240'001; });
  reject_policy([](auto& s) { required(required(s.policy).media).ramp_elapsed = 240'001; });
  reject_policy([](auto& s) { required(required(s.policy).media).ramp_duration = 960; });
  reject_policy([](auto& s) { invalid_tag(required(required(s.policy).media).phase); });
  reject_policy([](auto& s) { invalid_tag(required(required(s.policy).media).end); });
  required(required(policy.policy).media).completed_loops.reset();
  required(required(policy.policy).media).loop_count_overflow = true;
  RPCMP_CHECK(suite, api::valid_player_snapshot(policy));
  policy.audio_control = api::PendingAudioControl{29, 72, api::AudioControlKind::SetPolicy,
                                                  api::RepeatApplication{5, 3}};
  RPCMP_CHECK(suite, api::valid_player_snapshot(policy));
  required(policy.audio_control).kind = api::AudioControlKind::Reset;
  RPCMP_CHECK(suite, !api::valid_player_snapshot(policy));
  required(policy.audio_control).kind = api::AudioControlKind::SetPolicy;
  required(policy.audio_control).repeat.reset();
  RPCMP_CHECK(suite, !api::valid_player_snapshot(policy));
  auto navigation = playing();
  navigation.capabilities.bits |=
      api::kPolicyObservations | api::kRepeatControl | api::kPlaybackNavigation;
  navigation.policy = api::PlaybackPolicyObservation{};
  required(navigation.policy).desired.order = api::PlaybackOrder::ShuffleLibrary;
  navigation.navigation =
      api::PlaybackNavigationObservation{true, true, api::ShuffleCycleObservation{42, {18}, 4, 2}};
  navigation.pending_intent =
      api::PendingIntent{19, api::TransportIntentKind::NextTrack, api::TrackSelection{{18}, {91}}};
  RPCMP_CHECK(suite, api::valid_player_snapshot(navigation));
  const auto reject_navigation = [&](const auto& mutate) {
    auto value = navigation;
    mutate(value);
    RPCMP_CHECK(suite, !api::valid_player_snapshot(value));
  };
  reject_navigation([](auto& s) { s.capabilities.bits &= ~api::kPlaybackNavigation; });
  reject_navigation([](auto& s) { s.capabilities.bits &= ~api::kRepeatControl; });
  reject_navigation([](auto& s) { s.navigation.reset(); });
  reject_navigation([](auto& s) { required(required(s.navigation).cycle).cycle_id = 0; });
  reject_navigation(
      [](auto& s) { required(required(s.navigation).cycle).library_generation = {17}; });
  reject_navigation([](auto& s) { required(required(s.navigation).cycle).total_tracks = 3; });
  reject_navigation([](auto& s) { required(required(s.navigation).cycle).started_tracks = 5; });
  reject_navigation(
      [](auto& s) { required(s.policy).desired.order = api::PlaybackOrder::AlbumOrder; });
  reject_navigation([](auto& s) { required(s.pending_intent).selection.reset(); });
  return suite.finish("Player state schema 2 contracts");
}
