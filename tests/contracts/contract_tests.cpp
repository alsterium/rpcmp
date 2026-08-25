#include "rpcmp/contracts/types.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

using namespace rpcmp::contracts;

PlayerSnapshot valid_snapshot() {
  PlayerSnapshot snapshot;
  snapshot.position.tick_rate = 60'000;
  snapshot.track = TrackSummary{TrackId{1}, "Synthetic", "RPCMP", std::nullopt, std::nullopt};
  snapshot.devices.push_back(DeviceSummary{DeviceId{1}, DeviceType::Ym2151, std::uint16_t{0},
                                           std::uint16_t{8}, DeviceOperationalState::Mock,
                                           CapabilitySet{0}});
  ChannelState channel;
  channel.channel_id = ChannelId{0};
  channel.label = "FM 1";
  channel.kind = ChannelKind::Fm;
  channel.enabled = true;
  channel.note = std::uint8_t{60};
  channel.fine_pitch_cents = std::int16_t{0};
  channel.pan = std::int8_t{0};
  channel.instrument_label = std::string{"Mock FM 1"};
  channel.device_id = DeviceId{1};
  channel.device_channel = 0;
  snapshot.channels.push_back(channel);
  return snapshot;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;

  auto snapshot = valid_snapshot();
  RPCMP_CHECK(suite, validate_snapshot(snapshot).ok());
  RPCMP_CHECK(suite, is_valid_utf8("RPCMP 日本語"));
  RPCMP_CHECK(suite, !is_valid_utf8(std::string{"\xC0\xAF", 2}));

  snapshot.extensions.push_back(
      StateExtension{0xDEADBEEFU, 77, std::vector<std::uint8_t>(256, 0xA5U)});
  RPCMP_CHECK(suite, validate_snapshot(snapshot).ok());
  snapshot.extensions.front().payload.push_back(0);
  RPCMP_CHECK(suite, validate_snapshot(snapshot).error == ContractError::ExtensionPayloadTooLarge);

  snapshot = valid_snapshot();
  snapshot.channels.front().note.reset();
  snapshot.channels.front().instrument_label.reset();
  RPCMP_CHECK(suite, validate_snapshot(snapshot).ok());

  snapshot.channels.front().label = std::string(25, 'x');
  RPCMP_CHECK(suite, validate_snapshot(snapshot).error == ContractError::ValueTooLong);

  snapshot = valid_snapshot();
  snapshot.channels.front().device_id = DeviceId{99};
  RPCMP_CHECK(suite, validate_snapshot(snapshot).error == ContractError::UnknownDevice);

  snapshot = valid_snapshot();
  snapshot.channels.push_back(snapshot.channels.front());
  RPCMP_CHECK(suite, validate_snapshot(snapshot).error == ContractError::DuplicateChannel);

  snapshot = valid_snapshot();
  snapshot.schema_version = 2;
  RPCMP_CHECK(suite, validate_snapshot(snapshot).error == ContractError::UnsupportedSchema);

  return suite.finish("contracts");
}
