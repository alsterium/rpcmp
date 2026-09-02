#ifndef RPCMP_RUNTIME_MDX_TIMELINE_HPP
#define RPCMP_RUNTIME_MDX_TIMELINE_HPP

#include "rpcmp/runtime/mdx_ym2151_router.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpcmp::runtime::mdx {

inline constexpr std::uint32_t kMdxOpmClockHz = 4'000'000;
inline constexpr std::uint8_t kMdxInitialTimerB = 0xc8;

struct MdxTimelineState {
  std::uint64_t scheduler_tick{};
  std::uint64_t remainder{};
  std::uint8_t timer_b{kMdxInitialTimerB};
};

struct TimedYm2151Write {
  std::uint64_t at_tick{};
  Ym2151Write write{};
};

struct TimedYm2151Batch {
  std::array<TimedYm2151Write, kMdxMaxYm2151WritesPerBatch> writes{};
  std::size_t count{};
};

struct MdxTimelineScratch {
  MdxTimelineState candidate_state{};
  TimedYm2151Batch pending_batch{};
};

// Stamps all writes at the current scheduler tick, applies the last Timer B
// command in service order, then advances one exact MDX driver interval while
// carrying the rational remainder. State and output commit together.
[[nodiscard]] DecodeResult stamp_ym2151_tick(const DocumentTickBatch& actions,
                                             const Ym2151WriteBatch& writes,
                                             std::uint32_t scheduler_tick_rate,
                                             MdxTimelineState& state, TimedYm2151Batch& timed,
                                             MdxTimelineScratch& scratch) noexcept;

} // namespace rpcmp::runtime::mdx

#endif // RPCMP_RUNTIME_MDX_TIMELINE_HPP
