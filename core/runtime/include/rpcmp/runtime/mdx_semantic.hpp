#ifndef RPCMP_RUNTIME_MDX_SEMANTIC_HPP
#define RPCMP_RUNTIME_MDX_SEMANTIC_HPP

#include "rpcmp/runtime/mdx_decoder.hpp"

#include <cstddef>
#include <cstdint>

namespace rpcmp::runtime::mdx {

struct SemanticInstruction {
  InstructionKind kind{InstructionKind::Rest};
  std::size_t byte_offset{};
  std::uint16_t duration_ticks{};
  std::int16_t signed_value{};
  std::uint8_t value{};
  std::uint8_t second_value{};
};

// Converts one already admitted MDX instruction into endian-independent typed
// values. It performs no control flow and changes output only on success.
[[nodiscard]] DecodeResult decode_semantic(ByteView source, std::size_t source_offset,
                                           std::uint8_t logical_track,
                                           SemanticInstruction& output) noexcept;

} // namespace rpcmp::runtime::mdx

#endif // RPCMP_RUNTIME_MDX_SEMANTIC_HPP
