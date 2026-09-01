#include "rpcmp/runtime/mdx_semantic.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using rpcmp::runtime::mdx::DecodeError;
using rpcmp::runtime::mdx::InstructionKind;
using rpcmp::runtime::mdx::SemanticInstruction;

struct Golden {
  std::array<std::uint8_t, 3> bytes{};
  std::size_t length{};
  InstructionKind kind{InstructionKind::Rest};
  std::uint16_t duration{};
  std::int16_t signed_value{};
  std::uint8_t value{};
  std::uint8_t second_value{};
};

} // namespace

int main() {
  rpcmp::test::Suite suite;
  constexpr std::array<Golden, 20> golden{{
      {{{0x00, 0, 0}}, 1, InstructionKind::Rest, 1, 0, 0, 0},
      {{{0x7f, 0, 0}}, 1, InstructionKind::Rest, 128, 0, 0, 0},
      {{{0x80, 0x00, 0}}, 2, InstructionKind::Note, 1, 0, 0x80, 0},
      {{{0xdf, 0xff, 0}}, 2, InstructionKind::Note, 256, 0, 0xdf, 0},
      {{{0xff, 0xc8, 0}}, 2, InstructionKind::TimerB, 0, 0, 0xc8, 0},
      {{{0xfe, 0x20, 0xc7}}, 3, InstructionKind::DirectWrite, 0, 0, 0x20, 0xc7},
      {{{0xfd, 0x12, 0}}, 2, InstructionKind::SelectVoice, 0, 0, 0x12, 0},
      {{{0xfc, 0x03, 0}}, 2, InstructionKind::Pan, 0, 0, 0x03, 0},
      {{{0xfb, 0x0f, 0}}, 2, InstructionKind::Volume, 0, 0, 0x0f, 0},
      {{{0xf8, 0x08, 0}}, 2, InstructionKind::Gate, 0, 0, 0x08, 0},
      {{{0xf7, 0, 0}}, 1, InstructionKind::SuppressKeyOff, 0, 0, 0, 0},
      {{{0xf6, 0x04, 0x00}}, 3, InstructionKind::RepeatStart, 0, 0, 0x04, 0},
      {{{0xf5, 0xff, 0xf0}}, 3, InstructionKind::RepeatEnd, 0, -16, 0, 0},
      {{{0xf4, 0x00, 0x7f}}, 3, InstructionKind::RepeatEscape, 0, 127, 0, 0},
      {{{0xf3, 0x80, 0x00}}, 3, InstructionKind::Detune, 0, -32768, 0, 0},
      {{{0xf2, 0x7f, 0xff}}, 3, InstructionKind::Portamento, 0, 32767, 0, 0},
      {{{0xf1, 0x00, 0}}, 2, InstructionKind::TrackEnd, 0, 0, 0, 0},
      {{{0xf0, 0xff, 0}}, 2, InstructionKind::KeyOnDelay, 0, 0, 0xff, 0},
      {{{0xef, 0x08, 0}}, 2, InstructionKind::ReleaseChannel, 0, 0, 0x08, 0},
      {{{0xee, 0, 0}}, 1, InstructionKind::WaitChannel, 0, 0, 0, 0},
  }};

  for (const auto& expected : golden) {
    SemanticInstruction actual{};
    const auto result = rpcmp::runtime::mdx::decode_semantic(
        {expected.bytes.data(), expected.length}, 123, 2, actual);
    RPCMP_CHECK(suite, result.ok());
    RPCMP_CHECK(suite, actual.kind == expected.kind);
    RPCMP_CHECK(suite, actual.byte_offset == 123);
    RPCMP_CHECK(suite, actual.duration_ticks == expected.duration);
    RPCMP_CHECK(suite, actual.signed_value == expected.signed_value);
    RPCMP_CHECK(suite, actual.value == expected.value);
    RPCMP_CHECK(suite, actual.second_value == expected.second_value);
  }

  const std::array<std::uint8_t, 3> loop{0xf1, 0xff, 0xfc};
  SemanticInstruction loop_action{};
  RPCMP_CHECK(
      suite,
      rpcmp::runtime::mdx::decode_semantic({loop.data(), loop.size()}, 200, 7, loop_action).ok());
  RPCMP_CHECK(suite, loop_action.kind == InstructionKind::TrackLoop);
  RPCMP_CHECK(suite, loop_action.signed_value == -4);

  const std::uint8_t unsupported = 0xe6;
  SemanticInstruction unchanged{};
  unchanged.byte_offset = 999;
  const auto rejected = rpcmp::runtime::mdx::decode_semantic({&unsupported, 1}, 300, 4, unchanged);
  RPCMP_CHECK(suite, rejected.error == DecodeError::UnsupportedExtension);
  RPCMP_CHECK(suite, rejected.byte_offset == 300);
  RPCMP_CHECK(suite, unchanged.byte_offset == 999);

  return suite.finish("mdx semantic instruction decoding");
}
