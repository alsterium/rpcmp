#include "rpcmp/ui/player_ui.hpp"

namespace rpcmp::ui::v2 {

InputMapper::InputMapper(const InputBindings bindings) noexcept { rebind(bindings); }

void InputMapper::rebind(const InputBindings bindings) noexcept {
  bindings_ = bindings;
  std::uint16_t used = 0;
  valid_ = bindings.repeat_delay_us != 0 && bindings.repeat_period_us != 0;
  for (const auto mask : bindings.masks) {
    valid_ = valid_ && mask != 0 && (mask & (mask - 1U)) == 0 && (used & mask) == 0;
    used = static_cast<std::uint16_t>(used | mask);
  }
  connected_ = false;
}

Action InputMapper::sample(const InputSample input, const std::uint64_t now_us) noexcept {
  if (!valid_ || !input.connected) {
    connected_ = false;
    return Action::None;
  }
  if (!connected_ || now_us < last_time_) {
    connected_ = true;
    blocked_ = input.down;
    previous_ = 0;
    direction_ = Action::None;
    repeating_ = false;
    last_time_ = now_us;
    return Action::None;
  }
  last_time_ = now_us;
  blocked_ = static_cast<std::uint16_t>(blocked_ & input.down);
  const auto active = static_cast<std::uint16_t>(input.down & ~blocked_);
  const auto edges = static_cast<std::uint16_t>(active & ~previous_);
  previous_ = active;
  const auto held = [&](const Action action) {
    return (active & bindings_.masks[static_cast<std::size_t>(action) - 1]) != 0;
  };
  const auto pressed = [&](const Action action) {
    return (edges & bindings_.masks[static_cast<std::size_t>(action) - 1]) != 0;
  };
  auto direction = Action::None;
  if (held(Action::Up) != held(Action::Down))
    direction = held(Action::Up) ? Action::Up : Action::Down;
  else if (held(Action::Left) != held(Action::Right))
    direction = held(Action::Left) ? Action::Left : Action::Right;
  auto movement = Action::None;
  if (direction != direction_) {
    direction_ = direction;
    repeat_time_ = now_us;
    repeating_ = false;
    movement = direction;
  } else if (direction != Action::None &&
             now_us - repeat_time_ >=
                 (repeating_ ? bindings_.repeat_period_us : bindings_.repeat_delay_us)) {
    movement = direction;
    repeat_time_ = now_us;
    repeating_ = true;
  }
  if (pressed(Action::Back))
    return Action::Back;
  if (!(held(Action::Previous) && held(Action::Next))) {
    if (pressed(Action::Previous))
      return Action::Previous;
    if (pressed(Action::Next))
      return Action::Next;
  }
  if (pressed(Action::Confirm))
    return Action::Confirm;
  return movement;
}

FocusTable default_focus_table() noexcept {
  using F = Focus;
  return {{{F::Main, F::PlayPause, F::Main, F::PlayPause},
           {F::List, F::ViewSwitch, F::List, F::PlayPause},
           {F::List, F::Repeat, F::Previous, F::Stop},
           {F::List, F::Shuffle, F::PlayPause, F::Next},
           {F::List, F::Shuffle, F::Stop, F::Next},
           {F::Previous, F::ViewSwitch, F::List, F::Repeat},
           {F::PlayPause, F::Repeat, F::ViewSwitch, F::Shuffle},
           {F::Stop, F::Shuffle, F::Repeat, F::Shuffle}}};
}

} // namespace rpcmp::ui::v2
