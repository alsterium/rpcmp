#ifndef RPCMP_UTILITY_METADATA_HPP
#define RPCMP_UTILITY_METADATA_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace rpcmp::utility {

inline constexpr std::size_t kMetadataMaxBytes = 4096;
inline constexpr std::size_t kMetadataWorkBytes = 65536;

enum class TextError : std::uint8_t {
  None = 0,
  InvalidEncoding,
  EmbeddedNul,
  SizeLimit,
  ControlCharacter,
  Empty,
  Normalization
};

struct TextResult {
  TextError error{TextError::None};
  std::string text;
  [[nodiscard]] bool ok() const noexcept { return error == TextError::None; }
};

enum class TitleFallback : std::uint8_t { None = 0, InvalidEncoding, Empty, ControlCharacter };

struct MdxTitleResult {
  TextResult title;
  TitleFallback fallback{TitleFallback::None};
};

// No locale, filesystem access, display sanitation or compatibility folding.
[[nodiscard]] TextResult normalize_utf8(std::string_view input);
[[nodiscard]] TextResult utf16_to_utf8(std::u16string_view input);

// raw_title excludes the MDX delimiter. filename_stem is already native-path
// converted UTF-8, not a path to reopen. A size failure never triggers fallback.
[[nodiscard]] MdxTitleResult select_mdx_title(std::string_view raw_title,
                                              std::string_view filename_stem);
[[nodiscard]] const char* title_fallback_name(TitleFallback reason) noexcept;

} // namespace rpcmp::utility

#endif // RPCMP_UTILITY_METADATA_HPP
