#ifndef RPCMP_CONTRACTS_PLAYBACK_POLICY_V2_HPP
#define RPCMP_CONTRACTS_PLAYBACK_POLICY_V2_HPP

#include <cstdint>
#include <optional>

namespace rpcmp::contracts::v2 {

inline constexpr std::uint64_t kPolicyObservations = 1ULL << 3U;
inline constexpr std::uint64_t kRepeatControl = 1ULL << 4U;
enum class PlaybackOrder : std::uint8_t { AlbumOrder, ShuffleLibrary };
enum class RepeatMode : std::uint8_t { Default, RepeatOne, Counted };
struct PlaybackPolicy {
  PlaybackOrder order{PlaybackOrder::AlbumOrder};
  RepeatMode repeat{RepeatMode::Default};
  std::optional<std::uint32_t> count;
};
[[nodiscard]] constexpr bool operator==(const PlaybackPolicy& a, const PlaybackPolicy& b) noexcept {
  return a.order == b.order && a.repeat == b.repeat && a.count == b.count;
}
[[nodiscard]] constexpr bool valid_playback_policy(const PlaybackPolicy& policy) noexcept {
  return static_cast<std::uint8_t>(policy.order) <=
             static_cast<std::uint8_t>(PlaybackOrder::ShuffleLibrary) &&
         static_cast<std::uint8_t>(policy.repeat) <=
             static_cast<std::uint8_t>(RepeatMode::Counted) &&
         (policy.repeat == RepeatMode::Counted) == policy.count.has_value() &&
         (!policy.count || *policy.count != 0);
}
struct RepeatApplication {
  std::uint64_t revision{};
  std::optional<std::uint32_t> target;
};
[[nodiscard]] constexpr bool operator==(const RepeatApplication& a,
                                        const RepeatApplication& b) noexcept {
  return a.revision == b.revision && a.target == b.target;
}
[[nodiscard]] constexpr bool operator!=(const RepeatApplication& a,
                                        const RepeatApplication& b) noexcept {
  return !(a == b);
}
[[nodiscard]] constexpr bool valid_repeat_application(const RepeatApplication& value) noexcept {
  return value.revision != 0 && (!value.target || *value.target != 0);
}
// Call only after validating the policy. An invalid policy is never an unlimited target.
[[nodiscard]] constexpr RepeatApplication repeat_application(const PlaybackPolicy& policy,
                                                             std::uint64_t revision) noexcept {
  return {revision,
          policy.repeat == RepeatMode::Default ? std::optional<std::uint32_t>{2} : policy.count};
}
enum class PlaybackPhase : std::uint8_t { Steady, Fading, RestoringGain };
enum class PlaybackEndReason : std::uint8_t { None, NaturalEnd, RepeatOne, LoopLimit };
struct PlaybackMediaObservation {
  std::uint64_t play_generation{};
  std::uint64_t position_frames{};
  RepeatApplication applied{};
  std::optional<std::uint32_t> completed_loops;
  bool loop_count_overflow{};
  PlaybackPhase phase{PlaybackPhase::Steady};
  std::uint32_t gain{240'000};
  std::uint32_t ramp_elapsed{};
  std::uint32_t ramp_duration{};
  PlaybackEndReason end{PlaybackEndReason::None};
};
struct PlaybackPolicyObservation {
  PlaybackPolicy desired{};
  std::uint64_t revision{1};
  std::optional<std::uint64_t> last_command_id;
  std::optional<PlaybackMediaObservation> media;
};
[[nodiscard]] bool valid_media_observation(const PlaybackMediaObservation& media) noexcept;

} // namespace rpcmp::contracts::v2

#endif // RPCMP_CONTRACTS_PLAYBACK_POLICY_V2_HPP
