#include "rpcmp/ui/text_ui.hpp"

#include <algorithm>
#include <string>

namespace rpcmp::ui {
namespace {

std::string transport_name(const contracts::TransportState state) {
  using contracts::TransportState;
  switch (state) {
  case TransportState::Empty:
    return "empty";
  case TransportState::Loading:
    return "loading";
  case TransportState::Stopped:
    return "stopped";
  case TransportState::Playing:
    return "playing";
  case TransportState::Paused:
    return "paused";
  case TransportState::Ended:
    return "ended";
  case TransportState::Error:
    return "error";
  }
  return "unknown";
}

std::string optional_text(const std::optional<std::string>& value) {
  return value.has_value() ? *value : "—";
}

std::string note_text(const std::optional<std::uint8_t>& note) {
  return note.has_value() ? std::to_string(static_cast<unsigned int>(*note)) : "—";
}

std::string pan_text(const std::optional<std::int8_t>& pan) {
  return pan.has_value() ? std::to_string(static_cast<int>(*pan)) : "—";
}

std::vector<std::string> render_overview(const contracts::PlayerSnapshot& snapshot) {
  std::vector<std::string> lines;
  lines.push_back("RPCMP M0 / Overview");
  lines.push_back("transport: " + transport_name(snapshot.transport));
  lines.push_back("position: " + std::to_string(snapshot.position.position_ticks) + "/" +
                  std::to_string(snapshot.position.tick_rate) + " ticks");
  if (snapshot.track.has_value()) {
    lines.push_back("track: " + snapshot.track->title);
    lines.push_back("artist: " + snapshot.track->artist);
    lines.push_back("album: " + optional_text(snapshot.track->album));
  } else {
    lines.push_back("track: —");
    lines.push_back("artist: —");
    lines.push_back("album: —");
  }
  lines.push_back("devices: " + std::to_string(snapshot.devices.size()));
  if (snapshot.error.has_value()) {
    lines.push_back("error: " + optional_text(snapshot.error->detail));
  } else {
    lines.push_back("error: —");
  }
  return lines;
}

std::vector<std::string> render_channels(const contracts::PlayerSnapshot& snapshot,
                                         const UiState& state) {
  std::vector<std::string> lines;
  lines.push_back("RPCMP M0 / Channels");
  for (std::size_t index = 0; index < snapshot.channels.size(); ++index) {
    const auto& channel = snapshot.channels[index];
    std::string flags;
    flags += channel.enabled ? "enabled" : "disabled";
    if (channel.muted) {
      flags += ",mute";
    }
    if (channel.solo) {
      flags += ",solo";
    }
    const auto marker = index == state.selected_channel ? "> " : "  ";
    lines.push_back(
        marker + channel.label + " [" + flags + "] key=" + (channel.key_on ? "on" : "off") +
        " note=" + note_text(channel.note) + " activity=" + std::to_string(channel.activity) +
        " pan=" + pan_text(channel.pan));
  }
  if (snapshot.channels.empty()) {
    lines.push_back("—");
  }
  return lines;
}

} // namespace

void apply_action(UiState& state, const UiAction action, const std::size_t channel_count) noexcept {
  switch (action) {
  case UiAction::SwitchView:
    state.view = state.view == UiView::Overview ? UiView::Channels : UiView::Overview;
    return;
  case UiAction::SelectNext:
    if (channel_count != 0) {
      state.selected_channel = (state.selected_channel + 1U) % channel_count;
    }
    return;
  case UiAction::SelectPrevious:
    if (channel_count != 0) {
      state.selected_channel = (state.selected_channel + channel_count - 1U) % channel_count;
    }
    return;
  }
}

std::vector<std::string> render(const contracts::PlayerSnapshot& snapshot, const UiState& state) {
  if (state.view == UiView::Channels) {
    return render_channels(snapshot, state);
  }
  return render_overview(snapshot);
}

} // namespace rpcmp::ui
