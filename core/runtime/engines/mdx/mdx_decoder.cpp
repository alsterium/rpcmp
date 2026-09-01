#include "rpcmp/runtime/mdx_decoder.hpp"

namespace rpcmp::runtime::mdx {
namespace {

DecodeResult failure(const DecodeError error, const std::size_t offset,
                     const std::uint8_t track) noexcept {
  return {error, offset, track};
}

DecodeResult admitted(const ByteView source, const std::size_t source_offset,
                      const std::uint8_t track, const InstructionKind kind,
                      const std::size_t length, InstructionView& output) noexcept {
  if ((source.data == nullptr && source.size != 0) || source.size < length) {
    return failure(source.data == nullptr && source.size != 0 ? DecodeError::RangeOutsideInput
                                                              : DecodeError::TruncatedInstruction,
                   source_offset, track);
  }
  output = {kind, source.data[0], source_offset, {source.data, length}};
  return {};
}

void set_boundary(ControlFlowScratch& scratch, const std::size_t offset) noexcept {
  scratch.instruction_starts[offset / 8U] |= static_cast<std::uint8_t>(1U << (offset % 8U));
}

bool is_boundary(const ControlFlowScratch& scratch, const std::size_t offset) noexcept {
  return (scratch.instruction_starts[offset / 8U] &
          static_cast<std::uint8_t>(1U << (offset % 8U))) != 0;
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

std::int16_t read_i16be(const std::uint8_t* bytes) noexcept {
  const auto bits =
      static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[0]) << 8U) | bytes[1]);
  const auto value =
      bits <= 0x7fffU ? static_cast<std::int32_t>(bits) : static_cast<std::int32_t>(bits) - 0x10000;
  return static_cast<std::int16_t>(value);
}

} // namespace

DecodeResult decode_instruction(const ByteView source, const std::size_t source_offset,
                                const std::uint8_t logical_track,
                                InstructionView& output) noexcept {
  if (source.data == nullptr || source.size == 0) {
    return failure(source.data == nullptr && source.size != 0 ? DecodeError::RangeOutsideInput
                                                              : DecodeError::TruncatedInstruction,
                   source_offset, logical_track);
  }

  const std::uint8_t opcode = source.data[0];
  if (opcode <= 0x7fU) {
    return admitted(source, source_offset, logical_track, InstructionKind::Rest, 1, output);
  }
  if (opcode <= 0xdfU) {
    return admitted(source, source_offset, logical_track, InstructionKind::Note, 2, output);
  }

  switch (opcode) {
  case 0xff:
    return admitted(source, source_offset, logical_track, InstructionKind::TimerB, 2, output);
  case 0xfe:
    return admitted(source, source_offset, logical_track, InstructionKind::DirectWrite, 3, output);
  case 0xfd:
    return admitted(source, source_offset, logical_track, InstructionKind::SelectVoice, 2, output);
  case 0xfc:
    return admitted(source, source_offset, logical_track, InstructionKind::Pan, 2, output);
  case 0xfb:
    return admitted(source, source_offset, logical_track, InstructionKind::Volume, 2, output);
  case 0xf8:
    return admitted(source, source_offset, logical_track, InstructionKind::Gate, 2, output);
  case 0xf7:
    return admitted(source, source_offset, logical_track, InstructionKind::SuppressKeyOff, 1,
                    output);
  case 0xf6:
    if (source.size < 3) {
      return failure(DecodeError::TruncatedInstruction, source_offset, logical_track);
    }
    if (source.data[2] != 0) {
      return failure(DecodeError::InvalidRepeat, source_offset, logical_track);
    }
    return admitted(source, source_offset, logical_track, InstructionKind::RepeatStart, 3, output);
  case 0xf5:
    return admitted(source, source_offset, logical_track, InstructionKind::RepeatEnd, 3, output);
  case 0xf4:
    return admitted(source, source_offset, logical_track, InstructionKind::RepeatEscape, 3, output);
  case 0xf3:
    return admitted(source, source_offset, logical_track, InstructionKind::Detune, 3, output);
  case 0xf2:
    return admitted(source, source_offset, logical_track, InstructionKind::Portamento, 3, output);
  case 0xf1:
    if (source.size < 2) {
      return failure(DecodeError::TruncatedInstruction, source_offset, logical_track);
    }
    return admitted(source, source_offset, logical_track,
                    source.data[1] == 0 ? InstructionKind::TrackEnd : InstructionKind::TrackLoop,
                    source.data[1] == 0 ? 2 : 3, output);
  case 0xf0:
    return admitted(source, source_offset, logical_track, InstructionKind::KeyOnDelay, 2, output);
  case 0xef:
    if (source.size < 2) {
      return failure(DecodeError::TruncatedInstruction, source_offset, logical_track);
    }
    if (source.data[1] >= kMdxTrackCount) {
      return failure(DecodeError::InvalidSyncChannel, source_offset, logical_track);
    }
    return admitted(source, source_offset, logical_track, InstructionKind::ReleaseChannel, 2,
                    output);
  case 0xee:
    return admitted(source, source_offset, logical_track, InstructionKind::WaitChannel, 1, output);
  case 0xfa:
  case 0xf9:
  case 0xed:
  case 0xec:
  case 0xeb:
  case 0xea:
  case 0xe9:
    return failure(DecodeError::UnsupportedOpcode, source_offset, logical_track);
  case 0xe8:
    return failure(DecodeError::UnsupportedPcm, source_offset, logical_track);
  case 0xe7:
  case 0xe6:
  case 0xe5:
  case 0xe4:
  case 0xe3:
  case 0xe2:
  case 0xe1:
  case 0xe0:
    return failure(DecodeError::UnsupportedExtension, source_offset, logical_track);
  default:
    return failure(DecodeError::UnsupportedOpcode, source_offset, logical_track);
  }
}

DecodeResult validate_track(const TrackView& track, TrackValidation& output,
                            const DecodeLimits limits) noexcept {
  if (limits.max_instructions == 0 || limits.max_instructions > kMdxMaxDecodedInstructions) {
    return failure(DecodeError::InvalidLimits, track.source_offset, track.logical_channel);
  }
  if (track.source.data == nullptr && track.source.size != 0) {
    return failure(DecodeError::RangeOutsideInput, track.source_offset, track.logical_channel);
  }

  TrackValidation candidate{};
  std::size_t cursor = 0;
  while (cursor < track.source.size) {
    if (candidate.instruction_count == limits.max_instructions) {
      return failure(DecodeError::BudgetExhausted, track.source_offset + cursor,
                     track.logical_channel);
    }
    InstructionView instruction{};
    const auto result =
        decode_instruction({track.source.data + cursor, track.source.size - cursor},
                           track.source_offset + cursor, track.logical_channel, instruction);
    if (!result.ok()) {
      return result;
    }
    ++candidate.instruction_count;
    if (instruction.kind == InstructionKind::TrackEnd) {
      candidate.terminal_byte_offset = instruction.byte_offset;
      output = candidate;
      return {};
    }
    cursor += instruction.source.size;
  }
  return failure(DecodeError::TruncatedInstruction, track.source_offset + track.source.size,
                 track.logical_channel);
}

DecodeResult validate_document(const MdxDocument& document, DocumentValidation& output,
                               const DecodeLimits limits) noexcept {
  if (limits.max_instructions == 0 || limits.max_instructions > kMdxMaxDecodedInstructions) {
    return failure(DecodeError::InvalidLimits, 0, 0xff);
  }
  DocumentValidation candidate{};
  for (std::size_t index = 0; index < document.tracks.size(); ++index) {
    const TrackView& track = document.tracks[index];
    if (track.source.data == nullptr && track.source.size != 0) {
      return failure(DecodeError::RangeOutsideInput, track.source_offset, track.logical_channel);
    }
    if (index == kMdxTrackCount - 1) {
      std::size_t cursor = 0;
      std::size_t instruction_count = 0;
      while (cursor < track.source.size) {
        if (instruction_count == limits.max_instructions) {
          return failure(DecodeError::BudgetExhausted, track.source_offset + cursor,
                         track.logical_channel);
        }
        InstructionView instruction{};
        const auto decoded =
            decode_instruction({track.source.data + cursor, track.source.size - cursor},
                               track.source_offset + cursor, track.logical_channel, instruction);
        if (!decoded.ok()) {
          return decoded.error == DecodeError::TruncatedInstruction
                     ? decoded
                     : failure(DecodeError::UnsupportedPcm, decoded.byte_offset,
                               track.logical_channel);
        }
        if (instruction.kind != InstructionKind::Rest &&
            instruction.kind != InstructionKind::TrackEnd) {
          return failure(DecodeError::UnsupportedPcm, instruction.byte_offset,
                         track.logical_channel);
        }
        if (instruction.kind == InstructionKind::TrackEnd) {
          break;
        }
        ++instruction_count;
        cursor += instruction.source.size;
      }
    }
    const auto result = validate_track(track, candidate.tracks[index], limits);
    if (!result.ok()) {
      return result;
    }
  }
  output = candidate;
  return {};
}

DecodeResult validate_control_flow(const TrackView& track, ControlFlowScratch& scratch,
                                   const DecodeLimits limits) noexcept {
  if (limits.max_instructions == 0 || limits.max_instructions > kMdxMaxDecodedInstructions) {
    return failure(DecodeError::InvalidLimits, track.source_offset, track.logical_channel);
  }
  if ((track.source.data == nullptr && track.source.size != 0) ||
      track.source.size > kMdxMaxInputBytes) {
    return failure(DecodeError::RangeOutsideInput, track.source_offset, track.logical_channel);
  }

  const std::size_t bitmap_bytes = (track.source.size + 7U) / 8U;
  for (std::size_t index = 0; index < bitmap_bytes; ++index) {
    scratch.instruction_starts[index] = 0;
  }

  std::size_t cursor = 0;
  std::size_t instruction_count = 0;
  bool found_terminal = false;
  while (cursor < track.source.size) {
    if (instruction_count == limits.max_instructions) {
      return failure(DecodeError::BudgetExhausted, track.source_offset + cursor,
                     track.logical_channel);
    }
    set_boundary(scratch, cursor);
    InstructionView instruction{};
    const auto decoded =
        decode_instruction({track.source.data + cursor, track.source.size - cursor},
                           track.source_offset + cursor, track.logical_channel, instruction);
    if (!decoded.ok()) {
      return decoded;
    }
    ++instruction_count;
    cursor += instruction.source.size;
    if (instruction.kind == InstructionKind::TrackEnd) {
      found_terminal = true;
      break;
    }
  }
  if (!found_terminal) {
    return failure(DecodeError::TruncatedInstruction, track.source_offset + track.source.size,
                   track.logical_channel);
  }

  cursor = 0;
  while (cursor < track.source.size) {
    InstructionView instruction{};
    const auto decoded =
        decode_instruction({track.source.data + cursor, track.source.size - cursor},
                           track.source_offset + cursor, track.logical_channel, instruction);
    if (!decoded.ok()) {
      return decoded;
    }
    const std::size_t after = cursor + instruction.source.size;
    if (instruction.kind == InstructionKind::TrackLoop ||
        instruction.kind == InstructionKind::RepeatEnd) {
      std::size_t target = 0;
      if (!relative_target(after, read_i16be(instruction.source.data + 1), track.source.size,
                           target) ||
          target >= track.source.size || !is_boundary(scratch, target)) {
        return failure(DecodeError::InvalidBranchTarget, instruction.byte_offset,
                       track.logical_channel);
      }
      if (instruction.kind == InstructionKind::RepeatEnd &&
          (target < 3 || track.source.data[target - 3] != 0xf6 ||
           track.source.data[target - 1] != 0x00)) {
        return failure(DecodeError::InvalidRepeat, instruction.byte_offset, track.logical_channel);
      }
    } else if (instruction.kind == InstructionKind::RepeatEscape) {
      std::size_t operand = 0;
      if (!relative_target(after, read_i16be(instruction.source.data + 1), track.source.size,
                           operand) ||
          operand == 0 || operand > track.source.size - 2 ||
          track.source.data[operand - 1] != 0xf5 || !is_boundary(scratch, operand - 1)) {
        return failure(DecodeError::InvalidRepeat, instruction.byte_offset, track.logical_channel);
      }
    }
    cursor = after;
    if (instruction.kind == InstructionKind::TrackEnd) {
      return {};
    }
  }
  return failure(DecodeError::TruncatedInstruction, track.source_offset + track.source.size,
                 track.logical_channel);
}

} // namespace rpcmp::runtime::mdx
