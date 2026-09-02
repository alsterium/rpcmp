#include "rpcmp/runtime/mdx_timeline.hpp"

#include <limits>

namespace rpcmp::runtime::mdx {

DecodeResult stamp_ym2151_tick(const DocumentTickBatch& actions, const Ym2151WriteBatch& writes,
                               const std::uint32_t scheduler_tick_rate, MdxTimelineState& state,
                               TimedYm2151Batch& timed, MdxTimelineScratch& scratch) noexcept {
  if (scheduler_tick_rate == 0 || actions.count > actions.actions.size() ||
      writes.count > writes.writes.size()) {
    return {DecodeError::InvalidLimits, 0, 0xff};
  }

  scratch.candidate_state = state;
  scratch.pending_batch.count = writes.count;
  for (std::size_t index = 0; index < writes.count; ++index) {
    scratch.pending_batch.writes[index] = {state.scheduler_tick, writes.writes[index]};
  }
  for (std::size_t index = 0; index < actions.count; ++index) {
    if (actions.actions[index].instruction.kind == InstructionKind::TimerB) {
      scratch.candidate_state.timer_b = actions.actions[index].instruction.value;
    }
  }

  const auto timer_span = static_cast<std::uint64_t>(256U - scratch.candidate_state.timer_b);
  const auto numerator =
      timer_span * 1'024U * scheduler_tick_rate + scratch.candidate_state.remainder;
  const auto elapsed = numerator / kMdxOpmClockHz;
  scratch.candidate_state.remainder = numerator % kMdxOpmClockHz;
  if (elapsed > std::numeric_limits<std::uint64_t>::max() - state.scheduler_tick) {
    return {DecodeError::BudgetExhausted, 0, 0xff};
  }
  scratch.candidate_state.scheduler_tick = state.scheduler_tick + elapsed;

  state = scratch.candidate_state;
  timed = scratch.pending_batch;
  return {};
}

} // namespace rpcmp::runtime::mdx
