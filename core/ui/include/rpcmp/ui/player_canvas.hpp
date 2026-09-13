#ifndef RPCMP_UI_PLAYER_CANVAS_HPP
#define RPCMP_UI_PLAYER_CANVAS_HPP

#include "rpcmp/ui/player_ui.hpp"

#include <string_view>

namespace rpcmp::ui::v2 {
inline constexpr std::uint16_t kCanvasWidth = 640;
inline constexpr std::uint16_t kCanvasHeight = 480;
struct Point {
  std::uint16_t x{}, y{};
};
struct Box {
  std::uint16_t x{}, y{}, width{}, height{};
};
using Color = std::uint32_t; // 0x00RRGGBB, platform converts to its pixel format.
namespace palette {
inline constexpr Color background = 0x081316;
inline constexpr Color panel = 0x0F2024;
inline constexpr Color grid = 0x234247;
inline constexpr Color text = 0xAED6CD;
inline constexpr Color muted = 0x607F7B;
inline constexpr Color accent = 0x57E5C7;
inline constexpr Color selected = 0x173D3B;
inline constexpr Color warning = 0xE6BD6B;
inline constexpr Color white_key = 0xBECEC6;
inline constexpr Color black_key = 0x10201E;
} // namespace palette

// Synchronous drawing only. The port copies/consumes text before returning and
// clips to its box, eliding at glyph boundaries; source_truncated adds an ellipsis.
// Nominal text is 16 pixels high. Pocket glyph metrics remain a separate adapter.
class PlayerCanvas {
public:
  virtual ~PlayerCanvas() = default;
  virtual void fill(Box box, Color color) = 0;
  virtual void line(Point from, Point to, Color color) = 0;
  virtual void text(Box box, std::string_view value, Color color,
                    bool source_truncated = false) = 0;
};

// A checked PlayerUi view (or equivalently authored mock); no input, clock or Core calls.
void render_player(const PlayerView& view, PlayerCanvas& canvas);

} // namespace rpcmp::ui::v2

#endif // RPCMP_UI_PLAYER_CANVAS_HPP
