#include "rpcmp/ui/text_ui.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace {

rpcmp::contracts::PlayerSnapshot fixture() {
  using namespace rpcmp::contracts;
  PlayerSnapshot snapshot;
  snapshot.position.tick_rate = 60'000;
  snapshot.transport = TransportState::Playing;
  snapshot.track = TrackSummary{TrackId{1}, "UI Fixture", "RPCMP", std::nullopt, std::nullopt};
  snapshot.devices.push_back(DeviceSummary{DeviceId{1}, DeviceType::Ym2151, 0, 8,
                                           DeviceOperationalState::Mock, CapabilitySet{0}});
  for (std::uint16_t index = 0; index < 8; ++index) {
    ChannelState channel;
    channel.channel_id = ChannelId{index};
    channel.label = "FM " + std::to_string(index + 1U);
    channel.kind = ChannelKind::Fm;
    channel.enabled = true;
    channel.key_on = index % 2U == 0U;
    channel.activity = static_cast<std::uint8_t>(index * 16U);
    channel.device_id = DeviceId{1};
    channel.device_channel = index;
    snapshot.channels.push_back(channel);
  }
  snapshot.extensions.push_back(StateExtension{0xFFFF0001U, 99, {1, 2, 3}});
  return snapshot;
}

} // namespace

int main() {
  const auto snapshot = fixture();
  rpcmp::ui::UiState state;
  for (const auto& line : rpcmp::ui::render(snapshot, state)) {
    std::cout << line << '\n';
  }
  rpcmp::ui::apply_action(state, rpcmp::ui::UiAction::SwitchView, snapshot.channels.size());
  for (const auto& line : rpcmp::ui::render(snapshot, state)) {
    std::cout << line << '\n';
  }
  return 0;
}
