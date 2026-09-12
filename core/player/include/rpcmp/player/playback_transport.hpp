#ifndef RPCMP_PLAYER_PLAYBACK_TRANSPORT_HPP
#define RPCMP_PLAYER_PLAYBACK_TRANSPORT_HPP

#include "rpcmp/contracts/playback_policy_v2.hpp"
#include "rpcmp/contracts/player_state_v2.hpp"
#include "rpcmp/player/catalog_session.hpp"
#include "rpcmp/player/media_loop_envelope.hpp"
#include "rpcmp/player/playback_navigation.hpp"

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
  Stop,
  SetPolicy,
  NextTrack,
  PreviousTrack
};
struct TransportIntent {
  std::uint64_t command_id{};
  TransportIntentKind kind{TransportIntentKind::Stop};
  PlaybackSelection selection{};
  std::optional<contracts::v2::PlaybackPolicy> policy{std::nullopt};
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
  BatchTooLarge,
  UnsupportedPolicy,
  NoNextTrack,
  NoPreviousTrack
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

enum class AudioControlKind : std::uint8_t { Reset, Start, Pause, Resume, SetPolicy };
struct AudioControlRequest {
  std::uint64_t operation_id{};
  std::uint64_t play_generation{};
  AudioControlKind kind{AudioControlKind::Reset};
  std::optional<contracts::v2::RepeatApplication> repeat{std::nullopt};
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
  std::optional<MediaEnvelopeSnapshot> media{std::nullopt};
};
class AudioTransportPort {
public:
  virtual ~AudioTransportPort() = default;
  [[nodiscard]] virtual bool supports_pause() const noexcept = 0;
  [[nodiscard]] virtual bool supports_policy() const noexcept { return false; }
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
  bool policy_supported{};
  contracts::v2::PlaybackPolicyObservation policy{};
  std::optional<contracts::v2::PlaybackNavigationObservation> navigation{std::nullopt};
};
struct TransportTiming {
  std::uint64_t prepare_timeout_us{};
  std::uint64_t control_timeout_us{};
};
struct TransportCounters {
  std::uint64_t last_play_generation{};
  std::uint64_t last_operation_id{};
  std::uint64_t initial_policy_revision{1};
  std::uint64_t last_shuffle_cycle_id{};
};

struct TransportProjection {
  contracts::TransportState state{contracts::TransportState::Empty};
  std::optional<PlaybackSelection> selection;
  bool prepared_for_start{};
  bool failure{};
  bool recovery_busy{};
  bool terminal{};
  bool policy_supported{};
  contracts::v2::PlaybackPolicy policy{};
  bool navigation_supported{};
  bool wants_playback{};
  bool cycle_pending{};
  ShuffleCycle cycle{};
};
enum class TransportAction : std::uint8_t {
  None,
  SelectPlay,
  SelectLoad,
  StartPrepared,
  Pause,
  Resume,
  Stop,
  SetPolicy,
  Navigate,
  ShuffleNext
};
struct TransportTransition {
  TransportRejection rejection{TransportRejection::None};
  TransportAction action{TransportAction::None};
  PlaybackSelection selection{};
  bool rebuild_cycle{};
};
[[nodiscard]] TransportTransition project_transport(const CatalogSession& catalog,
                                                    bool pause_supported,
                                                    const TransportIntent& intent,
                                                    TransportProjection& state,
                                                    const NavigationIndex* navigation = nullptr);

struct TransportAdmissionContext {
  contracts::CatalogStatus catalog{};
  std::uint64_t play_generation{};
  TransportFailure failure{TransportFailure::None};
  bool terminal{};
  bool pause_supported{};
  bool policy_supported{};
};

class PlayerSession;

// Core-only state machine. The public command ingress will translate into these
// intents and copy this state into versioned UI snapshots.
class TransportController {
public:
  TransportController(const CatalogSession& catalog, PreparationPort& preparation,
                      AudioTransportPort& audio, TransportTiming timing,
                      TransportCounters counters = {}, RandomSource* random = nullptr) noexcept;
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
  void select(const TransportIntent& intent, bool play, bool new_cycle = true);
  [[nodiscard]] bool begin_cycle(contracts::TrackId selected, bool already_started);
  void consume_audio(const AudioObservation& observation);
  void consume_preparation();
  void check_timeouts(std::uint64_t now_us);
  void consume_position(const AudioObservation& observation);
  [[nodiscard]] bool consume_media(const AudioObservation& observation);
  void pump(std::uint64_t now_us);
  void start_audio(AudioControlKind kind, std::uint64_t now_us);
  void settle();
  const CatalogSession& catalog_;
  PreparationPort& preparation_port_;
  AudioTransportPort& audio_port_;
  TransportTiming timing_{};
  TransportCounters counters_{};
  RandomSource* random_{};
  NavigationIndex navigation_index_{};
  ShuffleCycle shuffle_{};
  contracts::CatalogStatus catalog_status_{};
  TransportSnapshot state_{};
  contracts::TransportState target_{contracts::TransportState::Empty};
  std::uint64_t last_now_us_{};
  std::uint64_t prepared_generation_{};
  std::uint64_t started_generation_{};
  std::optional<std::uint64_t> held_end_frame_;
  contracts::v2::PlaybackEndReason held_end_reason_{contracts::v2::PlaybackEndReason::None};
  std::optional<contracts::v2::RepeatApplication> applied_repeat_;
  bool clock_started_{};
  bool catalog_seen_{};
  bool reset_required_{true};
  bool need_prepare_{};
  bool fault_observed_{};
};

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_PLAYBACK_TRANSPORT_HPP
