#ifndef RPCMP_RUNTIME_MDX_SCHEDULER_BRIDGE_HPP
#define RPCMP_RUNTIME_MDX_SCHEDULER_BRIDGE_HPP

#include "rpcmp/runtime/device_scheduler.hpp"
#include "rpcmp/runtime/mdx_timeline.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace rpcmp::runtime::mdx {

enum class MdxSchedulerBridgeResult : std::uint8_t {
  Complete,
  Pending,
  InvalidBatch,
  InvalidDevice,
  TimeOverflow,
  DeviceFault,
};

struct MdxSchedulerBridgeState {
  std::size_t count{};
  std::size_t cursor{};
  contracts::DeviceId device_id{};
  bool active{};
};

struct MdxSchedulerBridgeScratch {
  std::array<contracts::DeviceOp, kMaxScheduledDeviceOps> operations{};
};

// Validates the complete caller-owned batch before beginning incremental
// submission. The batch must remain immutable until pump reports Complete.
[[nodiscard]] MdxSchedulerBridgeResult
begin_scheduler_batch(const TimedYm2151Batch& batch, contracts::DeviceId device_id,
                      const DeviceScheduler& scheduler, MdxSchedulerBridgeState& state,
                      MdxSchedulerBridgeScratch& scratch) noexcept;

// Submits as many writes as currently fit without advancing media time.
// Queue backpressure is resumed by draining the scheduler and calling again.
[[nodiscard]] MdxSchedulerBridgeResult
pump_scheduler_batch(const TimedYm2151Batch& batch, contracts::DeviceId device_id,
                     DeviceScheduler& scheduler, MdxSchedulerBridgeState& state,
                     MdxSchedulerBridgeScratch& scratch) noexcept;

} // namespace rpcmp::runtime::mdx

#endif // RPCMP_RUNTIME_MDX_SCHEDULER_BRIDGE_HPP
