#ifndef RPCMP_RUNTIME_MDX_PARSER_HPP
#define RPCMP_RUNTIME_MDX_PARSER_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpcmp::runtime::mdx {

inline constexpr std::size_t kMdxTrackCount = 9;
inline constexpr std::size_t kMdxVoiceSlots = 256;
inline constexpr std::size_t kMdxMaxInputBytes = 1'048'576;
inline constexpr std::size_t kMdxMaxTitleBytes = 4'096;
inline constexpr std::size_t kMdxMaxPdxReferenceBytes = 255;

struct ByteView {
  const std::uint8_t* data{};
  std::size_t size{};
};

enum class TrackTarget : std::uint8_t { Ym2151 = 0, LegacyAdpcm };

enum class MdxError : std::uint8_t {
  None = 0,
  InvalidLimits,
  InputTooLarge,
  MissingTitleTerminator,
  TitleTooLong,
  MissingPdxTerminator,
  PdxReferenceTooLong,
  RangeOutsideInput,
  OverlappingRegions,
  InvalidTrackTable,
  UnsupportedPcm8Layout,
  UnsupportedCompression,
  InvalidVoiceRecord,
  DuplicateVoice,
};

struct ParseLimits {
  std::size_t max_input_bytes{kMdxMaxInputBytes};
  std::size_t max_title_bytes{kMdxMaxTitleBytes};
  std::size_t max_pdx_reference_bytes{kMdxMaxPdxReferenceBytes};
  std::size_t max_voice_records{kMdxVoiceSlots};
};

struct Voice {
  bool present{};
  std::uint8_t feedback_connection{};
  std::uint8_t slot_mask{};
  std::array<std::array<std::uint8_t, 4>, 6> operators{};
};

struct TrackView {
  std::uint8_t logical_channel{};
  TrackTarget target{TrackTarget::Ym2151};
  std::size_t source_offset{};
  ByteView source{};
};

struct MdxDocument {
  ByteView input{};
  ByteView original_title{};
  ByteView pdx_reference{};
  std::array<Voice, kMdxVoiceSlots> voices{};
  std::size_t voice_count{};
  std::array<TrackView, kMdxTrackCount> tracks{};
};

struct ParseResult {
  MdxError error{MdxError::None};
  std::size_t byte_offset{};
  std::uint8_t logical_track{0xff};

  [[nodiscard]] bool ok() const noexcept { return error == MdxError::None; }
};

// Every view in a successful document borrows from input. The caller keeps the
// input bytes alive and immutable for the document's complete lifetime.
[[nodiscard]] ParseResult parse(ByteView input, MdxDocument& output,
                                ParseLimits limits = ParseLimits{}) noexcept;

} // namespace rpcmp::runtime::mdx

#endif // RPCMP_RUNTIME_MDX_PARSER_HPP
