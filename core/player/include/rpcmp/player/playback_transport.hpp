#ifndef RPCMP_PLAYER_PLAYBACK_TRANSPORT_HPP
#define RPCMP_PLAYER_PLAYBACK_TRANSPORT_HPP

#include "rpcmp/player/catalog_session.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace rpcmp::player {

inline constexpr std::uint16_t kTransportBatchCapacity = 32;
inline constexpr std::uint32_t kPlaybackFrameRate = 48'000;

struct PlaybackSelection {
  contracts::LibraryGeneration library_generation{};
  contracts::TrackId track_id{};
};
enum class TransportIntentKind : std::uint8_t {
  PlayTrack,
  LoadTrack,
  Play,
  Pause,
  Resume,
  TogglePause,
  Stop
};
struct TransportIntent {
  std::uint64_t command_id{};
  TransportIntentKind kind{TransportIntentKind::Stop};
  PlaybackSelection selection{};
};
struct TransportBatch {
  std::array<TransportIntent, kTransportBatchCapacity> intents{};
  std::uint16_t count{};
};
enum class TransportRejection : std::uint8_t {
  None,
  Malformed,
  InvalidState,
  LibraryUnavailable,
  StaleLibrary,
  UnknownTrack,
  UnsupportedPause,
  Busy,
  TerminalFailure,
  BatchTooLarge
};
struct TransportDecision {
  std::uint64_t command_id{};
  TransportRejection rejection{TransportRejection::None};
  [[nodiscard]] bool accepted() const noexcept { return rejection == TransportRejection::None; }
};
struct TransportStepResult {
  TransportRejection batch_error{TransportRejection::None};
  std::array<TransportDecision, kTransportBatchCapacity> decisions{};
  std::uint16_t count{};
};

struct PreparationRequest {
  std::uint64_t operation_id{};
  std::uint64_t play_generation{};
  PlaybackSelection selection{};
};
enum class PreparationProgress : std::uint8_t { Pending, Ready, Failed, Cancelled };
class PreparationPort {
public:
  virtual ~PreparationPort() = default;
  [[nodiscard]] virtual bool begin(const PreparationRequest& request) = 0;
  virtual void cancel(std::uint64_t operation_id) = 0;
  [[nodiscard]] virtual PreparationProgress poll(std::uint64_t operation_id) = 0;
  virtual void release() = 0;
};

enum class AudioControlKind : std::uint8_t { Reset, Start, Pause, Resume };
struct AudioControlRequest {
  std::uint64_t operation_id{};
  std::uint64_t play_generation{};
  AudioControlKind kind{AudioControlKind::Reset};
};
enum class AudioControlOutcome : std::uint8_t { Success, Failed };
struct AudioControlCompletion {
  AudioControlRequest request{};
  AudioControlOutcome outcome{AudioControlOutcome::Success};
  std::uint64_t media_frame{};
};
struct AudioObservation {
  bool fault{};
  std::uint64_t play_generation{};
  std::uint64_t media_frame{};
  bool ended{};
  std::optional<AudioControlCompletion> completion;
};
class AudioTransportPort {
public:
  virtual ~AudioTransportPort() = default;
  [[nodiscard]] virtual bool supports_pause() const noexcept = 0;
  [[nodiscard]] virtual bool begin(const AudioControlRequest& request) = 0;
  [[nodiscard]] virtual AudioObservation observe() = 0;
  virtual void emergency_silence() = 0;
};

enum class TransportFailure : std::uint8_t {
  None,
  Library,
  Preparation,
  PreparationTimeout,
  AudioControl,
  DeviceFault,
  AudioTimeout,
  ResetFailed,
  Protocol,
  Clock,
  InvalidConfiguration,
  ResourceExhausted
};
struct PendingPreparation {
  PreparationRequest request{};
  std::uint64_t started_at_us{};
  bool cancel_requested{};
};
struct PendingAudioControl {
  AudioControlRequest request{};
  std::uint64_t started_at_us{};
};
struct TransportSnapshot {
  contracts::CatalogStatus catalog{};
  bool pause_supported{};
  contracts::TransportState transport{contracts::TransportState::Empty};
  contracts::TransportState projected{contracts::TransportState::Empty};
  std::optional<PlaybackSelection> selection;
  std::uint64_t play_generation{};
  std::uint64_t media_frame{};
  bool prepared{};
  bool silence_confirmed{};
  bool terminal{};
  TransportFailure failure{TransportFailure::None};
  std::optional<TransportIntent> pending_intent;
  std::optional<PendingPreparation> preparation;
  std::optional<PendingAudioControl> audio_control;
};
struct TransportTiming {
  std::uint64_t prepare_timeout_us{};
  std::uint64_t control_timeout_us{};
};
struct TransportCounters {
  std::uint64_t last_play_generation{};
  std::uint64_t last_operation_id{};
};

struct TransportProjection {
  contracts::TransportState state{contracts::TransportState::Empty};
  std::optional<PlaybackSelection> selection;
  bool prepared_for_start{};
  bool failure{};
  bool recovery_busy{};
  bool terminal{};
};
enum class TransportAction : std::uint8_t {
  None,
  SelectPlay,
  SelectLoad,
  StartPrepared,
  Pause,
  Resume,
  Stop
};
struct TransportTransition {
  TransportRejection rejection{TransportRejection::None};
  TransportAction action{TransportAction::None};
  PlaybackSelection selection{};
};
[[nodiscard]] TransportTransition project_transport(const CatalogSession& catalog,
                                                    bool pause_supported,
                                                    const TransportIntent& intent,
                                                    TransportProjection& state);

struct TransportAdmissionContext {
  contracts::CatalogStatus catalog{};
  std::uint64_t play_generation{};
  TransportFailure failure{TransportFailure::None};
  bool terminal{};
  bool pause_supported{};
};

class PlayerSession;

// Core-only state machine. The public command ingress will translate into these
// intents and copy this state into versioned UI snapshots.
class TransportController {
public:
  TransportController(const CatalogSession& catalog, PreparationPort& preparation,
                      AudioTransportPort& audio, TransportTiming timing,
                      TransportCounters counters = {}) noexcept;
  TransportController(const TransportController&) = delete;
  TransportController& operator=(const TransportController&) = delete;
  [[nodiscard]] TransportStepResult
  step(std::uint64_t now_us, const TransportBatch& batch = {},
       const std::optional<TransportAdmissionContext>& admitted = std::nullopt);
  [[nodiscard]] TransportSnapshot snapshot() const noexcept;
  [[nodiscard]] TransportProjection projection() const noexcept;
  [[nodiscard]] TransportAdmissionContext admission_context() const noexcept;

private:
  friend class PlayerSession;
  [[nodiscard]] bool advance_generation();
  [[nodiscard]] std::optional<std::uint64_t> next_operation();
  void fail(TransportFailure failure, bool terminal);
  void invalidate();
  void sync_catalog(bool fault_this_step);
  [[nodiscard]] TransportRejection apply(const TransportIntent& intent);
  void select(const TransportIntent& intent, bool play);
  void consume_audio(const AudioObservation& observation);
  void consume_preparation();
  void check_timeouts(std::uint64_t now_us);
  void consume_position(const AudioObservation& observation);
  void pump(std::uint64_t now_us);
  void start_audio(AudioControlKind kind, std::uint64_t now_us);
  void settle();
  const CatalogSession& catalog_;
  PreparationPort& preparation_port_;
  AudioTransportPort& audio_port_;
  TransportTiming timing_{};
  TransportCounters counters_{};
  contracts::CatalogStatus catalog_status_{};
  TransportSnapshot state_{};
  contracts::TransportState target_{contracts::TransportState::Empty};
  std::uint64_t last_now_us_{};
  std::uint64_t prepared_generation_{};
  std::uint64_t started_generation_{};
  std::optional<std::uint64_t> held_end_frame_;
  bool clock_started_{};
  bool catalog_seen_{};
  bool reset_required_{true};
  bool need_prepare_{};
  bool fault_observed_{};
};

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_PLAYBACK_TRANSPORT_HPP
