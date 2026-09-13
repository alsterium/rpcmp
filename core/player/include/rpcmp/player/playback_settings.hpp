#ifndef RPCMP_PLAYER_PLAYBACK_SETTINGS_HPP
#define RPCMP_PLAYER_PLAYBACK_SETTINGS_HPP

#include "rpcmp/contracts/playback_policy_v2.hpp"
#include "rpcmp/contracts/playback_settings_v2.hpp"

#include <array>

namespace rpcmp::player {
using SettingsBytes = std::array<std::uint8_t, 64>;
enum class SettingsSlotState : std::uint8_t { Missing, Bytes, IoError, Unsupported };
struct SettingsSlot {
  SettingsSlotState state{SettingsSlotState::Missing};
  SettingsBytes bytes{};
  std::uint16_t length{};
};
struct SettingsRecord {
  std::uint64_t sequence{};
  contracts::v2::PlaybackPolicy policy{};
};
enum class SettingsDecode : std::uint8_t { Valid, Invalid, Unsupported };
[[nodiscard]] bool encode_settings(const SettingsRecord& record, SettingsBytes& bytes) noexcept;
[[nodiscard]] SettingsDecode decode_settings(const SettingsSlot& slot,
                                             SettingsRecord& record) noexcept;

enum class SettingsOperation : std::uint8_t { Read, Commit };
enum class SettingsIoResult : std::uint8_t { Success, IoError, Unsupported };
struct SettingsRequest {
  std::uint64_t id{};
  SettingsOperation operation{SettingsOperation::Read};
  std::uint64_t revision{};
  std::uint8_t slot{};
  SettingsBytes bytes{};
};
struct SettingsCompletion {
  std::uint64_t id{};
  SettingsOperation operation{SettingsOperation::Read};
  std::uint64_t revision{};
  std::uint8_t slot{};
  SettingsIoResult result{SettingsIoResult::Success};
  std::array<SettingsSlot, 2> slots{};
  bool durable{};
  SettingsSlot readback{};
};
class PlaybackSettingsPort {
public:
  virtual ~PlaybackSettingsPort() = default;
  [[nodiscard]] virtual bool begin(const SettingsRequest& request) = 0;
  [[nodiscard]] virtual std::optional<SettingsCompletion> poll() = 0;
  virtual void cancel(std::uint64_t request_id) = 0;
  [[nodiscard]] virtual bool quiescent(std::uint64_t request_id) = 0;
};
struct SettingsTiming {
  std::uint64_t read_timeout_us{};
  std::uint64_t commit_timeout_us{};
  std::uint64_t quiesce_timeout_us{};
};
struct SettingsConfiguration {
  PlaybackSettingsPort* port{};
  SettingsTiming timing{};
  std::uint64_t last_request_id{};
};

// Single Core owner. All I/O occurs in step(), never in observe() or snapshot().
class PlaybackSettings {
public:
  explicit PlaybackSettings(SettingsConfiguration configuration) noexcept;
  PlaybackSettings(const PlaybackSettings&) = delete;
  PlaybackSettings& operator=(const PlaybackSettings&) = delete;
  void step(std::uint64_t now_us);
  void observe(const contracts::v2::PlaybackPolicy& policy, std::uint64_t revision,
               std::uint64_t now_us) noexcept;
  void unsupported_backend() noexcept;
  [[nodiscard]] bool enabled() const noexcept { return config_.port != nullptr; }
  [[nodiscard]] bool ready() const noexcept { return ready_; }
  [[nodiscard]] contracts::v2::PlaybackPolicy policy() const noexcept { return policy_; }
  [[nodiscard]] contracts::v2::PlaybackSettingsObservation snapshot() const noexcept {
    return state_;
  }

private:
  enum class Phase : std::uint8_t { Initial, Idle, Active, Cancelling, Disabled };
  void fail(contracts::v2::SettingsError error, std::uint64_t now_us, bool permanent = false);
  void read_slots(const std::array<SettingsSlot, 2>& slots, std::uint64_t now_us);
  void complete(const SettingsCompletion& completion, std::uint64_t now_us);
  void begin(bool read, std::uint64_t now_us);
  void set_error(contracts::v2::SettingsError error, bool permanent) noexcept;
  SettingsConfiguration config_{};
  contracts::v2::PlaybackSettingsObservation state_{};
  contracts::v2::PlaybackPolicy policy_{};
  Phase phase_{Phase::Initial};
  std::optional<SettingsRequest> active_;
  std::uint64_t active_since_{};
  std::uint64_t cancel_since_{};
  std::uint64_t last_now_{};
  std::uint64_t changed_at_{};
  std::uint64_t max_sequence_{};
  std::uint8_t next_slot_{};
  bool ready_{};
  bool clock_started_{};
  bool dirty_{};
  bool needs_read_{};
  bool permanent_{};
};
} // namespace rpcmp::player

#endif // RPCMP_PLAYER_PLAYBACK_SETTINGS_HPP
