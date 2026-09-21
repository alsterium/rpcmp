#ifndef RPCMP_UI_MINIMAL_PLAYER_HPP
#define RPCMP_UI_MINIMAL_PLAYER_HPP

#include "rpcmp/contracts/minimal_player.hpp"

namespace rpcmp::ui::minimal {
namespace api = contracts::minimal;
inline constexpr std::uint32_t kRows = 13;
struct Bindings {
  std::uint32_t up{1}, down{2}, left{4}, right{8}, confirm{0x10}, back{0x20};
  std::uint32_t panel_l{0x100}, panel_r{0x200};
};
enum class Panel : std::uint8_t { List, Controls };
enum class Icon : std::uint8_t { Previous, PlayPause, Next, Stop, Repeat };
struct View {
  std::uint64_t revision{};
  api::PlayerSnapshot playback{};
  std::uint32_t selected{}, first{}, count{};
  std::array<contracts::CatalogText, kRows> titles{};
  std::array<bool, kRows> playing{};
  std::uint32_t rows{}, selected_row{}, playing_number{}, playing_count{};
  api::PlaylistId playlist{};
  Panel panel{Panel::List};
  Icon icon{Icon::PlayPause};
  std::array<bool, 5> enabled{false, false, false, false, true};
  std::uint32_t scroll_tick{};
  contracts::CatalogText list_title{}, playing_list{};
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
  void activate();
  void back();
  void move(std::uint32_t direction);
  void reset_scroll();
  const api::TrackList& tracks_;
  api::CommandSink& commands_;
  Bindings bindings_;
  View view_{};
  std::array<std::uint32_t, api::kMaxPlaylists> positions_{};
  std::uint32_t list_position_{};
  std::uint32_t previous_{}, navigation_{}, repeated_at_{}, blocked_{};
  std::uint32_t now_{}, selected_at_{};
  bool connected_{}, repeating_{};
};
} // namespace rpcmp::ui::minimal
#endif
