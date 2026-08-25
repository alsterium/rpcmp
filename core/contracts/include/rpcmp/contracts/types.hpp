#ifndef RPCMP_CONTRACTS_TYPES_HPP
#define RPCMP_CONTRACTS_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace rpcmp::contracts {

inline constexpr std::uint16_t kSchemaVersion = 1;
inline constexpr std::size_t kMaxChannels = 16;
inline constexpr std::size_t kMaxDevices = 4;
inline constexpr std::size_t kMaxExtensions = 8;
inline constexpr std::size_t kMaxExtensionPayloadBytes = 256;
inline constexpr std::size_t kMaxVisualizationSamples = 64;
inline constexpr std::size_t kMaxMetadataBytes = 96;
inline constexpr std::size_t kMaxChannelLabelBytes = 24;
inline constexpr std::size_t kMaxInstrumentLabelBytes = 48;
inline constexpr std::size_t kMaxErrorDetailBytes = 160;
inline constexpr std::size_t kMaxLibraryReferenceBytes = 96;

struct TrackId {
  std::uint64_t value{};
};

struct ChannelId {
  std::uint16_t value{};
};

struct DeviceId {
  std::uint16_t value{};
};

constexpr bool operator==(TrackId left, TrackId right) noexcept {
  return left.value == right.value;
}
constexpr bool operator!=(TrackId left, TrackId right) noexcept { return !(left == right); }
constexpr bool operator==(ChannelId left, ChannelId right) noexcept {
  return left.value == right.value;
}
constexpr bool operator!=(ChannelId left, ChannelId right) noexcept { return !(left == right); }
constexpr bool operator==(DeviceId left, DeviceId right) noexcept {
  return left.value == right.value;
}
constexpr bool operator!=(DeviceId left, DeviceId right) noexcept { return !(left == right); }

enum class TransportState : std::uint8_t {
  Empty,
  Loading,
  Stopped,
  Playing,
  Paused,
  Ended,
  Error,
};

enum class ChannelKind : std::uint8_t { Fm, Pcm, Psg, Other };
enum class DeviceType : std::uint8_t { Ym2151, Other };
enum class DeviceOperationalState : std::uint8_t { Mock, Ready, Reset, Fault };
enum class ErrorDomain : std::uint8_t {
  Library,
  Format,
  Device,
  Resource,
  Unsupported,
  Internal,
};
enum class ErrorSeverity : std::uint8_t { Info, Warning, Recoverable, Fatal };

struct CapabilitySet {
  std::uint64_t bits{};
};

struct TrackSummary {
  TrackId track_id{};
  std::string title;
  std::string artist;
  std::optional<std::string> album;
  std::optional<std::string> composer;
};

struct PositionState {
  std::uint64_t position_ticks{};
  std::uint32_t tick_rate{};
  std::optional<std::uint64_t> duration_ticks;
  std::optional<std::uint64_t> loop_start_ticks;
  std::optional<std::uint32_t> loop_count;
  bool seekable{};
};

struct ChannelState {
  ChannelId channel_id{};
  std::string label;
  ChannelKind kind{ChannelKind::Other};
  bool enabled{};
  bool mute_capable{};
  bool muted{};
  bool solo_capable{};
  bool solo{};
  bool key_on{};
  std::optional<std::uint8_t> note;
  std::optional<std::int16_t> fine_pitch_cents;
  std::uint8_t level{};
  std::uint8_t activity{};
  std::optional<std::int8_t> pan;
  std::optional<std::string> instrument_label;
  DeviceId device_id{};
  std::uint16_t device_channel{};
};

struct DeviceSummary {
  DeviceId device_id{};
  DeviceType type{DeviceType::Other};
  std::uint16_t first_channel{};
  std::uint16_t channel_count{};
  DeviceOperationalState operational_state{DeviceOperationalState::Reset};
  CapabilitySet capabilities{};
};

struct VisualizationState {
  std::vector<std::uint8_t> recent_activity;
};

struct PlayerError {
  ErrorDomain domain{ErrorDomain::Internal};
  std::uint32_t code{};
  ErrorSeverity severity{ErrorSeverity::Recoverable};
  bool recoverable{};
  std::optional<std::string> detail;
};

struct StateExtension {
  std::uint32_t type_id{};
  std::uint16_t version{};
  std::vector<std::uint8_t> payload;
};

struct PlayerSnapshot {
  std::uint16_t schema_version{kSchemaVersion};
  std::uint64_t sequence{};
  std::uint64_t published_at_tick{};
  CapabilitySet capabilities{};
  TransportState transport{TransportState::Empty};
  std::optional<TrackSummary> track;
  PositionState position{};
  std::vector<ChannelState> channels;
  std::vector<DeviceSummary> devices;
  VisualizationState visualization;
  std::optional<PlayerError> error;
  std::vector<StateExtension> extensions;
};

enum class ContractError : std::uint8_t {
  None,
  UnsupportedSchema,
  InvalidUtf8,
  ValueTooLong,
  TooManyChannels,
  TooManyDevices,
  TooManyExtensions,
  ExtensionPayloadTooLarge,
  VisualizationTooLarge,
  InvalidTickRate,
  InvalidNote,
  InvalidFinePitch,
  InvalidPan,
  DuplicateChannel,
  DuplicateDevice,
  UnknownDevice,
  InvalidDeviceChannel,
};

struct ContractValidationResult {
  ContractError error{ContractError::None};
  bool ok() const noexcept { return error == ContractError::None; }
};

bool is_valid_utf8(const std::string& value) noexcept;
ContractValidationResult validate_snapshot(const PlayerSnapshot& snapshot) noexcept;

} // namespace rpcmp::contracts

#endif // RPCMP_CONTRACTS_TYPES_HPP
