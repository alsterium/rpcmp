#ifndef RPCMP_RUNTIME_MOCK_CORE_HPP
#define RPCMP_RUNTIME_MOCK_CORE_HPP

#include "rpcmp/contracts/player_command.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>

namespace rpcmp::runtime {

inline constexpr std::uint32_t kMockTickRate = 60'000;
inline constexpr std::uint64_t kSnapshotCadenceTicks = 1'000;
inline constexpr std::size_t kMockChannelCount = 8;
inline constexpr std::size_t kCommandQueueCapacity = 32;
inline constexpr std::size_t kCommandHistoryCapacity = 64;
inline constexpr std::uint64_t kMaxCatchUpSnapshots = 1'024;

struct FakeDeviceEvent {
  std::uint64_t at_media_tick{};
  contracts::ChannelId channel_id{};
  bool key_on{};
  std::optional<std::uint8_t> note;
};

bool operator==(const FakeDeviceEvent& left, const FakeDeviceEvent& right) noexcept;

class IFakeDeviceSink {
public:
  virtual ~IFakeDeviceSink() = default;
  virtual void write(const FakeDeviceEvent& event) = 0;
};

class ISnapshotObserver {
public:
  virtual ~ISnapshotObserver() = default;
  virtual void published(const contracts::PlayerSnapshot& snapshot) = 0;
};

enum class AdvanceResult : std::uint8_t {
  Ok,
  TimeReversed,
  CatchUpLimit,
  TickOverflow,
  ProjectionMismatch,
};

class MockCore final {
public:
  explicit MockCore(IFakeDeviceSink* device_sink = nullptr,
                    ISnapshotObserver* snapshot_observer = nullptr);

  contracts::CommandResult submit(const contracts::PlayerCommand& command);
  AdvanceResult advance_to(std::uint64_t clock_tick);
  contracts::PlayerSnapshot latest() const;
  std::size_t pending_command_count() const noexcept;

private:
  struct ControlState {
    bool library_open{};
    std::optional<std::size_t> track_index;
    contracts::TransportState transport{contracts::TransportState::Empty};
    std::array<bool, kMockChannelCount> muted{};
    std::array<bool, kMockChannelCount> solo{};
  };

  struct Observation {
    bool key_on{};
    std::optional<std::uint8_t> note;
    std::uint8_t activity{};
  };

  contracts::CommandReason transition(const contracts::PlayerCommand& command,
                                      ControlState& state) const;
  static bool control_equal(const ControlState& left, const ControlState& right) noexcept;
  static void clear_overrides(ControlState& state) noexcept;
  void apply_command_effects(const contracts::PlayerCommand& command, const ControlState& before);
  AdvanceResult drain_commands();
  AdvanceResult advance_media(std::uint64_t delta_ticks);
  void update_observations(std::uint64_t media_tick);
  void deactivate_observations(std::uint64_t media_tick);
  void publish(std::uint64_t clock_tick);
  contracts::PlayerSnapshot make_snapshot(std::uint64_t clock_tick, std::uint64_t sequence) const;
  contracts::CommandResult reject_and_remember(std::uint64_t command_id,
                                               contracts::CommandReason reason);
  void remember(const contracts::CommandResult& result);

  IFakeDeviceSink* device_sink_{};
  ISnapshotObserver* snapshot_observer_{};
  ControlState actual_{};
  ControlState projected_{};
  std::array<Observation, kMockChannelCount> observations_{};
  std::deque<contracts::PlayerCommand> queue_;
  std::deque<contracts::CommandResult> history_;
  std::uint64_t command_high_water_{};
  std::uint64_t clock_tick_{};
  std::uint64_t position_ticks_{};
  std::optional<contracts::PlayerError> runtime_error_;
  contracts::PlayerSnapshot latest_;
};

} // namespace rpcmp::runtime

#endif // RPCMP_RUNTIME_MOCK_CORE_HPP
