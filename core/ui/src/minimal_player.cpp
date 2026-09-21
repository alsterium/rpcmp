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
void Controller::reset_scroll() {
  selected_at_ = now_;
  view_.scroll_tick = 0;
}
void Controller::titles() {
  const bool in_list = view_.playlist.value != 0;
  view_.first = view_.selected / kRows * kRows;
  view_.selected_row = view_.selected - view_.first;
  view_.titles = {};
  view_.playing = {};
  const auto list = tracks_.playlist(view_.playlist);
  view_.list_title = in_list ? list.name : label("プレイリスト一覧");
  const bool active =
      view_.playback.state == api::State::Playing || view_.playback.state == api::State::Paused;
  std::uint32_t row = 0;
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
  view_.playing_title = tracks_.title(snapshot.last_played);
  const auto list = tracks_.playlist(tracks_.playlist_for(snapshot.last_played));
  view_.playing_list = list.name;
  view_.playing_count = list.count;
  const auto track = snapshot.last_played.value;
  view_.playing_number =
      list.count != 0 && track >= list.first.value && track < list.first.value + list.count
          ? static_cast<std::uint32_t>(track - list.first.value + 1)
          : 0;
  const bool valid = view_.playing_number != 0;
  const bool operable = snapshot.error != api::Error::Reset;
  view_.enabled = {operable && valid && view_.playing_number > 1,
                   operable && valid && snapshot.state != api::State::Advancing,
                   operable && valid && view_.playing_number < list.count,
                   operable && snapshot.state != api::State::Stopped, operable};
  titles();
}
void Controller::back() {
  if (view_.panel == Panel::Controls) {
    commands_.submit({api::CommandKind::Stop, {}});
  } else if (view_.playlist.value != 0) {
    positions_[view_.playlist.value - 1] = view_.selected;
    view_.playlist = {};
    view_.count = std::min(tracks_.playlist_count(), api::kMaxPlaylists);
    view_.selected = list_position_;
    reset_scroll();
    titles();
  }
}
void Controller::activate() {
  if (view_.panel == Panel::Controls) {
    const auto index = static_cast<std::size_t>(view_.icon);
    if (view_.enabled[index]) {
      constexpr std::array<api::CommandKind, 5> commands{
          api::CommandKind::Previous, api::CommandKind::PlayPause, api::CommandKind::Next,
          api::CommandKind::Stop, api::CommandKind::CycleRepeat};
      commands_.submit({commands[index], {}});
    }
    return;
  }
  if (view_.count == 0)
    return;
  if (view_.playlist.value == 0) {
    const auto list = tracks_.playlist({view_.selected + 1});
    if (list.id.value == 0 || list.id.value > positions_.size() || list.count == 0)
      return;
    list_position_ = view_.selected;
    view_.playlist = list.id;
    view_.count = list.count;
    view_.selected = std::min(positions_[list.id.value - 1], list.count - 1);
    reset_scroll();
    titles();
  } else {
    const auto list = tracks_.playlist(view_.playlist);
    commands_.submit({api::CommandKind::PlayTrack, {list.first.value + view_.selected}});
  }
}
void Controller::move(const std::uint32_t direction) {
  if (view_.panel == Panel::Controls) {
    // Columns: Previous / PlayPause / Next; Stop / Repeat (spans two columns).
    constexpr std::array<Icon, 5> up{Icon::Previous, Icon::PlayPause, Icon::Next, Icon::Previous,
                                     Icon::PlayPause};
    constexpr std::array<Icon, 5> down{Icon::Stop, Icon::Repeat, Icon::Repeat, Icon::Stop,
                                       Icon::Repeat};
    constexpr std::array<Icon, 5> left{Icon::Previous, Icon::Previous, Icon::PlayPause, Icon::Stop,
                                       Icon::Stop};
    constexpr std::array<Icon, 5> right{Icon::PlayPause, Icon::Next, Icon::Next, Icon::Repeat,
                                        Icon::Repeat};
    const auto index = static_cast<std::size_t>(view_.icon);
    const auto before = view_.icon;
    if (direction == bindings_.up)
      view_.icon = up[index];
    else if (direction == bindings_.down)
      view_.icon = down[index];
    else if (direction == bindings_.left)
      view_.icon = left[index];
    else if (direction == bindings_.right)
      view_.icon = right[index];
    if (before != view_.icon)
      ++view_.revision;
    return;
  }
  if (view_.count == 0)
    return;
  const auto before = view_.selected;
  if (direction == bindings_.up)
    view_.selected -= view_.selected != 0 ? 1U : 0U;
  else if (direction == bindings_.down)
    view_.selected = std::min(view_.selected + 1, view_.count - 1);
  else if (direction == bindings_.left)
    view_.selected -= std::min(kRows, view_.selected);
  else if (direction == bindings_.right)
    view_.selected = std::min(view_.selected + kRows, view_.count - 1);
  if (view_.selected != before) {
    reset_scroll();
    titles();
  }
}
void Controller::input(std::uint32_t buttons, const bool connected, const std::uint32_t now_us) {
  now_ = now_us;
  if (!connected || !connected_) {
    previous_ = buttons;
    blocked_ = buttons;
    connected_ = connected;
    navigation_ = 0;
    reset_scroll();
    return;
  }
  blocked_ &= buttons;
  buttons &= ~blocked_;
  const auto pressed = buttons & ~previous_;
  previous_ = buttons;
  const auto directions = bindings_.up | bindings_.down | bindings_.left | bindings_.right;
  const auto direction = buttons & directions;
  // A single action per observation. Suppress held directions after action/panel changes.
  if ((pressed & (bindings_.back | bindings_.confirm | bindings_.panel_l | bindings_.panel_r)) !=
      0) {
    blocked_ |= direction;
    navigation_ = 0;
    if ((pressed & bindings_.back) != 0)
      back();
    else if ((pressed & bindings_.confirm) != 0)
      activate();
    else {
      view_.panel = view_.panel == Panel::List ? Panel::Controls : Panel::List;
      reset_scroll();
      ++view_.revision;
    }
  } else {
    if (direction != navigation_) {
      navigation_ = direction;
      repeated_at_ = now_us;
      repeating_ = false;
      move(direction);
    } else if (direction != 0 && now_us - repeated_at_ >= (repeating_ ? 80'000U : 350'000U)) {
      repeating_ = true;
      repeated_at_ = now_us;
      move(direction);
    }
  }
  view_.scroll_tick = view_.panel == Panel::List ? (now_us - selected_at_) / 50'000 : 0;
}
} // namespace rpcmp::ui::minimal
