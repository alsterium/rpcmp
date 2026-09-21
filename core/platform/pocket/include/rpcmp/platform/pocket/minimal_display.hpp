#ifndef RPCMP_PLATFORM_POCKET_MINIMAL_DISPLAY_HPP
#define RPCMP_PLATFORM_POCKET_MINIMAL_DISPLAY_HPP

#include "rpcmp/ui/minimal_player.hpp"

namespace rpcmp::platform::pocket {
inline constexpr std::array<std::uint32_t, 5> kMinimalPalette{0x101820, 0xEBF2F5, 0x244254,
                                                              0x72DACB, 0xFF9292};
struct MinimalTimings {
  std::uint32_t render{}, feed{}, draw{}, flip{}, queue{};
  std::uint32_t catalog_load{};
};
// One immutable view per frame, rendered four scanlines per audio-service turn.
class MinimalDisplay {
public:
  void begin(ui::minimal::View view, MinimalTimings timings = {});
  bool pump(std::uint8_t* surface, std::size_t bytes);
  [[nodiscard]] bool complete() const noexcept { return row_ == 480; }

private:
  ui::minimal::View view_{};
  MinimalTimings timings_{};
  std::uint32_t row_{};
};
} // namespace rpcmp::platform::pocket
#endif
