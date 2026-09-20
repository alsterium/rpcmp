#ifndef RPCMP_PLATFORM_POCKET_BITMAP_FONT_HPP
#define RPCMP_PLATFORM_POCKET_BITMAP_FONT_HPP

#include <cstdint>

namespace rpcmp::platform::pocket {
struct BitmapGlyph {
  const std::uint8_t* rows{}; // 16 rows, MSB first, width/8 bytes per row.
  std::uint32_t width{};
};
// Unknown/control scalars use the visible replacement glyph. No dynamic cache.
[[nodiscard]] BitmapGlyph bitmap_glyph(std::uint32_t scalar) noexcept;
[[nodiscard]] bool bitmap_has_glyph(std::uint32_t scalar) noexcept;
} // namespace rpcmp::platform::pocket
#endif
