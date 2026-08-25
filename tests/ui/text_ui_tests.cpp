#include "rpcmp/ui/text_ui.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace rpcmp;

contracts::PlayerSnapshot fixture() {
  contracts::PlayerSnapshot snapshot;
  snapshot.position.tick_rate = 60'000;
  snapshot.transport = contracts::TransportState::Playing;
  snapshot.track = contracts::TrackSummary{contracts::TrackId{1}, "Mock Track", "RPCMP",
                                           std::nullopt, std::nullopt};
  snapshot.devices.push_back(
      {contracts::DeviceId{1}, contracts::DeviceType::Ym2151, std::uint16_t{0}, std::uint16_t{8},
       contracts::DeviceOperationalState::Mock, contracts::CapabilitySet{0}});
  for (std::uint16_t index = 0; index < 8; ++index) {
    contracts::ChannelState channel;
    channel.channel_id = contracts::ChannelId{index};
    channel.label = "FM " + std::to_string(index + 1U);
    channel.kind = contracts::ChannelKind::Fm;
    channel.enabled = true;
    channel.key_on = index == 0;
    channel.note = index == 0 ? std::optional<std::uint8_t>{std::uint8_t{60}} : std::nullopt;
    channel.activity = static_cast<std::uint8_t>(index * 10U);
    channel.device_id = contracts::DeviceId{1};
    channel.device_channel = index;
    snapshot.channels.push_back(channel);
  }
  snapshot.extensions.push_back({0xABCD1234U, 999, {1, 2, 3, 4}});
  return snapshot;
}

bool contains(const std::vector<std::string>& lines, const std::string& text) {
  return std::any_of(lines.begin(), lines.end(), [&text](const std::string& line) {
    return line.find(text) != std::string::npos;
  });
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  const auto snapshot = fixture();
  rpcmp::ui::UiState state;

  const auto overview = rpcmp::ui::render(snapshot, state);
  RPCMP_CHECK(suite, contains(overview, "Overview"));
  RPCMP_CHECK(suite, contains(overview, "Mock Track"));
  RPCMP_CHECK(suite, contains(overview, "album: —"));

  rpcmp::ui::apply_action(state, rpcmp::ui::UiAction::SwitchView, snapshot.channels.size());
  const auto channels = rpcmp::ui::render(snapshot, state);
  RPCMP_CHECK(suite, contains(channels, "Channels"));
  RPCMP_CHECK(suite, channels.size() == 9U);
  RPCMP_CHECK(suite, contains(channels, "FM 8"));
  RPCMP_CHECK(suite, contains(channels, "note=—"));

  rpcmp::ui::apply_action(state, rpcmp::ui::UiAction::SelectPrevious, snapshot.channels.size());
  RPCMP_CHECK(suite, state.selected_channel == 7U);
  rpcmp::ui::apply_action(state, rpcmp::ui::UiAction::SelectNext, snapshot.channels.size());
  RPCMP_CHECK(suite, state.selected_channel == 0U);

  auto missing = snapshot;
  missing.track.reset();
  missing.error = contracts::PlayerError{contracts::ErrorDomain::Unsupported, 7,
                                         contracts::ErrorSeverity::Recoverable, true, std::nullopt};
  state.view = rpcmp::ui::UiView::Overview;
  const auto fallback = rpcmp::ui::render(missing, state);
  RPCMP_CHECK(suite, contains(fallback, "track: —"));
  RPCMP_CHECK(suite, contains(fallback, "error: —"));

  return suite.finish("ui");
}
