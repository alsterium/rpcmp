#include "rpcmp/platform/pocket/bitmap_font.hpp"

#include <algorithm>
#include <array>

namespace rpcmp::platform::pocket {
namespace {
struct FontEntry {
  std::uint32_t scalar, offset, width;
};
#include "bitmap_font.inc"
const FontEntry* find(const std::uint32_t scalar) noexcept {
  const auto entry = std::lower_bound(
      kFontIndex.begin(), kFontIndex.end(), scalar,
      [](const FontEntry& item, const std::uint32_t value) { return item.scalar < value; });
  return entry != kFontIndex.end() && entry->scalar == scalar ? &*entry : nullptr;
}
} // namespace
BitmapGlyph bitmap_glyph(const std::uint32_t scalar) noexcept {
  auto* entry = find(scalar);
  if (entry == nullptr)
    entry = find(0xFFFD);
  return {kFontBits.data() + entry->offset, entry->width};
}
bool bitmap_has_glyph(const std::uint32_t scalar) noexcept { return find(scalar) != nullptr; }
} // namespace rpcmp::platform::pocket
