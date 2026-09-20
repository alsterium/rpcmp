#ifndef RPCMP_PLAYER_MDX_PERFORMANCE_MAPPER_HPP
#define RPCMP_PLAYER_MDX_PERFORMANCE_MAPPER_HPP

#include "rpcmp/player/performance_history.hpp"
#include "rpcmp/runtime/mdx_performance.hpp"

namespace rpcmp::player {

struct MdxPerformanceFrames {
  std::array<std::uint64_t, contracts::v2::kPerformanceCapacity> frames{};
  std::uint16_t count{};
};
struct MdxPerformanceBoundary {
  std::uint64_t source_tick{};
  // Final checkpoint; also the common event frame when event_frames is absent.
  std::uint64_t at_frame{};
  std::uint64_t through_frame{};
  std::optional<MdxPerformanceFrames> event_frames{std::nullopt};
  std::uint64_t omitted_before{};
  PerformanceLosses omitted_before_events{};
  std::uint64_t omitted_after{};
  bool capture_lost{};
};
enum class MdxPerformanceResult : std::uint8_t { Applied, Future, Stale, Invalid, Exhausted };

// The audio adapter supplies the mapping only after physical output commits.
class MdxPerformanceMapper {
public:
  MdxPerformanceMapper(PerformanceHistory& history, std::uint32_t chip_clock_hz) noexcept
      : history_(history), chip_clock_hz_(chip_clock_hz) {}
  [[nodiscard]] MdxPerformanceResult commit(std::uint64_t play_generation,
                                            const runtime::mdx::SequencedPerformance& source,
                                            const MdxPerformanceBoundary& boundary) noexcept;

private:
  [[nodiscard]] contracts::v2::PerformanceChannel
  channel(const runtime::mdx::Ym2151ObservedChannel& input, std::uint8_t id) const noexcept;
  PerformanceHistory& history_;
  std::uint32_t chip_clock_hz_{};
  contracts::v2::PerformanceChannels channels_{};
  std::array<contracts::v2::PerformanceChange, contracts::v2::kPerformanceCapacity> changes_{};
};

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_MDX_PERFORMANCE_MAPPER_HPP
