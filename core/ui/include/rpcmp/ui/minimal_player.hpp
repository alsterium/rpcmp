#ifndef RPCMP_UI_MINIMAL_PLAYER_HPP
#define RPCMP_UI_MINIMAL_PLAYER_HPP

#include "rpcmp/contracts/minimal_player.hpp"

namespace rpcmp::ui::minimal {
namespace api = contracts::minimal;
inline constexpr std::uint32_t kRows = 13;
struct Bindings {
  std::uint32_t up{1}, down{2}, left{4}, right{8}, play{0x10}, stop{0x20};
  std::uint32_t pause{0x40};
};
struct View {
  std::uint64_t revision{};
  api::PlayerSnapshot playback{};
  std::uint32_t selected{}, first{}, count{};
  std::array<contracts::CatalogText, kRows> titles{};
  contracts::CatalogText playing_title{};
};
class Controller {
public:
  Controller(const api::TrackList& tracks, api::CommandSink& commands, Bindings bindings = {});
  void input(std::uint32_t buttons, bool connected, std::uint32_t now_us);
  void update(api::PlayerSnapshot snapshot);
  [[nodiscard]] View view() const noexcept { return view_; }

private:
  void titles();
  const api::TrackList& tracks_;
  api::CommandSink& commands_;
  Bindings bindings_;
  View view_{};
  std::uint32_t previous_{}, navigation_{}, repeated_at_{};
  bool connected_{}, repeating_{};
};
} // namespace rpcmp::ui::minimal
#endif
