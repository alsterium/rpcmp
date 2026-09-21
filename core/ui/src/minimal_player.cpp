#include "rpcmp/ui/minimal_player.hpp"

#include <algorithm>

namespace rpcmp::ui::minimal {
Controller::Controller(const api::TrackList& tracks, api::CommandSink& commands,
                       const Bindings bindings)
    : tracks_(tracks), commands_(commands), bindings_(bindings) {
  view_.count = tracks_.count();
  titles();
}
void Controller::titles() {
  view_.first = view_.selected / kRows * kRows;
  for (std::uint32_t row = 0; row < kRows; ++row)
    view_.titles[row] = tracks_.title({view_.first + row + 1});
  ++view_.revision;
}
void Controller::update(const api::PlayerSnapshot snapshot) {
  if (snapshot.version != api::kVersion || snapshot.sequence == view_.playback.sequence)
    return;
  view_.playback = snapshot;
  view_.playing_title = tracks_.title(snapshot.track);
  ++view_.revision;
}
void Controller::input(const std::uint32_t buttons, const bool connected,
                       const std::uint32_t now_us) {
  if (!connected || !connected_) {
    previous_ = buttons;
    connected_ = connected;
    navigation_ = 0;
    return; // A held at boot/reconnect must not start playback.
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
    if (direction == bindings_.up)
      view_.selected -= view_.selected != 0 ? 1U : 0U;
    else if (direction == bindings_.down)
      view_.selected = std::min(view_.selected + 1, view_.count - 1);
    else if (direction == bindings_.left)
      view_.selected -= std::min(kRows, view_.selected);
    else if (direction == bindings_.right)
      view_.selected = std::min(view_.selected + kRows, view_.count - 1);
    if (view_.selected != before)
      titles();
  }
  if ((pressed & bindings_.stop) != 0)
    commands_.submit({api::CommandKind::Stop, {}});
  else if ((pressed & bindings_.play) != 0 && view_.count != 0)
    commands_.submit({api::CommandKind::PlayTrack, {view_.selected + 1}});
  else if ((pressed & bindings_.pause) != 0)
    commands_.submit({api::CommandKind::TogglePause, {}});
}
} // namespace rpcmp::ui::minimal
