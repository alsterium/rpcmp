#ifndef RPCMP_PLATFORM_POCKET_BITMAP_CANVAS_HPP
#define RPCMP_PLATFORM_POCKET_BITMAP_CANVAS_HPP

#include "rpcmp/platform/pocket/bitmap_font.hpp"
#include "rpcmp/ui/player_canvas.hpp"

namespace rpcmp::platform::pocket {
inline constexpr std::array<ui::v2::Color, 10> kBitmapPalette{
    ui::v2::palette::background, ui::v2::palette::panel,   ui::v2::palette::grid,
    ui::v2::palette::text,       ui::v2::palette::muted,   ui::v2::palette::accent,
    ui::v2::palette::selected,   ui::v2::palette::warning, ui::v2::palette::white_key,
    ui::v2::palette::black_key};

// Copies synchronous UI drawing commands, then rasterizes bounded pixel work.
// No player, clock, audio callbacks, heap, or borrowed text. The composition owner
// services audio between pump calls and presents only complete, successful frames.
class BitmapCanvas final : public ui::v2::PlayerCanvas {
public:
  static constexpr std::size_t kCommandCapacity = 2048;
  void begin() noexcept;
  void fill(ui::v2::Box box, ui::v2::Color color) override;
  void line(ui::v2::Point from, ui::v2::Point to, ui::v2::Color color) override;
  void text(ui::v2::Box box, std::string_view value, ui::v2::Color color,
            bool source_truncated = false) override;
  [[nodiscard]] bool seal() noexcept;
  // Transparent glyph pixels also consume the budget. Geometry/stride is checked
  // before touching memory. Every call must use the same owned draw surface.
  [[nodiscard]] std::uint32_t pump(std::uint8_t* surface, std::size_t bytes, std::uint32_t stride,
                                   std::uint32_t pixel_budget) noexcept;
  [[nodiscard]] bool complete() const noexcept { return sealed_ && !failed_ && next_ == count_; }
  [[nodiscard]] bool failed() const noexcept { return failed_; }
  [[nodiscard]] bool missing_glyphs() const noexcept { return missing_; }
  [[nodiscard]] std::size_t command_count() const noexcept { return count_; }

private:
  enum class Kind : std::uint8_t { Fill, Line, Text };
  struct Command {
    ui::v2::Box box{};
    ui::v2::Point end{};
    // The complete public prefix plus a platform-added UTF-8 ellipsis.
    std::array<char, contracts::kMaxMetadataBytes + 3> text{};
    std::uint16_t length{};
    std::uint8_t ink{};
    Kind kind{};
  };
  [[nodiscard]] Command* append(ui::v2::Box box, ui::v2::Color color, Kind kind) noexcept;
  void advance() noexcept;
  std::array<Command, kCommandCapacity> commands_{};
  std::size_t count_{}, next_{}, text_offset_{};
  std::uint32_t position_{}, text_x_{};
  BitmapGlyph glyph_{};
  std::int32_t line_x_{}, line_y_{}, line_error_{};
  std::uint8_t* surface_{};
  std::uint32_t stride_{};
  bool failed_{}, sealed_{}, missing_{};
};
} // namespace rpcmp::platform::pocket
#endif
