#ifndef RPCMP_CONTRACTS_PLAYER_STATE_V2_HPP
#define RPCMP_CONTRACTS_PLAYER_STATE_V2_HPP

#include "rpcmp/contracts/catalog.hpp"

namespace rpcmp::contracts::v2 {

inline constexpr std::uint16_t kSchemaVersion = 2;
inline constexpr std::uint32_t kFrameRate = 48'000;
inline constexpr std::uint64_t kTransportObservations = 1ULL << 0U;
inline constexpr std::uint64_t kStatePreservingPause = 1ULL << 1U;

enum class TransportIntentKind : std::uint8_t {
  PlayTrack = 0,
  LoadTrack = 1,
  Play = 2,
  Pause = 3,
  Resume = 4,
  TogglePause = 5,
  Stop = 6
};
struct TrackSelection {
  LibraryGeneration library_generation{};
  TrackId track_id{};
};
struct SelectedTrack {
  CatalogTrackItem item{};
  CatalogText album_name{};
};
struct PendingIntent {
  std::uint64_t command_id{};
  TransportIntentKind kind{TransportIntentKind::Stop};
  std::optional<TrackSelection> selection;
};
struct PendingPreparation {
  std::uint64_t operation_id{};
  std::uint64_t play_generation{};
  TrackSelection selection{};
  bool cancel_requested{};
};
enum class AudioControlKind : std::uint8_t { Reset = 0, Start = 1, Pause = 2, Resume = 3 };
struct PendingAudioControl {
  std::uint64_t operation_id{};
  std::uint64_t play_generation{};
  AudioControlKind kind{AudioControlKind::Reset};
};
enum class PlaybackErrorCode : std::uint8_t {
  Library = 1,
  Preparation = 2,
  PreparationTimeout = 3,
  AudioControl = 4,
  DeviceFault = 5,
  AudioTimeout = 6,
  ResetFailed = 7,
  Protocol = 8,
  Clock = 9,
  InvalidConfiguration = 10,
  ResourceExhausted = 11
};
struct PlaybackError {
  PlaybackErrorCode code{PlaybackErrorCode::Protocol};
  bool terminal{};
};
struct PlayerSnapshot {
  std::uint16_t schema_version{kSchemaVersion};
  std::uint64_t sequence{};
  std::uint64_t published_at_us{};
  CapabilitySet capabilities{kTransportObservations};
  CatalogStatus library{};
  TransportState transport{TransportState::Empty};
  TransportState projected{TransportState::Empty};
  std::optional<SelectedTrack> track;
  std::uint64_t play_generation{};
  std::uint64_t position_frames{};
  std::uint32_t frame_rate{kFrameRate};
  bool prepared{};
  bool silence_confirmed{};
  std::optional<PendingIntent> pending_intent;
  std::optional<PendingPreparation> preparation;
  std::optional<PendingAudioControl> audio_control;
  std::optional<PlaybackError> error;
};

class SnapshotSource {
public:
  virtual ~SnapshotSource() = default;
  [[nodiscard]] virtual PlayerSnapshot latest() const noexcept = 0;
};

[[nodiscard]] bool valid_player_snapshot(const PlayerSnapshot& snapshot) noexcept;

} // namespace rpcmp::contracts::v2

#endif // RPCMP_CONTRACTS_PLAYER_STATE_V2_HPP
