#include "rpcmp/platform/host/svg_canvas.hpp"

#include <algorithm>
#include <limits>
#include <string>

namespace rpcmp::platform::host {
namespace {
namespace ui = rpcmp::ui::v2;
bool inside(const ui::Point point) noexcept {
  return point.x < ui::kCanvasWidth && point.y < ui::kCanvasHeight;
}
struct Glyph {
  std::string_view bytes;
  unsigned advance{};
};
// Called only on a string accepted by the public UTF-8/catalog validator.
Glyph glyph(const std::string_view value, std::size_t& offset) noexcept {
  const auto start = offset;
  const auto first = static_cast<std::uint8_t>(value[offset++]);
  const unsigned size = first < 0x80 ? 1 : first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
  std::uint32_t code = first & (size == 1 ? 0x7FU : size == 2 ? 0x1FU : size == 3 ? 0x0FU : 0x07U);
  for (unsigned i = 1; i < size; ++i)
    code = (code << 6U) | (static_cast<std::uint8_t>(value[offset++]) & 0x3FU);
  if (code < 0x20 || (code >= 0x7F && code <= 0x9F) || code == 0xFFFE || code == 0xFFFF)
    return {"�", 16};
  const bool wide = code >= 0x1100 && (code < 0xFF61 || code > 0xFF9F);
  return {value.substr(start, size), wide ? 16U : 8U};
}
void escaped(std::ostream& out, const std::string_view value) {
  for (const auto ch : value) {
    switch (ch) {
    case '&':
      out << "&amp;";
      break;
    case '<':
      out << "&lt;";
      break;
    case '>':
      out << "&gt;";
      break;
    case '"':
      out << "&quot;";
      break;
    case '\'':
      out << "&apos;";
      break;
    default:
      out.put(ch);
      break;
    }
  }
}
} // namespace

SvgCanvas::SvgCanvas(std::ostream& output) : output_(output) {
  output_ << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"640\" height=\"480\" "
             "viewBox=\"0 0 640 480\" shape-rendering=\"crispEdges\">\n";
}
bool SvgCanvas::usable(const ui::Box box, const ui::Color color_value) noexcept {
  if (finished_ || !inside({box.x, box.y}) || box.width == 0 || box.height == 0 ||
      box.width > ui::kCanvasWidth - box.x || box.height > ui::kCanvasHeight - box.y ||
      color_value > 0xFFFFFF)
    failed_ = true;
  return !failed_;
}
void SvgCanvas::color(const ui::Color value) {
  output_.put('#');
  for (unsigned shift = 24; shift != 0; shift -= 4)
    output_.put("0123456789ABCDEF"[(value >> (shift - 4)) & 0xFU]);
}
void SvgCanvas::rectangle(const ui::Box box) {
  output_ << "x=\"" << box.x << "\" y=\"" << box.y << "\" width=\"" << box.width << "\" height=\""
          << box.height << '"';
}
void SvgCanvas::fill(const ui::Box box, const ui::Color value) {
  if (!usable(box, value))
    return;
  output_ << "<rect ";
  rectangle(box);
  output_ << " fill=\"";
  color(value);
  output_ << "\"/>\n";
}
void SvgCanvas::line(const ui::Point from, const ui::Point to, const ui::Color value) {
  if (!inside(to))
    failed_ = true;
  if (!usable({from.x, from.y, 1, 1}, value))
    return;
  output_ << "<line x1=\"" << from.x << "\" y1=\"" << from.y << "\" x2=\"" << to.x << "\" y2=\""
          << to.y << "\" stroke=\"";
  color(value);
  output_ << "\"/>\n";
}
void SvgCanvas::text(const ui::Box box, const std::string_view value, const ui::Color ink,
                     const bool source_truncated) {
  if (!usable(box, ink))
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
  unsigned total = 0;
  for (std::size_t offset = 0; offset < value.size();)
    total += glyph(value, offset).advance;
  const bool elided = source_truncated || total > box.width;
  if (elided && box.width < 16)
    return;
  const auto available = box.width - (elided ? 16U : 0U);
  unsigned width = 0;
  std::string visible;
  for (std::size_t offset = 0; offset < value.size();) {
    const auto next = glyph(value, offset);
    if (width + next.advance > available)
      break;
    visible += next.bytes;
    width += next.advance;
  }
  if (elided) {
    visible += "…";
    width += 16;
  }
  if (visible.empty())
    return;
  if (next_clip_ == std::numeric_limits<std::uint32_t>::max()) {
    failed_ = true;
    return;
  }
  const auto id = next_clip_++;
  output_ << "<defs><clipPath id=\"clip" << id << "\"><rect ";
  rectangle(box);
  output_ << "/></clipPath></defs>\n<text x=\"" << box.x << "\" y=\"" << box.y + 13
          << "\" font-family=\"Consolas,monospace\" font-size=\"16\" textLength=\"" << width
          << "\" lengthAdjust=\"spacingAndGlyphs\" clip-path=\"url(#clip" << id << ")\" fill=\"";
  color(ink);
  output_ << "\" data-elided=\"" << (elided ? "true" : "false") << "\">";
  escaped(output_, visible);
  output_ << "</text>\n";
}
bool SvgCanvas::finish() {
  if (!finished_) {
    output_ << "</svg>\n";
    finished_ = true;
  }
  return !failed_ && output_.good();
}

} // namespace rpcmp::platform::host
