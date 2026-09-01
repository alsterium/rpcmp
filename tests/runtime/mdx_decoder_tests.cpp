#include "rpcmp/runtime/mdx_decoder.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using rpcmp::runtime::mdx::ByteView;
using rpcmp::runtime::mdx::DecodeError;
using rpcmp::runtime::mdx::DecodeLimits;
using rpcmp::runtime::mdx::DocumentValidation;
using rpcmp::runtime::mdx::InstructionKind;
using rpcmp::runtime::mdx::InstructionView;
using rpcmp::runtime::mdx::MdxDocument;
using rpcmp::runtime::mdx::TrackTarget;
using rpcmp::runtime::mdx::TrackValidation;
using rpcmp::runtime::mdx::TrackView;

struct Case {
  std::array<std::uint8_t, 3> bytes{};
  std::size_t length{};
  InstructionKind kind{InstructionKind::Rest};
};

ByteView view(const std::vector<std::uint8_t>& bytes) { return {bytes.data(), bytes.size()}; }

TrackView track(const std::vector<std::uint8_t>& bytes, const std::uint8_t channel = 0,
                const std::size_t offset = 100) {
  return {channel, channel < 8 ? TrackTarget::Ym2151 : TrackTarget::LegacyAdpcm, offset,
          view(bytes)};
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  constexpr std::array<Case, 19> cases{{
      {{{0x00, 0, 0}}, 1, InstructionKind::Rest},
      {{{0x80, 0x03, 0}}, 2, InstructionKind::Note},
      {{{0xff, 0xc8, 0}}, 2, InstructionKind::TimerB},
      {{{0xfe, 0x20, 0x7f}}, 3, InstructionKind::DirectWrite},
      {{{0xfd, 0x01, 0}}, 2, InstructionKind::SelectVoice},
      {{{0xfc, 0x03, 0}}, 2, InstructionKind::Pan},
      {{{0xfb, 0x0f, 0}}, 2, InstructionKind::Volume},
      {{{0xf8, 0x08, 0}}, 2, InstructionKind::Gate},
      {{{0xf7, 0, 0}}, 1, InstructionKind::SuppressKeyOff},
      {{{0xf6, 0x02, 0x00}}, 3, InstructionKind::RepeatStart},
      {{{0xf5, 0xff, 0xf0}}, 3, InstructionKind::RepeatEnd},
      {{{0xf4, 0x00, 0x04}}, 3, InstructionKind::RepeatEscape},
      {{{0xf3, 0xff, 0xff}}, 3, InstructionKind::Detune},
      {{{0xf2, 0x00, 0x01}}, 3, InstructionKind::Portamento},
      {{{0xf1, 0x00, 0}}, 2, InstructionKind::TrackEnd},
      {{{0xf1, 0xff, 0xf0}}, 3, InstructionKind::TrackLoop},
      {{{0xf0, 0x02, 0}}, 2, InstructionKind::KeyOnDelay},
      {{{0xef, 0x03, 0}}, 2, InstructionKind::ReleaseChannel},
      {{{0xee, 0, 0}}, 1, InstructionKind::WaitChannel},
  }};

  for (const auto& item : cases) {
    InstructionView decoded{};
    const auto result =
        rpcmp::runtime::mdx::decode_instruction({item.bytes.data(), item.length}, 500, 3, decoded);
    RPCMP_CHECK(suite, result.ok());
    RPCMP_CHECK(suite, decoded.kind == item.kind);
    RPCMP_CHECK(suite, decoded.opcode == item.bytes[0]);
    RPCMP_CHECK(suite, decoded.byte_offset == 500);
    RPCMP_CHECK(suite, decoded.source.size == item.length);
    if (item.length > 1) {
      InstructionView unchanged{};
      unchanged.byte_offset = 77;
      const auto truncated = rpcmp::runtime::mdx::decode_instruction(
          {item.bytes.data(), item.length - 1}, 501, 4, unchanged);
      RPCMP_CHECK(suite, truncated.error == DecodeError::TruncatedInstruction);
      RPCMP_CHECK(suite, truncated.byte_offset == 501);
      RPCMP_CHECK(suite, truncated.logical_track == 4);
      RPCMP_CHECK(suite, unchanged.byte_offset == 77);
    }
  }

  constexpr std::array<std::uint8_t, 7> unsupported_opcodes{0xfa, 0xf9, 0xed, 0xec,
                                                            0xeb, 0xea, 0xe9};
  for (const std::uint8_t opcode : unsupported_opcodes) {
    InstructionView output{};
    RPCMP_CHECK(suite, rpcmp::runtime::mdx::decode_instruction({&opcode, 1}, 12, 2, output).error ==
                           DecodeError::UnsupportedOpcode);
  }
  constexpr std::array<std::uint8_t, 4> unsupported_extensions{0xe7, 0xe6, 0xe5, 0xe0};
  for (const std::uint8_t opcode : unsupported_extensions) {
    InstructionView output{};
    RPCMP_CHECK(suite, rpcmp::runtime::mdx::decode_instruction({&opcode, 1}, 13, 2, output).error ==
                           DecodeError::UnsupportedExtension);
  }
  const std::uint8_t pcm8 = 0xe8;
  InstructionView instruction{};
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::decode_instruction({&pcm8, 1}, 14, 8, instruction).error ==
                  DecodeError::UnsupportedPcm);

  const std::array<std::uint8_t, 3> invalid_repeat{0xf6, 0x02, 0x01};
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::decode_instruction(
                         {invalid_repeat.data(), invalid_repeat.size()}, 15, 0, instruction)
                             .error == DecodeError::InvalidRepeat);
  const std::array<std::uint8_t, 2> invalid_sync{0xef, 0x09};
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::decode_instruction(
                         {invalid_sync.data(), invalid_sync.size()}, 16, 0, instruction)
                             .error == DecodeError::InvalidSyncChannel);

  const std::vector<std::uint8_t> complete{0x00, 0xff, 0xc0, 0xf1, 0x00, 0xaa};
  TrackValidation validated{};
  const auto valid = rpcmp::runtime::mdx::validate_track(track(complete), validated);
  RPCMP_CHECK(suite, valid.ok());
  RPCMP_CHECK(suite, validated.instruction_count == 3);
  RPCMP_CHECK(suite, validated.terminal_byte_offset == 103);

  DecodeLimits limits{};
  limits.max_instructions = 2;
  TrackValidation unchanged{};
  unchanged.instruction_count = 91;
  const auto exhausted = rpcmp::runtime::mdx::validate_track(track(complete), unchanged, limits);
  RPCMP_CHECK(suite, exhausted.error == DecodeError::BudgetExhausted);
  RPCMP_CHECK(suite, exhausted.byte_offset == 103);
  RPCMP_CHECK(suite, unchanged.instruction_count == 91);

  MdxDocument document{};
  const std::vector<std::uint8_t> inert{0x02, 0xf1, 0x00};
  for (std::size_t index = 0; index < document.tracks.size(); ++index) {
    document.tracks[index] = track(inert, static_cast<std::uint8_t>(index), 200 + index * 10);
  }
  DocumentValidation document_validation{};
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::validate_document(document, document_validation).ok());

  const std::vector<std::uint8_t> active_pcm{0x80, 0x00, 0xf1, 0x00};
  document.tracks[8] = track(active_pcm, 8, 900);
  document_validation.tracks[0].instruction_count = 88;
  const auto rejected = rpcmp::runtime::mdx::validate_document(document, document_validation);
  RPCMP_CHECK(suite, rejected.error == DecodeError::UnsupportedPcm);
  RPCMP_CHECK(suite, rejected.byte_offset == 900);
  RPCMP_CHECK(suite, rejected.logical_track == 8);
  RPCMP_CHECK(suite, document_validation.tracks[0].instruction_count == 88);

  document.tracks[8].source = {nullptr, 1};
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::validate_document(document, document_validation).error ==
                         DecodeError::RangeOutsideInput);

  const std::vector<std::uint8_t> unterminated{0x00};
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::validate_track(track(unterminated), validated).error ==
                         DecodeError::TruncatedInstruction);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::decode_instruction({nullptr, 1}, 0, 0, instruction).error ==
                  DecodeError::RangeOutsideInput);

  return suite.finish("mdx instruction decoder");
}
