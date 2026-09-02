#include "rpcmp/runtime/mdx_scheduler_bridge.hpp"

#include <algorithm>

namespace rpcmp::runtime::mdx {
namespace {

MdxSchedulerBridgeResult map_scheduler_result(const SchedulerResult result) noexcept {
  switch (result) {
  case SchedulerResult::Accepted:
    return MdxSchedulerBridgeResult::Complete;
  case SchedulerResult::Backpressure:
  case SchedulerResult::QueueFull:
    return MdxSchedulerBridgeResult::Pending;
  case SchedulerResult::InvalidDevice:
    return MdxSchedulerBridgeResult::InvalidDevice;
  case SchedulerResult::TimeOverflow:
  case SchedulerResult::TimeReversed:
    return MdxSchedulerBridgeResult::TimeOverflow;
  case SchedulerResult::InvalidOperation:
    return MdxSchedulerBridgeResult::InvalidBatch;
  case SchedulerResult::DeviceFault:
    return MdxSchedulerBridgeResult::DeviceFault;
  }
  return MdxSchedulerBridgeResult::InvalidBatch;
}

} // namespace

MdxSchedulerBridgeResult begin_scheduler_batch(const TimedYm2151Batch& batch,
                                               const contracts::DeviceId device_id,
                                               const DeviceScheduler& scheduler,
                                               MdxSchedulerBridgeState& state,
                                               MdxSchedulerBridgeScratch& scratch) noexcept {
  if (state.active || batch.count > batch.writes.size()) {
    return MdxSchedulerBridgeResult::InvalidBatch;
  }
  for (std::size_t index = 1; index < batch.count; ++index) {
    if (batch.writes[index].at_tick < batch.writes[index - 1].at_tick) {
      return MdxSchedulerBridgeResult::InvalidBatch;
    }
  }
  for (std::size_t offset = 0; offset < batch.count;) {
    const auto chunk_count = std::min(kMaxScheduledDeviceOps, batch.count - offset);
    for (std::size_t index = 0; index < chunk_count; ++index) {
      const auto& source = batch.writes[offset + index];
      scratch.operations[index] = {
          source.at_tick, device_id,
          contracts::WriteRegister{source.write.address, source.write.value}};
    }
    const auto validated =
        scheduler.validate({scratch.operations.data(), chunk_count,
                            contracts::kDeviceOpStreamVersion, scheduler.tick_rate()});
    if (validated != SchedulerResult::Accepted) {
      return map_scheduler_result(validated);
    }
    offset += chunk_count;
  }
  state = {batch.count, 0, device_id, batch.count != 0};
  return batch.count == 0 ? MdxSchedulerBridgeResult::Complete : MdxSchedulerBridgeResult::Pending;
}

MdxSchedulerBridgeResult pump_scheduler_batch(const TimedYm2151Batch& batch,
                                              const contracts::DeviceId device_id,
                                              DeviceScheduler& scheduler,
                                              MdxSchedulerBridgeState& state,
                                              MdxSchedulerBridgeScratch& scratch) noexcept {
  if (!state.active || state.count != batch.count || state.cursor > state.count ||
      state.device_id != device_id || batch.count > batch.writes.size()) {
    return MdxSchedulerBridgeResult::InvalidBatch;
  }

  const auto available = scheduler.capacity() - scheduler.pending_count();
  if (available == 0) {
    return MdxSchedulerBridgeResult::Pending;
  }
  const auto chunk_count = std::min(available, state.count - state.cursor);
  for (std::size_t index = 0; index < chunk_count; ++index) {
    const auto& source = batch.writes[state.cursor + index];
    scratch.operations[index] = {
        source.at_tick, device_id,
        contracts::WriteRegister{source.write.address, source.write.value}};
  }

  const auto submitted =
      scheduler.submit({scratch.operations.data(), chunk_count, contracts::kDeviceOpStreamVersion,
                        scheduler.tick_rate()});
  if (submitted != SchedulerResult::Accepted) {
    return map_scheduler_result(submitted);
  }
  state.cursor += chunk_count;
  if (state.cursor != state.count) {
    return MdxSchedulerBridgeResult::Pending;
  }
  state.active = false;
  return MdxSchedulerBridgeResult::Complete;
}

} // namespace rpcmp::runtime::mdx
