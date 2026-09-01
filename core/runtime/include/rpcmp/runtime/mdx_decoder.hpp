#ifndef RPCMP_RUNTIME_MDX_DECODER_HPP
#define RPCMP_RUNTIME_MDX_DECODER_HPP

#include "rpcmp/runtime/mdx_parser.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpcmp::runtime::mdx {

inline constexpr std::size_t kMdxMaxDecodedInstructions = kMdxMaxInputBytes;
inline constexpr std::size_t kMdxBoundaryBitmapBytes = (kMdxMaxInputBytes + 7U) / 8U;

enum class InstructionKind : std::uint8_t {
  Rest = 0,
  Note,
  TimerB,
  DirectWrite,
  SelectVoice,
  Pan,
  Volume,
  Gate,
  SuppressKeyOff,
  RepeatStart,
  RepeatEnd,
  RepeatEscape,
  Detune,
  Portamento,
  TrackEnd,
  TrackLoop,
  KeyOnDelay,
  ReleaseChannel,
  WaitChannel,
};

enum class DecodeError : std::uint8_t {
  None = 0,
  InvalidLimits,
  RangeOutsideInput,
  TruncatedInstruction,
  InvalidBranchTarget,
  InvalidRepeat,
  InvalidSyncChannel,
  UnsupportedOpcode,
  UnsupportedExtension,
  UnsupportedPcm,
  BudgetExhausted,
};

struct DecodeLimits {
  std::size_t max_instructions{kMdxMaxDecodedInstructions};
};

struct InstructionView {
  InstructionKind kind{InstructionKind::Rest};
  std::uint8_t opcode{};
  std::size_t byte_offset{};
  ByteView source{};
};

struct DecodeResult {
  DecodeError error{DecodeError::None};
  std::size_t byte_offset{};
  std::uint8_t logical_track{0xff};

  [[nodiscard]] bool ok() const noexcept { return error == DecodeError::None; }
};

struct TrackValidation {
  std::size_t instruction_count{};
  std::size_t terminal_byte_offset{};
};

struct DocumentValidation {
  std::array<TrackValidation, kMdxTrackCount> tracks{};
};

struct ControlFlowScratch {
  std::array<std::uint8_t, kMdxBoundaryBitmapBytes> instruction_starts{};
};

// Decodes one admitted v1 instruction without executing it. source_offset is
// the absolute input offset corresponding to source.data[0].
[[nodiscard]] DecodeResult decode_instruction(ByteView source, std::size_t source_offset,
                                              std::uint8_t logical_track,
                                              InstructionView& output) noexcept;

// Validation stops at the first f1 00. It never follows loops or branches and
// changes output only after the whole track has been admitted.
[[nodiscard]] DecodeResult validate_track(const TrackView& track, TrackValidation& output,
                                          DecodeLimits limits = DecodeLimits{}) noexcept;

// Validates A-H as FM and admits P only when it contains rests followed by
// f1 00. Rejection is transactional and emits no semantic or device work.
[[nodiscard]] DecodeResult validate_document(const MdxDocument& document,
                                             DocumentValidation& output,
                                             DecodeLimits limits = DecodeLimits{}) noexcept;

// Proves that every admitted control-flow target has the MXDRV-defined base,
// remains within this track, and lands on the required instruction/repeat
// boundary. Scratch is caller-owned so Pocket integrations choose its storage.
[[nodiscard]] DecodeResult validate_control_flow(const TrackView& track,
                                                 ControlFlowScratch& scratch,
                                                 DecodeLimits limits = DecodeLimits{}) noexcept;

} // namespace rpcmp::runtime::mdx

#endif // RPCMP_RUNTIME_MDX_DECODER_HPP
