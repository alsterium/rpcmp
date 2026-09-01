#include "rpcmp/runtime/mdx_track_machine.hpp"

namespace rpcmp::runtime::mdx {
namespace {

DecodeResult failure(const DecodeError error, const std::size_t offset,
                     const std::uint8_t track) noexcept {
  return {error, offset, track};
}

bool valid_limits(const PlaybackLimits& limits) noexcept {
  return limits.max_instructions_per_tick != 0 &&
         limits.max_instructions_per_tick <= kMdxMaxInstructionsPerTick &&
         limits.max_branches_per_tick != 0 &&
         limits.max_branches_per_tick <= kMdxMaxBranchesPerTick && limits.max_repeat_frames != 0 &&
         limits.max_repeat_frames <= kMdxMaxRepeatFrames;
}

bool relative_target(const std::size_t after, const std::int16_t relative, const std::size_t size,
                     std::size_t& target) noexcept {
  if (relative < 0) {
    const auto magnitude = static_cast<std::size_t>(-static_cast<std::int32_t>(relative));
    if (magnitude > after) {
      return false;
    }
    target = after - magnitude;
    return true;
  }
  const auto magnitude = static_cast<std::size_t>(relative);
  if (magnitude > size - after) {
    return false;
  }
  target = after + magnitude;
  return true;
}

} // namespace

DecodeResult advance_track_tick(const TrackView& track, TrackPlaybackState& state,
                                TrackTickTrace& trace, TrackTickScratch& scratch,
                                const PlaybackLimits limits) noexcept {
  if (!valid_limits(limits)) {
    return failure(DecodeError::InvalidLimits, track.source_offset, track.logical_channel);
  }
  if ((track.source.data == nullptr && track.source.size != 0) ||
      state.cursor > track.source.size || state.repeat_depth > limits.max_repeat_frames) {
    return failure(DecodeError::RangeOutsideInput, track.source_offset, track.logical_channel);
  }

  TrackPlaybackState candidate = state;
  TrackTickTrace& candidate_trace = scratch.pending;
  candidate_trace.count = 0;
  if (candidate.ended || candidate.waiting) {
    trace = candidate_trace;
    return {};
  }
  if (candidate.remaining_ticks != 0) {
    --candidate.remaining_ticks;
    if (candidate.remaining_ticks != 0) {
      state = candidate;
      trace = candidate_trace;
      return {};
    }
  }

  std::size_t branches = 0;
  while (candidate.cursor < track.source.size) {
    if (candidate_trace.count == limits.max_instructions_per_tick) {
      return failure(DecodeError::BudgetExhausted, track.source_offset + candidate.cursor,
                     track.logical_channel);
    }
    SemanticInstruction instruction{};
    const auto decoded = decode_semantic(
        {track.source.data + candidate.cursor, track.source.size - candidate.cursor},
        track.source_offset + candidate.cursor, track.logical_channel, instruction);
    if (!decoded.ok()) {
      return decoded;
    }
    InstructionView view{};
    const auto boundary = decode_instruction(
        {track.source.data + candidate.cursor, track.source.size - candidate.cursor},
        track.source_offset + candidate.cursor, track.logical_channel, view);
    if (!boundary.ok()) {
      return boundary;
    }
    candidate_trace.instructions[candidate_trace.count++] = instruction;
    const std::size_t after = candidate.cursor + view.source.size;
    candidate.cursor = after;

    switch (instruction.kind) {
    case InstructionKind::Rest:
    case InstructionKind::Note:
      candidate.remaining_ticks = instruction.duration_ticks;
      state = candidate;
      trace = candidate_trace;
      return {};
    case InstructionKind::RepeatStart:
      if (candidate.repeat_depth == limits.max_repeat_frames) {
        return failure(DecodeError::BudgetExhausted, instruction.byte_offset,
                       track.logical_channel);
      }
      candidate.repeats[candidate.repeat_depth++] = {
          after, instruction.value == 0 ? static_cast<std::uint16_t>(256U)
                                        : static_cast<std::uint16_t>(instruction.value)};
      break;
    case InstructionKind::RepeatEnd: {
      if (candidate.repeat_depth == 0) {
        return failure(DecodeError::InvalidRepeat, instruction.byte_offset, track.logical_channel);
      }
      std::size_t target = 0;
      if (!relative_target(after, instruction.signed_value, track.source.size, target) ||
          target != candidate.repeats[candidate.repeat_depth - 1].body_offset) {
        return failure(DecodeError::InvalidRepeat, instruction.byte_offset, track.logical_channel);
      }
      RepeatFrame& frame = candidate.repeats[candidate.repeat_depth - 1];
      --frame.remaining;
      if (frame.remaining != 0) {
        if (branches == limits.max_branches_per_tick) {
          return failure(DecodeError::BudgetExhausted, instruction.byte_offset,
                         track.logical_channel);
        }
        ++branches;
        candidate.cursor = target;
      } else {
        --candidate.repeat_depth;
      }
      break;
    }
    case InstructionKind::RepeatEscape: {
      if (candidate.repeat_depth == 0) {
        return failure(DecodeError::InvalidRepeat, instruction.byte_offset, track.logical_channel);
      }
      if (candidate.repeats[candidate.repeat_depth - 1].remaining == 1) {
        std::size_t operand = 0;
        if (!relative_target(after, instruction.signed_value, track.source.size, operand) ||
            operand > track.source.size - 2) {
          return failure(DecodeError::InvalidRepeat, instruction.byte_offset,
                         track.logical_channel);
        }
        candidate.cursor = operand + 2;
        --candidate.repeat_depth;
        if (branches == limits.max_branches_per_tick) {
          return failure(DecodeError::BudgetExhausted, instruction.byte_offset,
                         track.logical_channel);
        }
        ++branches;
      }
      break;
    }
    case InstructionKind::TrackLoop: {
      std::size_t target = 0;
      if (!relative_target(after, instruction.signed_value, track.source.size, target) ||
          target >= track.source.size) {
        return failure(DecodeError::InvalidBranchTarget, instruction.byte_offset,
                       track.logical_channel);
      }
      if (branches == limits.max_branches_per_tick) {
        return failure(DecodeError::BudgetExhausted, instruction.byte_offset,
                       track.logical_channel);
      }
      ++branches;
      candidate.cursor = target;
      break;
    }
    case InstructionKind::TrackEnd:
      candidate.ended = true;
      state = candidate;
      trace = candidate_trace;
      return {};
    case InstructionKind::WaitChannel:
      candidate.waiting = true;
      state = candidate;
      trace = candidate_trace;
      return {};
    case InstructionKind::TimerB:
    case InstructionKind::DirectWrite:
    case InstructionKind::SelectVoice:
    case InstructionKind::Pan:
    case InstructionKind::Volume:
    case InstructionKind::Gate:
    case InstructionKind::SuppressKeyOff:
    case InstructionKind::Detune:
    case InstructionKind::Portamento:
    case InstructionKind::KeyOnDelay:
    case InstructionKind::ReleaseChannel:
      break;
    }
  }
  return failure(DecodeError::TruncatedInstruction, track.source_offset + track.source.size,
                 track.logical_channel);
}

void release_track_wait(TrackPlaybackState& state) noexcept { state.waiting = false; }

} // namespace rpcmp::runtime::mdx
