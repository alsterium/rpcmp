#include "rpcmp/contracts/types.hpp"

#include <algorithm>
#include <limits>

namespace rpcmp::contracts {
namespace {

bool valid_bounded_text(const std::string& value, const std::size_t maximum) noexcept {
  return value.size() <= maximum && is_valid_utf8(value);
}

ContractError validate_optional_text(const std::optional<std::string>& value,
                                     const std::size_t maximum) noexcept {
  if (!value.has_value()) {
    return ContractError::None;
  }
  if (value->size() > maximum) {
    return ContractError::ValueTooLong;
  }
  return is_valid_utf8(*value) ? ContractError::None : ContractError::InvalidUtf8;
}

bool has_device(const PlayerSnapshot& snapshot, const DeviceId id) noexcept {
  return std::any_of(snapshot.devices.begin(), snapshot.devices.end(),
                     [id](const DeviceSummary& device) { return device.device_id == id; });
}

} // namespace

bool is_valid_utf8(const std::string& value) noexcept {
  std::size_t index = 0;
  while (index < value.size()) {
    const auto first = static_cast<std::uint8_t>(value[index]);
    std::size_t continuation_count = 0;
    std::uint32_t code_point = 0;
    if (first <= 0x7FU) {
      ++index;
      continue;
    }
    if ((first & 0xE0U) == 0xC0U) {
      continuation_count = 1;
      code_point = first & 0x1FU;
      if (code_point == 0) {
        return false;
      }
    } else if ((first & 0xF0U) == 0xE0U) {
      continuation_count = 2;
      code_point = first & 0x0FU;
    } else if ((first & 0xF8U) == 0xF0U) {
      continuation_count = 3;
      code_point = first & 0x07U;
    } else {
      return false;
    }
    if (index + continuation_count >= value.size()) {
      return false;
    }
    for (std::size_t offset = 1; offset <= continuation_count; ++offset) {
      const auto next = static_cast<std::uint8_t>(value[index + offset]);
      if ((next & 0xC0U) != 0x80U) {
        return false;
      }
      code_point = (code_point << 6U) | (next & 0x3FU);
    }
    const bool overlong = (continuation_count == 1 && code_point < 0x80U) ||
                          (continuation_count == 2 && code_point < 0x800U) ||
                          (continuation_count == 3 && code_point < 0x10000U);
    if (overlong || code_point > 0x10FFFFU || (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
      return false;
    }
    index += continuation_count + 1;
  }
  return true;
}

ContractValidationResult validate_snapshot(const PlayerSnapshot& snapshot) noexcept {
  const auto fail = [](const ContractError error) { return ContractValidationResult{error}; };
  if (snapshot.schema_version != kSchemaVersion) {
    return fail(ContractError::UnsupportedSchema);
  }
  if (snapshot.position.tick_rate == 0) {
    return fail(ContractError::InvalidTickRate);
  }
  if (snapshot.channels.size() > kMaxChannels) {
    return fail(ContractError::TooManyChannels);
  }
  if (snapshot.devices.size() > kMaxDevices) {
    return fail(ContractError::TooManyDevices);
  }
  if (snapshot.extensions.size() > kMaxExtensions) {
    return fail(ContractError::TooManyExtensions);
  }
  if (snapshot.visualization.recent_activity.size() > kMaxVisualizationSamples) {
    return fail(ContractError::VisualizationTooLarge);
  }
  if (snapshot.track.has_value()) {
    const auto& track = *snapshot.track;
    if (!valid_bounded_text(track.title, kMaxMetadataBytes) ||
        !valid_bounded_text(track.artist, kMaxMetadataBytes)) {
      return fail(track.title.size() > kMaxMetadataBytes || track.artist.size() > kMaxMetadataBytes
                      ? ContractError::ValueTooLong
                      : ContractError::InvalidUtf8);
    }
    for (const auto error : {validate_optional_text(track.album, kMaxMetadataBytes),
                             validate_optional_text(track.composer, kMaxMetadataBytes)}) {
      if (error != ContractError::None) {
        return fail(error);
      }
    }
  }
  for (std::size_t index = 0; index < snapshot.devices.size(); ++index) {
    for (std::size_t other = index + 1; other < snapshot.devices.size(); ++other) {
      if (snapshot.devices[index].device_id == snapshot.devices[other].device_id) {
        return fail(ContractError::DuplicateDevice);
      }
    }
  }
  for (std::size_t index = 0; index < snapshot.channels.size(); ++index) {
    const auto& channel = snapshot.channels[index];
    if (channel.label.size() > kMaxChannelLabelBytes ||
        (channel.instrument_label.has_value() &&
         channel.instrument_label->size() > kMaxInstrumentLabelBytes)) {
      return fail(ContractError::ValueTooLong);
    }
    if (!is_valid_utf8(channel.label) ||
        (channel.instrument_label.has_value() && !is_valid_utf8(*channel.instrument_label))) {
      return fail(ContractError::InvalidUtf8);
    }
    if (channel.note.has_value() && *channel.note > 127U) {
      return fail(ContractError::InvalidNote);
    }
    if (channel.fine_pitch_cents.has_value() &&
        (*channel.fine_pitch_cents < -100 || *channel.fine_pitch_cents > 100)) {
      return fail(ContractError::InvalidFinePitch);
    }
    if (channel.pan.has_value() && *channel.pan == std::numeric_limits<std::int8_t>::min()) {
      return fail(ContractError::InvalidPan);
    }
    if (!has_device(snapshot, channel.device_id)) {
      return fail(ContractError::UnknownDevice);
    }
    for (std::size_t other = index + 1; other < snapshot.channels.size(); ++other) {
      if (channel.channel_id == snapshot.channels[other].channel_id) {
        return fail(ContractError::DuplicateChannel);
      }
    }
    const auto device = std::find_if(snapshot.devices.begin(), snapshot.devices.end(),
                                     [&channel](const DeviceSummary& candidate) {
                                       return candidate.device_id == channel.device_id;
                                     });
    if (device != snapshot.devices.end() && channel.device_channel >= device->channel_count) {
      return fail(ContractError::InvalidDeviceChannel);
    }
  }
  for (const auto& extension : snapshot.extensions) {
    if (extension.payload.size() > kMaxExtensionPayloadBytes) {
      return fail(ContractError::ExtensionPayloadTooLarge);
    }
  }
  if (snapshot.error.has_value() && snapshot.error->detail.has_value()) {
    if (snapshot.error->detail->size() > kMaxErrorDetailBytes) {
      return fail(ContractError::ValueTooLong);
    }
    if (!is_valid_utf8(*snapshot.error->detail)) {
      return fail(ContractError::InvalidUtf8);
    }
  }
  return {};
}

} // namespace rpcmp::contracts
