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

} // namespace rpcmp::runtime::mdx
