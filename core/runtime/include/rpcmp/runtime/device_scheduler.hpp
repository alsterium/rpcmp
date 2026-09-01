#ifndef RPCMP_RUNTIME_DEVICE_SCHEDULER_HPP
#define RPCMP_RUNTIME_DEVICE_SCHEDULER_HPP

#include "rpcmp/contracts/device_op.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpcmp::runtime {

inline constexpr std::size_t kMaxScheduledDeviceOps = 64;

enum class DevicePortResult : std::uint8_t { Accepted, Backpressure, DeviceFault };

class IDevicePort {
public:
  virtual ~IDevicePort() = default;
  virtual contracts::DeviceId device_id() const noexcept = 0;
  virtual contracts::DeviceType device_type() const noexcept = 0;
  virtual DevicePortResult accept(const contracts::DeviceOperation& operation) = 0;
  virtual DevicePortResult reset() = 0;
};

enum class SchedulerResult : std::uint8_t {
  Accepted,
  Backpressure,
  QueueFull,
  InvalidDevice,
  InvalidOperation,
  TimeReversed,
  TimeOverflow,
  DeviceFault,
};

class DeviceScheduler final {
public:
  DeviceScheduler(IDevicePort& port, std::uint32_t tick_rate, std::size_t capacity,
                  std::uint64_t max_media_tick) noexcept;

  SchedulerResult submit(const contracts::DeviceOpStream& stream);
  SchedulerResult advance_to(std::uint64_t media_tick);
  SchedulerResult advance_by(std::uint64_t delta_ticks);
  SchedulerResult reset();

  std::uint32_t tick_rate() const noexcept;
  std::uint64_t media_tick() const noexcept;
  std::size_t pending_count() const noexcept;
  std::size_t capacity() const noexcept;
  bool faulted() const noexcept;

private:
  SchedulerResult validate(const contracts::DeviceOpStream& stream) const noexcept;
  SchedulerResult drain();
  const contracts::DeviceOp& front() const noexcept;
  const contracts::DeviceOp& back() const noexcept;
  void pop_front() noexcept;
  void push_back(const contracts::DeviceOp& operation) noexcept;

  IDevicePort& port_;
  std::uint32_t tick_rate_{};
  std::size_t capacity_{};
  std::uint64_t max_media_tick_{};
  std::array<contracts::DeviceOp, kMaxScheduledDeviceOps> queue_{};
  std::size_t head_{};
  std::size_t size_{};
  std::uint64_t media_tick_{};
  bool configuration_valid_{};
  bool faulted_{};
};

} // namespace rpcmp::runtime

#endif // RPCMP_RUNTIME_DEVICE_SCHEDULER_HPP
