#include "rpcmp/platform/pocket/device_queue_mmio.hpp"

#include <limits>
#include <variant>

namespace rpcmp::platform::pocket {
namespace {

constexpr std::uintptr_t kIdOffset = 0x00;
constexpr std::uintptr_t kCapabilityOffset = 0x04;
constexpr std::uintptr_t kStatusOffset = 0x08;
constexpr std::uintptr_t kClearOffset = 0x0C;
constexpr std::uintptr_t kNowLowOffset = 0x10;
constexpr std::uintptr_t kNowHighOffset = 0x14;
constexpr std::uintptr_t kDueLowOffset = 0x18;
constexpr std::uintptr_t kDueHighOffset = 0x1C;
constexpr std::uintptr_t kPushOffset = 0x20;

constexpr std::uint32_t kStatusFull = 1U << 8U;
constexpr std::uint32_t kStatusFaults = (1U << 9U) | (1U << 10U);
constexpr std::uint32_t kClearFaults = 3U;
constexpr std::uint32_t kPushValid = 1U << 31U;
constexpr std::uint32_t kWriteKind = 1U << 16U;
constexpr unsigned kNowReadAttempts = 4;

} // namespace

DeviceQueueMmio::DeviceQueueMmio(IMmio32& mmio, const std::uintptr_t base) noexcept
    : mmio_(mmio), base_(base) {}

DeviceQueueMmioResult DeviceQueueMmio::initialize() noexcept {
  initialized_ = false;
  faulted_ = false;
  if (mmio_.read(base_ + kIdOffset) != kDeviceQueueId) {
    return DeviceQueueMmioResult::IncompatibleHardware;
  }
  const auto capability = mmio_.read(base_ + kCapabilityOffset);
  if (capability != ((kDeviceQueueVersion << 16U) | kDeviceQueueDepth)) {
    return DeviceQueueMmioResult::IncompatibleHardware;
  }
  const auto status = mmio_.read(base_ + kStatusOffset);
  if ((status & kStatusFaults) != 0U) {
    faulted_ = true;
    return DeviceQueueMmioResult::DeviceFault;
  }
  const auto now_result = read_now(cpu_epoch_);
  if (now_result != DeviceQueueMmioResult::Accepted) {
    return now_result;
  }
  initialized_ = true;
  return DeviceQueueMmioResult::Accepted;
}

DeviceQueueMmioResult
DeviceQueueMmio::enqueue_ym2151(const std::uint64_t media_tick,
                                const contracts::DeviceOperation& operation) noexcept {
  if (!initialized_) {
    return DeviceQueueMmioResult::NotInitialized;
  }
  if (faulted_ || operation.valueless_by_exception()) {
    return faulted_ ? DeviceQueueMmioResult::DeviceFault : DeviceQueueMmioResult::InvalidOperation;
  }
  const auto status = mmio_.read(base_ + kStatusOffset);
  if ((status & kStatusFaults) != 0U) {
    faulted_ = true;
    return DeviceQueueMmioResult::DeviceFault;
  }
  if ((status & kStatusFull) != 0U) {
    return DeviceQueueMmioResult::Backpressure;
  }

  std::uint64_t due{};
  const auto converted = due_tick(media_tick, due);
  if (converted != DeviceQueueMmioResult::Accepted) {
    return converted;
  }

  std::uint32_t push = kPushValid;
  if (const auto* const write = std::get_if<contracts::WriteRegister>(&operation)) {
    push |= kWriteKind | (static_cast<std::uint32_t>(write->address) << 8U) | write->value;
  } else if (std::get_if<contracts::ResetDevice>(&operation) == nullptr) {
    return DeviceQueueMmioResult::InvalidOperation;
  }
  mmio_.write(base_ + kDueLowOffset, static_cast<std::uint32_t>(due));
  mmio_.write(base_ + kDueHighOffset, static_cast<std::uint32_t>(due >> 32U));
  mmio_.write(base_ + kPushOffset, push);

  if ((mmio_.read(base_ + kStatusOffset) & kStatusFaults) != 0U) {
    faulted_ = true;
    return DeviceQueueMmioResult::DeviceFault;
  }
  return DeviceQueueMmioResult::Accepted;
}

DeviceQueueMmioResult DeviceQueueMmio::clear_fault_flags() noexcept {
  mmio_.write(base_ + kClearOffset, kClearFaults);
  faulted_ = (mmio_.read(base_ + kStatusOffset) & kStatusFaults) != 0U;
  return faulted_ ? DeviceQueueMmioResult::DeviceFault : DeviceQueueMmioResult::Accepted;
}

bool DeviceQueueMmio::initialized() const noexcept { return initialized_; }
bool DeviceQueueMmio::faulted() const noexcept { return faulted_; }
std::uint64_t DeviceQueueMmio::cpu_epoch() const noexcept { return cpu_epoch_; }

DeviceQueueMmioResult DeviceQueueMmio::read_now(std::uint64_t& value) noexcept {
  for (unsigned attempt = 0; attempt < kNowReadAttempts; ++attempt) {
    const auto high_before = mmio_.read(base_ + kNowHighOffset);
    const auto low = mmio_.read(base_ + kNowLowOffset);
    const auto high_after = mmio_.read(base_ + kNowHighOffset);
    if (high_before == high_after) {
      value = (static_cast<std::uint64_t>(high_after) << 32U) | low;
      return DeviceQueueMmioResult::Accepted;
    }
  }
  faulted_ = true;
  return DeviceQueueMmioResult::DeviceFault;
}

DeviceQueueMmioResult DeviceQueueMmio::due_tick(const std::uint64_t media_tick,
                                                std::uint64_t& value) const noexcept {
  static_assert(kM5CpuTickRate % kM5MediaTickRate == 0U);
  constexpr std::uint64_t scale = kM5CpuTickRate / kM5MediaTickRate;
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  if (media_tick > (maximum - cpu_epoch_) / scale) {
    return DeviceQueueMmioResult::TimeOverflow;
  }
  value = cpu_epoch_ + media_tick * scale;
  return DeviceQueueMmioResult::Accepted;
}

std::uint32_t VolatileMmio32::read(const std::uintptr_t address) noexcept {
  // Hardware register addresses are integers by definition at this platform boundary.
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  return *reinterpret_cast<volatile std::uint32_t*>(address);
}

void VolatileMmio32::write(const std::uintptr_t address, const std::uint32_t value) noexcept {
  // Hardware register addresses are integers by definition at this platform boundary.
  // NOLINTNEXTLINE(performance-no-int-to-ptr)
  *reinterpret_cast<volatile std::uint32_t*>(address) = value;
}

} // namespace rpcmp::platform::pocket
