#include "rpcmp/platform/pocket/bitmap_canvas.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace rpcmp::platform::pocket {
namespace {
namespace ui = rpcmp::ui::v2;
bool inside(const ui::Point point) noexcept {
  return point.x < ui::kCanvasWidth && point.y < ui::kCanvasHeight;
}
std::uint8_t ink(const ui::Color color) noexcept {
  const auto exact = std::find(kBitmapPalette.begin(), kBitmapPalette.end(), color);
  if (exact != kBitmapPalette.end())
    return static_cast<std::uint8_t>(exact - kBitmapPalette.begin());
  std::uint32_t distance = std::numeric_limits<std::uint32_t>::max();
  std::uint8_t best{};
  for (std::size_t i = 0; i < kBitmapPalette.size(); ++i) {
    std::uint32_t sum{};
    for (unsigned shift = 0; shift < 24; shift += 8) {
      const auto delta = static_cast<int>((color >> shift) & 255) -
                         static_cast<int>((kBitmapPalette[i] >> shift) & 255);
      sum += static_cast<std::uint32_t>(delta * delta);
    }
    if (sum < distance) {
      distance = sum;
      best = static_cast<std::uint8_t>(i);
    }
  }
  return best;
}
// The whole string was validated before this decoder is called.
std::uint32_t scalar(const std::string_view value, std::size_t& offset) noexcept {
  const auto first = static_cast<std::uint8_t>(value[offset++]);
  const unsigned count = first < 0x80 ? 1 : first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
  std::uint32_t result = first & (count == 1 ? 0x7FU : count == 2 ? 0x1FU : count == 3 ? 15U : 7U);
  for (unsigned i = 1; i < count; ++i)
    result = (result << 6U) | (static_cast<std::uint8_t>(value[offset++]) & 63U);
  return result;
}
} // namespace

void BitmapCanvas::begin() noexcept {
  count_ = next_ = text_offset_ = 0;
  position_ = text_x_ = 0;
  glyph_ = {};
  surface_ = nullptr;
  stride_ = 0;
  failed_ = sealed_ = missing_ = false;
}
BitmapCanvas::Command* BitmapCanvas::append(const ui::Box box, const ui::Color color,
                                            const Kind kind) noexcept {
  if (sealed_ || count_ == commands_.size() || !inside({box.x, box.y}) || box.width == 0 ||
      box.height == 0 || box.width > ui::kCanvasWidth - box.x ||
      box.height > ui::kCanvasHeight - box.y || color > 0xFFFFFF)
    failed_ = true;
  if (failed_)
    return nullptr;
  auto& item = commands_[count_++];
  item.box = box;
  item.ink = ink(color);
  item.kind = kind;
  item.length = 0;
  return &item;
}
void BitmapCanvas::fill(const ui::Box box, const ui::Color color) {
  (void)append(box, color, Kind::Fill);
}
void BitmapCanvas::line(const ui::Point from, const ui::Point to, const ui::Color color) {
  if (!inside(to))
    failed_ = true;
  if (auto* item = append({from.x, from.y, 1, 1}, color, Kind::Line))
    item->end = to;
}
void BitmapCanvas::text(const ui::Box box, const std::string_view value, const ui::Color color,
                        const bool source_truncated) {
  auto* item = append(box, color, Kind::Text);
  if (item == nullptr)
    return;
  contracts::CatalogText checked;
  if (value.size() > checked.bytes.size()) {
    failed_ = true;
    return;
  }
  std::copy(value.begin(), value.end(), checked.bytes.begin());
  checked.length = static_cast<std::uint16_t>(value.size());
  if (!contracts::valid_catalog_text(checked)) {
    failed_ = true;
    return;
  }
  std::uint32_t width{};
  for (std::size_t offset = 0; offset < value.size();) {
    const auto code = scalar(value, offset);
    missing_ = missing_ || !bitmap_has_glyph(code);
    width += bitmap_glyph(code).width;
  }
  const bool elide = source_truncated || width > box.width;
  const auto ellipsis = bitmap_glyph(0x2026).width;
  if (elide && box.width < ellipsis)
    return;
  const auto available = box.width - (elide ? ellipsis : 0U);
  width = 0;
  for (std::size_t offset = 0; offset < value.size();) {
    const auto end = offset;
    const auto glyph = bitmap_glyph(scalar(value, offset));
    if (width + glyph.width > available)
      break;
    std::copy(value.begin() + static_cast<std::ptrdiff_t>(end),
              value.begin() + static_cast<std::ptrdiff_t>(offset),
              item->text.begin() + item->length);
    item->length = static_cast<std::uint16_t>(offset);
    width += glyph.width;
  }
  if (elide) {
    constexpr std::string_view suffix = "…";
    std::copy(suffix.begin(), suffix.end(), item->text.begin() + item->length);
    item->length += 3;
  }
}
bool BitmapCanvas::seal() noexcept {
  sealed_ = true;
  return !failed_;
}
void BitmapCanvas::advance() noexcept {
  ++next_;
  position_ = text_x_ = 0;
  text_offset_ = 0;
  glyph_ = {};
}
std::uint32_t BitmapCanvas::pump(std::uint8_t* surface, const std::size_t bytes,
                                 const std::uint32_t stride,
                                 const std::uint32_t pixel_budget) noexcept {
  if (!sealed_ || surface == nullptr || stride < ui::kCanvasWidth || stride > 4096 ||
      bytes < stride * (ui::kCanvasHeight - 1U) + ui::kCanvasWidth ||
      (surface_ != nullptr && (surface_ != surface || stride_ != stride)))
    failed_ = true;
  if (failed_)
    return 0;
  surface_ = surface;
  stride_ = stride;
  std::uint32_t used{};
  while (next_ < count_ && used < pixel_budget) {
    const auto& item = commands_[next_];
    const auto& box = item.box;
    if (item.kind == Kind::Fill) {
      const auto x = position_ % box.width;
      const auto y = position_ / box.width;
      const auto count = std::min(pixel_budget - used, box.width - x);
      std::fill_n(surface + static_cast<std::size_t>(box.y + y) * stride + box.x + x, count,
                  item.ink);
      used += count;
      position_ += count;
      if (position_ == static_cast<std::uint32_t>(box.width) * box.height)
        advance();
    } else if (item.kind == Kind::Line) {
      const auto dx = std::abs(static_cast<int>(item.end.x) - box.x);
      const auto dy = -std::abs(static_cast<int>(item.end.y) - box.y);
      if (position_ == 0) {
        line_x_ = box.x;
        line_y_ = box.y;
        line_error_ = dx + dy;
      }
      surface[static_cast<std::uint32_t>(line_y_) * stride + static_cast<std::uint32_t>(line_x_)] =
          item.ink;
      ++used;
      ++position_;
      if (line_x_ == item.end.x && line_y_ == item.end.y) {
        advance();
        continue;
      }
      const auto twice = 2 * line_error_;
      if (twice >= dy) {
        line_error_ += dy;
        line_x_ += box.x < item.end.x ? 1 : -1;
      }
      if (twice <= dx) {
        line_error_ += dx;
        line_y_ += box.y < item.end.y ? 1 : -1;
      }
    } else {
      if (glyph_.rows == nullptr) {
        if (text_offset_ == item.length) {
          advance();
          continue;
        }
        glyph_ = bitmap_glyph(scalar({item.text.data(), item.length}, text_offset_));
        position_ = 0;
      }
      const auto x = position_ % glyph_.width;
      const auto y = position_ / glyph_.width;
      const auto bits = glyph_.rows[y * (glyph_.width / 8) + x / 8];
      if ((bits & (0x80U >> (x % 8))) != 0)
        surface[(box.y + y) * stride + box.x + text_x_ + x] = item.ink;
      ++used;
      if (++position_ == glyph_.width * std::min<std::uint32_t>(box.height, 16)) {
        text_x_ += glyph_.width;
        glyph_ = {};
      }
    }
  }
  return used;
}
} // namespace rpcmp::platform::pocket
