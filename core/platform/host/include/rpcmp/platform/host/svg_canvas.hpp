#ifndef RPCMP_PLATFORM_HOST_SVG_CANVAS_HPP
#define RPCMP_PLATFORM_HOST_SVG_CANVAS_HPP

#include "rpcmp/ui/player_canvas.hpp"

#include <ostream>

namespace rpcmp::platform::host {

// Host review port with an explicit mock 8/16-pixel text grid, not Pocket font metrics.
class SvgCanvas final : public ui::v2::PlayerCanvas {
public:
  explicit SvgCanvas(std::ostream& output);
  void fill(ui::v2::Box box, ui::v2::Color value) override;
  void line(ui::v2::Point from, ui::v2::Point to, ui::v2::Color value) override;
  void text(ui::v2::Box box, std::string_view value, ui::v2::Color ink,
            bool source_truncated = false) override;
  [[nodiscard]] bool finish();

private:
  [[nodiscard]] bool usable(ui::v2::Box box, ui::v2::Color color_value) noexcept;
  void color(ui::v2::Color value);
  void rectangle(ui::v2::Box box);
  std::ostream& output_;
  std::uint32_t next_clip_{};
  bool failed_{}, finished_{};
};

} // namespace rpcmp::platform::host

#endif // RPCMP_PLATFORM_HOST_SVG_CANVAS_HPP
