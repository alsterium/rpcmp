#include "rpcmp/runtime/mdx_semantic.hpp"

namespace rpcmp::runtime::mdx {
namespace {

std::int16_t read_i16be(const ByteView source) noexcept {
  const auto bits = static_cast<std::uint16_t>((static_cast<std::uint16_t>(source.data[1]) << 8U) |
                                               source.data[2]);
  const auto value =
      bits <= 0x7fffU ? static_cast<std::int32_t>(bits) : static_cast<std::int32_t>(bits) - 0x10000;
  return static_cast<std::int16_t>(value);
}

} // namespace

DecodeResult decode_semantic(const ByteView source, const std::size_t source_offset,
                             const std::uint8_t logical_track,
                             SemanticInstruction& output) noexcept {
  InstructionView decoded{};
  const auto result = decode_instruction(source, source_offset, logical_track, decoded);
  if (!result.ok()) {
    return result;
  }

  SemanticInstruction candidate{};
  candidate.kind = decoded.kind;
  candidate.byte_offset = decoded.byte_offset;
  switch (decoded.kind) {
  case InstructionKind::Rest:
    candidate.duration_ticks = static_cast<std::uint16_t>(decoded.opcode) + 1U;
    break;
  case InstructionKind::Note:
    candidate.value = decoded.opcode;
    candidate.duration_ticks = static_cast<std::uint16_t>(decoded.source.data[1]) + 1U;
    break;
  case InstructionKind::DirectWrite:
    candidate.value = decoded.source.data[1];
    candidate.second_value = decoded.source.data[2];
    break;
  case InstructionKind::RepeatStart:
    candidate.value = decoded.source.data[1];
    break;
  case InstructionKind::RepeatEnd:
  case InstructionKind::RepeatEscape:
  case InstructionKind::Detune:
  case InstructionKind::Portamento:
  case InstructionKind::TrackLoop:
    candidate.signed_value = read_i16be(decoded.source);
    break;
  case InstructionKind::TrackEnd:
  case InstructionKind::SuppressKeyOff:
  case InstructionKind::WaitChannel:
    break;
  case InstructionKind::TimerB:
  case InstructionKind::SelectVoice:
  case InstructionKind::Pan:
  case InstructionKind::Volume:
  case InstructionKind::Gate:
  case InstructionKind::KeyOnDelay:
  case InstructionKind::ReleaseChannel:
    candidate.value = decoded.source.data[1];
    break;
  }

  output = candidate;
  return {};
}

} // namespace rpcmp::runtime::mdx
