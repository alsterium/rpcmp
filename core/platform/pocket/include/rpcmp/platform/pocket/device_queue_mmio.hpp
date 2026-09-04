#ifndef RPCMP_PLATFORM_POCKET_DEVICE_QUEUE_MMIO_HPP
#define RPCMP_PLATFORM_POCKET_DEVICE_QUEUE_MMIO_HPP

#include "rpcmp/contracts/device_op.hpp"

#include <cstdint>

namespace rpcmp::platform::pocket {

inline constexpr std::uintptr_t kDeviceQueueMmioBase = 0x4000'0200U;
inline constexpr std::uintptr_t kSoundResetMmioBase = 0x4000'0240U;
inline constexpr std::uint32_t kDeviceQueueId = 0x5251'4D31U;
inline constexpr std::uint32_t kSoundResetId = 0x5253'4331U;
inline constexpr std::uint32_t kDeviceQueueVersion = 1;
inline constexpr std::uint32_t kDeviceQueueDepth = 8;
inline constexpr std::uint32_t kM5MediaTickRate = 48'000;
inline constexpr std::uint32_t kM5CpuTickRate = 90'000'000;

class IMmio32 {
public:
  virtual ~IMmio32() = default;
  [[nodiscard]] virtual std::uint32_t read(std::uintptr_t address) noexcept = 0;
  virtual void write(std::uintptr_t address, std::uint32_t value) noexcept = 0;
};

enum class DeviceQueueMmioResult : std::uint8_t {
  Accepted,
  Backpressure,
  NotInitialized,
  IncompatibleHardware,
  InvalidOperation,
  TimeOverflow,
  DeviceFault,
};

enum class SoundResetMmioResult : std::uint8_t {
  Complete,
  IncompatibleHardware,
  DeviceFault,
  Timeout,
};

class SoundResetMmio final {
public:
  explicit SoundResetMmio(IMmio32& mmio, std::uintptr_t base = kSoundResetMmioBase) noexcept;

  [[nodiscard]] SoundResetMmioResult initialize() noexcept;
  [[nodiscard]] SoundResetMmioResult reset(std::uint32_t poll_limit) noexcept;
  [[nodiscard]] SoundResetMmioResult read_audio_status(std::uint32_t& status) noexcept;
  [[nodiscard]] bool initialized() const noexcept;

private:
  IMmio32& mmio_;
  std::uintptr_t base_{};
  bool initialized_{};
};

class DeviceQueueMmio final {
public:
  explicit DeviceQueueMmio(IMmio32& mmio, std::uintptr_t base = kDeviceQueueMmioBase) noexcept;

  [[nodiscard]] DeviceQueueMmioResult initialize() noexcept;
  [[nodiscard]] DeviceQueueMmioResult
  enqueue_ym2151(std::uint64_t media_tick, const contracts::DeviceOperation& operation) noexcept;
  [[nodiscard]] DeviceQueueMmioResult clear_fault_flags() noexcept;
  [[nodiscard]] DeviceQueueMmioResult read_status(std::uint32_t& status) noexcept;

  [[nodiscard]] bool initialized() const noexcept;
  [[nodiscard]] bool faulted() const noexcept;
  [[nodiscard]] std::uint64_t cpu_epoch() const noexcept;

private:
  [[nodiscard]] DeviceQueueMmioResult read_now(std::uint64_t& value) noexcept;
  [[nodiscard]] DeviceQueueMmioResult due_tick(std::uint64_t media_tick,
                                               std::uint64_t& value) const noexcept;

  IMmio32& mmio_;
  std::uintptr_t base_{};
  std::uint64_t cpu_epoch_{};
  bool initialized_{};
  bool faulted_{};
};

class VolatileMmio32 final : public IMmio32 {
public:
  [[nodiscard]] std::uint32_t read(std::uintptr_t address) noexcept override;
  void write(std::uintptr_t address, std::uint32_t value) noexcept override;
};

} // namespace rpcmp::platform::pocket

#endif // RPCMP_PLATFORM_POCKET_DEVICE_QUEUE_MMIO_HPP
