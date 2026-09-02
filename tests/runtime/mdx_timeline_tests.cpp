#include "rpcmp/runtime/mdx_timeline.hpp"
#include "test_support.hpp"

#include <limits>

int main() {
  rpcmp::test::Suite suite;
  rpcmp::runtime::mdx::DocumentTickBatch actions{};
  rpcmp::runtime::mdx::Ym2151WriteBatch writes{};
  writes.writes[writes.count++] = {0x20, 0xc0, 0};
  writes.writes[writes.count++] = {0x08, 0x78, 0};
  rpcmp::runtime::mdx::MdxTimelineState state{};
  rpcmp::runtime::mdx::TimedYm2151Batch timed{};
  static rpcmp::runtime::mdx::MdxTimelineScratch scratch{};

  RPCMP_CHECK(
      suite,
      rpcmp::runtime::mdx::stamp_ym2151_tick(actions, writes, 48'000, state, timed, scratch).ok());
  RPCMP_CHECK(suite, timed.count == 2);
  RPCMP_CHECK(suite, timed.writes[0].at_tick == 0);
  RPCMP_CHECK(suite, timed.writes[1].at_tick == 0);
  RPCMP_CHECK(suite, state.scheduler_tick == 688);
  RPCMP_CHECK(suite, state.remainder == 512'000);

  RPCMP_CHECK(
      suite,
      rpcmp::runtime::mdx::stamp_ym2151_tick(actions, writes, 48'000, state, timed, scratch).ok());
  RPCMP_CHECK(suite, timed.writes[0].at_tick == 688);
  RPCMP_CHECK(suite, state.scheduler_tick == 1'376);
  RPCMP_CHECK(suite, state.remainder == 1'024'000);

  actions.actions[actions.count++] = {
      3, {rpcmp::runtime::mdx::InstructionKind::TimerB, 50, 0, 0, 0xff, 0}};
  state = {};
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::stamp_ym2151_tick(actions, writes, 1'000'000, state,
                                                            timed, scratch)
                         .ok());
  RPCMP_CHECK(suite, timed.writes[0].at_tick == 0);
  RPCMP_CHECK(suite, state.timer_b == 0xff);
  RPCMP_CHECK(suite, state.scheduler_tick == 256);
  RPCMP_CHECK(suite, state.remainder == 0);

  const auto before = state;
  timed.count = 1;
  timed.writes[0].at_tick = 99;
  const auto invalid =
      rpcmp::runtime::mdx::stamp_ym2151_tick(actions, writes, 0, state, timed, scratch);
  RPCMP_CHECK(suite, invalid.error == rpcmp::runtime::mdx::DecodeError::InvalidLimits);
  RPCMP_CHECK(suite, state.scheduler_tick == before.scheduler_tick);
  RPCMP_CHECK(suite, timed.count == 1);
  RPCMP_CHECK(suite, timed.writes[0].at_tick == 99);

  state.scheduler_tick = std::numeric_limits<std::uint64_t>::max();
  state.remainder = 0;
  const auto overflow =
      rpcmp::runtime::mdx::stamp_ym2151_tick(actions, writes, 48'000, state, timed, scratch);
  RPCMP_CHECK(suite, overflow.error == rpcmp::runtime::mdx::DecodeError::ArithmeticOverflow);
  RPCMP_CHECK(suite, state.scheduler_tick == std::numeric_limits<std::uint64_t>::max());
  RPCMP_CHECK(suite, state.remainder == 0);

  return suite.finish("mdx rational timeline");
}
