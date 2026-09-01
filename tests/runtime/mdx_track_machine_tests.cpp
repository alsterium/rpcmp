#include "rpcmp/runtime/mdx_track_machine.hpp"
#include "test_support.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using rpcmp::runtime::mdx::DecodeError;
using rpcmp::runtime::mdx::InstructionKind;
using rpcmp::runtime::mdx::PlaybackLimits;
using rpcmp::runtime::mdx::TrackPlaybackState;
using rpcmp::runtime::mdx::TrackTarget;
using rpcmp::runtime::mdx::TrackTickScratch;
using rpcmp::runtime::mdx::TrackTickTrace;
using rpcmp::runtime::mdx::TrackView;

TrackView track(const std::vector<std::uint8_t>& bytes) {
  return {0, TrackTarget::Ym2151, 100, {bytes.data(), bytes.size()}};
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  const std::vector<std::uint8_t> linear{0xff, 0xc8, 0xfe, 0x20, 0xc7,
                                         0x01, 0x80, 0x00, 0xf1, 0x00};
  TrackPlaybackState state{};
  TrackTickTrace trace{};
  static TrackTickScratch scratch{};
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_track_tick(track(linear), state, trace, scratch).ok());
  RPCMP_CHECK(suite, trace.count == 3);
  RPCMP_CHECK(suite, trace.instructions[0].kind == InstructionKind::TimerB);
  RPCMP_CHECK(suite, trace.instructions[1].kind == InstructionKind::DirectWrite);
  RPCMP_CHECK(suite, trace.instructions[2].kind == InstructionKind::Rest);
  RPCMP_CHECK(suite, state.cursor == 6);
  RPCMP_CHECK(suite, state.remaining_ticks == 2);

  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_track_tick(track(linear), state, trace, scratch).ok());
  RPCMP_CHECK(suite, trace.count == 0);
  RPCMP_CHECK(suite, state.remaining_ticks == 1);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_track_tick(track(linear), state, trace, scratch).ok());
  RPCMP_CHECK(suite, trace.count == 1);
  RPCMP_CHECK(suite, trace.instructions[0].kind == InstructionKind::Note);
  RPCMP_CHECK(suite, state.remaining_ticks == 1);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_track_tick(track(linear), state, trace, scratch).ok());
  RPCMP_CHECK(suite, trace.count == 1);
  RPCMP_CHECK(suite, trace.instructions[0].kind == InstructionKind::TrackEnd);
  RPCMP_CHECK(suite, state.ended);

  const std::vector<std::uint8_t> repeated{0xf6, 0x02, 0x00, 0x00, 0xf4, 0x00,
                                           0x01, 0xf5, 0xff, 0xf9, 0xf1, 0x00};
  state = {};
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_track_tick(track(repeated), state, trace, scratch).ok());
  RPCMP_CHECK(suite, trace.count == 2);
  RPCMP_CHECK(suite, trace.instructions[1].kind == InstructionKind::Rest);
  RPCMP_CHECK(suite, state.repeat_depth == 1);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_track_tick(track(repeated), state, trace, scratch).ok());
  RPCMP_CHECK(suite, trace.count == 3);
  RPCMP_CHECK(suite, trace.instructions[0].kind == InstructionKind::RepeatEscape);
  RPCMP_CHECK(suite, trace.instructions[1].kind == InstructionKind::RepeatEnd);
  RPCMP_CHECK(suite, trace.instructions[2].kind == InstructionKind::Rest);
  RPCMP_CHECK(suite, state.repeat_depth == 1);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_track_tick(track(repeated), state, trace, scratch).ok());
  RPCMP_CHECK(suite, trace.count == 2);
  RPCMP_CHECK(suite, trace.instructions[0].kind == InstructionKind::RepeatEscape);
  RPCMP_CHECK(suite, trace.instructions[1].kind == InstructionKind::TrackEnd);
  RPCMP_CHECK(suite, state.repeat_depth == 0);
  RPCMP_CHECK(suite, state.ended);

  const std::vector<std::uint8_t> waiting{0xee, 0xf1, 0x00};
  state = {};
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_track_tick(track(waiting), state, trace, scratch).ok());
  RPCMP_CHECK(suite, state.waiting);
  RPCMP_CHECK(suite, trace.count == 1);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_track_tick(track(waiting), state, trace, scratch).ok());
  RPCMP_CHECK(suite, trace.count == 0);
  rpcmp::runtime::mdx::release_track_wait(state);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::advance_track_tick(track(waiting), state, trace, scratch).ok());
  RPCMP_CHECK(suite, state.ended);

  const std::vector<std::uint8_t> instant_loop{0xf1, 0xff, 0xfd};
  state = {};
  PlaybackLimits limits{};
  limits.max_instructions_per_tick = 4;
  limits.max_branches_per_tick = 4;
  const auto exhausted =
      rpcmp::runtime::mdx::advance_track_tick(track(instant_loop), state, trace, scratch, limits);
  RPCMP_CHECK(suite, exhausted.error == DecodeError::BudgetExhausted);
  RPCMP_CHECK(suite, state.cursor == 0);

  return suite.finish("mdx track playback state machine");
}
