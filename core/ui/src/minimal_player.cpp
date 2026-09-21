#include "rpcmp/ui/minimal_player.hpp"

#include <algorithm>
#include <string_view>

namespace rpcmp::ui::minimal {
namespace {
contracts::CatalogText label(const std::string_view value) {
  contracts::CatalogText result{};
  result.length = static_cast<std::uint16_t>(value.size());
  std::copy(value.begin(), value.end(), result.bytes.begin());
  return result;
}
} // namespace
Controller::Controller(const api::TrackList& tracks, api::CommandSink& commands,
                       const Bindings bindings)
    : tracks_(tracks), commands_(commands), bindings_(bindings) {
  view_.count = std::min(tracks_.playlist_count(), api::kMaxPlaylists);
  titles();
}
void Controller::titles() {
  const bool in_list = view_.playlist.value != 0;
  const auto capacity = in_list ? kRows - 1 : kRows;
  view_.first = view_.selected / capacity * capacity;
  view_.selected_row =
      in_list && view_.back_selected ? 0 : view_.selected - view_.first + (in_list ? 1U : 0U);
  view_.titles = {};
  view_.playing = {};
  const auto list = tracks_.playlist(view_.playlist);
  view_.list_title = in_list ? list.name : label("プレイリスト一覧");
  const bool active =
      view_.playback.state == api::State::Playing || view_.playback.state == api::State::Paused;
  std::uint32_t row = 0;
  if (in_list)
    view_.titles[row++] = label("＜ プレイリスト一覧へ");
  for (std::uint32_t i = view_.first; i < view_.count && row < kRows; ++i, ++row) {
    if (in_list) {
      const contracts::TrackId track{list.first.value + i};
      view_.titles[row] = tracks_.title(track);
      view_.playing[row] = active && track.value == view_.playback.track.value;
    } else {
      view_.titles[row] = tracks_.playlist({i + 1}).name;
      view_.playing[row] = active && i + 1 == view_.playback.playlist.value;
    }
  }
  view_.rows = row;
  ++view_.revision;
}
void Controller::update(const api::PlayerSnapshot snapshot) {
  if (snapshot.version != api::kVersion || snapshot.sequence == view_.playback.sequence)
    return;
  view_.playback = snapshot;
  view_.playing_title = tracks_.title(snapshot.track);
  const auto list = tracks_.playlist(snapshot.playlist);
  view_.playing_list = list.name;
  view_.playing_count = list.count;
  view_.playing_number =
      snapshot.track.value >= list.first.value && list.count != 0
          ? static_cast<std::uint32_t>(snapshot.track.value - list.first.value + 1)
          : 0;
  titles();
}
void Controller::activate() {
  if (view_.playlist.value == 0) {
    const auto list = tracks_.playlist({view_.selected + 1});
    if (list.id.value == 0 || list.id.value > positions_.size() || list.count == 0)
      return;
    list_position_ = view_.selected;
    view_.playlist = list.id;
    view_.count = list.count;
    view_.selected = std::min(positions_[list.id.value - 1], list.count - 1);
    view_.back_selected = false;
    titles();
  } else if (view_.back_selected) {
    positions_[view_.playlist.value - 1] = view_.selected;
    view_.playlist = {};
    view_.count = std::min(tracks_.playlist_count(), api::kMaxPlaylists);
    view_.selected = list_position_;
    view_.back_selected = false;
    titles();
  } else {
    const auto list = tracks_.playlist(view_.playlist);
    commands_.submit({api::CommandKind::PlayTrack, {list.first.value + view_.selected}});
  }
}
void Controller::input(const std::uint32_t buttons, const bool connected,
                       const std::uint32_t now_us) {
  if (!connected || !connected_) {
    previous_ = buttons;
    connected_ = connected;
    navigation_ = 0;
    return; // Held controls at boot/reconnect do not activate anything.
  }
  const auto pressed = buttons & ~previous_;
  previous_ = buttons;
  const auto direction =
      buttons & (bindings_.up | bindings_.down | bindings_.left | bindings_.right);
  bool move = false;
  if (direction != navigation_) {
    navigation_ = direction;
    repeated_at_ = now_us;
    repeating_ = false;
    move = true;
  } else if (direction != 0 && now_us - repeated_at_ >= (repeating_ ? 80'000U : 350'000U)) {
    move = true;
    repeating_ = true;
    repeated_at_ = now_us;
  }
  if (move && view_.count != 0) {
    const auto before = view_.selected;
    const bool back = view_.back_selected;
    const bool in_list = view_.playlist.value != 0;
    const auto capacity = in_list ? kRows - 1 : kRows;
    if (direction == bindings_.up) {
      if (in_list && !back && view_.selected == view_.first)
        view_.back_selected = true;
      else if (!back)
        view_.selected -= view_.selected != 0 ? 1U : 0U;
    } else if (direction == bindings_.down) {
      if (back)
        view_.back_selected = false;
      else
        view_.selected = std::min(view_.selected + 1, view_.count - 1);
    } else if (direction == bindings_.left || direction == bindings_.right) {
      view_.back_selected = false;
      if (direction == bindings_.left)
        view_.selected -= std::min(capacity, view_.selected);
      else
        view_.selected = std::min(view_.selected + capacity, view_.count - 1);
    }
    if (view_.selected != before || view_.back_selected != back)
      titles();
  }
  if ((pressed & bindings_.stop) != 0)
    commands_.submit({api::CommandKind::Stop, {}});
  else if ((pressed & bindings_.play) != 0 && view_.count != 0)
    activate();
  else if ((pressed & bindings_.pause) != 0)
    commands_.submit({api::CommandKind::TogglePause, {}});
  else if ((pressed & bindings_.repeat) != 0)
    commands_.submit({api::CommandKind::CycleRepeat, {}});
}
} // namespace rpcmp::ui::minimal
