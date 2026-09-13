#ifndef RPCMP_CONTRACTS_PERFORMANCE_HISTORY_V2_HPP
#define RPCMP_CONTRACTS_PERFORMANCE_HISTORY_V2_HPP

#include <array>
#include <cstdint>
#include <optional>

namespace rpcmp::contracts::v2 {

inline constexpr std::uint64_t kPerformanceHistory = 1ULL << 6U;
inline constexpr std::uint16_t kPerformanceCapacity = 256;
inline constexpr std::uint16_t kPerformanceChannels = 8;

struct MdxFmV1 {
  std::optional<std::uint8_t> voice_number;
};
[[nodiscard]] constexpr bool operator==(const MdxFmV1& a, const MdxFmV1& b) noexcept {
  return a.voice_number == b.voice_number;
}
struct PerformanceChannel {
  std::uint16_t channel_id{};
  std::optional<bool> key_on;
  std::optional<std::uint8_t> note;
  std::optional<std::int16_t> fine_pitch_cents;
  std::optional<MdxFmV1> mdx_fm;
};
[[nodiscard]] constexpr bool operator==(const PerformanceChannel& a,
                                        const PerformanceChannel& b) noexcept {
  return a.channel_id == b.channel_id && a.key_on == b.key_on && a.note == b.note &&
         a.fine_pitch_cents == b.fine_pitch_cents && a.mdx_fm == b.mdx_fm;
}
using PerformanceChannels = std::array<PerformanceChannel, kPerformanceChannels>;

enum class PerformanceKind : std::uint8_t { KeyOn, KeyOff, PitchChanged, InstrumentChanged };
struct PerformanceChange {
  std::uint64_t at_frame{};
  PerformanceChannel channel{};
  PerformanceKind kind{PerformanceKind::KeyOn};
};
[[nodiscard]] constexpr bool operator==(const PerformanceChange& a,
                                        const PerformanceChange& b) noexcept {
  return a.at_frame == b.at_frame && a.channel == b.channel && a.kind == b.kind;
}
struct PerformanceEvent {
  std::uint64_t sequence{};
  PerformanceChange change{};
};
[[nodiscard]] constexpr bool operator==(const PerformanceEvent& a,
                                        const PerformanceEvent& b) noexcept {
  return a.sequence == b.sequence && a.change == b.change;
}
enum class PerformanceAvailability : std::uint8_t {
  Available,
  Degraded,
  Exhausted,
  Waiting,
  Invalid
};
struct PerformanceHistorySnapshot {
  std::uint16_t version{1};
  std::uint64_t play_generation{};
  std::uint64_t observed_through_frame{};
  std::uint64_t next_sequence{1};
  PerformanceChannels channels{};
  std::array<PerformanceEvent, kPerformanceCapacity> events{};
  std::uint16_t count{};
  bool retention_lost{};
  bool capture_lost{};
  PerformanceAvailability availability{PerformanceAvailability::Waiting};
};

[[nodiscard]] PerformanceChannels unknown_performance_channels() noexcept;
[[nodiscard]] bool valid_performance_channel(const PerformanceChannel& channel) noexcept;
[[nodiscard]] bool valid_performance_change(const PerformanceChange& change) noexcept;
[[nodiscard]] bool valid_performance_history(const PerformanceHistorySnapshot& history) noexcept;

} // namespace rpcmp::contracts::v2

#endif // RPCMP_CONTRACTS_PERFORMANCE_HISTORY_V2_HPP
