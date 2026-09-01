#include "rpcmp/runtime/device_scheduler.hpp"

#include <limits>

namespace rpcmp::runtime {

DeviceScheduler::DeviceScheduler(IDevicePort& port, const std::uint32_t tick_rate,
                                 const std::size_t capacity,
                                 const std::uint64_t max_media_tick) noexcept
    : port_(port), tick_rate_(tick_rate),
      capacity_(capacity <= kMaxScheduledDeviceOps ? capacity : kMaxScheduledDeviceOps),
      max_media_tick_(max_media_tick),
      configuration_valid_(tick_rate != 0 && capacity != 0 && capacity <= kMaxScheduledDeviceOps) {}

SchedulerResult DeviceScheduler::submit(const contracts::DeviceOpStream& stream) {
  const auto result = validate(stream);
  if (result != SchedulerResult::Accepted) {
    return result;
  }
  if (stream.operations.size() > capacity_ - size_) {
    return SchedulerResult::QueueFull;
  }
  for (const auto& operation : stream.operations) {
    push_back(operation);
  }
  return SchedulerResult::Accepted;
}

SchedulerResult DeviceScheduler::advance_to(const std::uint64_t media_tick) {
  if (faulted_) {
    return SchedulerResult::DeviceFault;
  }
  if (media_tick < media_tick_) {
    return SchedulerResult::TimeReversed;
  }
  if (media_tick > max_media_tick_) {
    return SchedulerResult::TimeOverflow;
  }
  media_tick_ = media_tick;
  return drain();
}

SchedulerResult DeviceScheduler::advance_by(const std::uint64_t delta_ticks) {
  if (faulted_) {
    return SchedulerResult::DeviceFault;
  }
  if (delta_ticks > std::numeric_limits<std::uint64_t>::max() - media_tick_) {
    return SchedulerResult::TimeOverflow;
  }
  return advance_to(media_tick_ + delta_ticks);
}

SchedulerResult DeviceScheduler::reset() {
  head_ = 0;
  size_ = 0;
  media_tick_ = 0;
  faulted_ = false;
  const auto result = port_.reset();
  if (result == DevicePortResult::DeviceFault) {
    faulted_ = true;
    return SchedulerResult::DeviceFault;
  }
  if (result == DevicePortResult::Backpressure) {
    return SchedulerResult::Backpressure;
  }
  return SchedulerResult::Accepted;
}

std::uint32_t DeviceScheduler::tick_rate() const noexcept { return tick_rate_; }

std::uint64_t DeviceScheduler::media_tick() const noexcept { return media_tick_; }

std::size_t DeviceScheduler::pending_count() const noexcept { return size_; }

std::size_t DeviceScheduler::capacity() const noexcept { return capacity_; }

bool DeviceScheduler::faulted() const noexcept { return faulted_; }

SchedulerResult DeviceScheduler::validate(const contracts::DeviceOpStream& stream) const noexcept {
  if (faulted_) {
    return SchedulerResult::DeviceFault;
  }
  if (!configuration_valid_ || stream.version != contracts::kDeviceOpStreamVersion ||
      stream.tick_rate != tick_rate_) {
    return SchedulerResult::InvalidOperation;
  }
  if (stream.operations.size() > capacity_) {
    return SchedulerResult::QueueFull;
  }

  std::uint64_t previous_tick{};
  bool first = true;
  for (const auto& operation : stream.operations) {
    if (operation.device_id != port_.device_id() ||
        port_.device_type() != contracts::DeviceType::Ym2151) {
      return SchedulerResult::InvalidDevice;
    }
    if (operation.operation.valueless_by_exception()) {
      return SchedulerResult::InvalidOperation;
    }
    if ((first && size_ != 0 && operation.at_tick < back().at_tick) ||
        (!first && operation.at_tick < previous_tick)) {
      return SchedulerResult::InvalidOperation;
    }
    if (operation.at_tick > max_media_tick_) {
      return SchedulerResult::TimeOverflow;
    }
    previous_tick = operation.at_tick;
    first = false;
  }
  return SchedulerResult::Accepted;
}

SchedulerResult DeviceScheduler::drain() {
  while (size_ != 0 && front().at_tick <= media_tick_) {
    const auto result = port_.accept(front().operation);
    if (result == DevicePortResult::Backpressure) {
      return SchedulerResult::Backpressure;
    }
    if (result == DevicePortResult::DeviceFault) {
      faulted_ = true;
      return SchedulerResult::DeviceFault;
    }
    pop_front();
  }
  return SchedulerResult::Accepted;
}

const contracts::DeviceOp& DeviceScheduler::front() const noexcept { return queue_[head_]; }

const contracts::DeviceOp& DeviceScheduler::back() const noexcept {
  return queue_[(head_ + size_ - 1) % kMaxScheduledDeviceOps];
}

void DeviceScheduler::pop_front() noexcept {
  head_ = (head_ + 1) % kMaxScheduledDeviceOps;
  --size_;
}

void DeviceScheduler::push_back(const contracts::DeviceOp& operation) noexcept {
  const auto tail = (head_ + size_) % kMaxScheduledDeviceOps;
  queue_[tail] = operation;
  ++size_;
}

} // namespace rpcmp::runtime
