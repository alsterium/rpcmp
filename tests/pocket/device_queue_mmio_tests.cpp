#include "rpcmp/platform/pocket/device_queue_mmio.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using rpcmp::platform::pocket::DeviceQueueMmioResult;
constexpr std::uintptr_t kBase = rpcmp::platform::pocket::kDeviceQueueMmioBase;
constexpr std::uintptr_t kSoundBase = rpcmp::platform::pocket::kSoundResetMmioBase;

struct Write {
  std::uintptr_t address{};
  std::uint32_t value{};
};

class MockMmio final : public rpcmp::platform::pocket::IMmio32 {
public:
  std::uint32_t id{rpcmp::platform::pocket::kDeviceQueueId};
  std::uint32_t capability{(rpcmp::platform::pocket::kDeviceQueueVersion << 16U) |
                           rpcmp::platform::pocket::kDeviceQueueDepth};
  std::uint32_t status{};
  std::uint64_t now{0x0000'0001'2345'6789ULL};
  bool rollover_once{};
  std::array<Write, 16> writes{};
  std::size_t write_count{};
  std::uint32_t sound_status{};
  std::uint32_t sound_polls_before_complete{};
  std::uint32_t sound_audio_status{8U};

  std::uint32_t read(const std::uintptr_t address) noexcept override {
    if (address == kBase)
      return id;
    if (address == kBase + 4U)
      return capability;
    if (address == kBase + 8U)
      return status;
    if (address == kBase + 0x10U) {
      const auto low = static_cast<std::uint32_t>(now);
      if (rollover_once) {
        now = 0x0000'0002'0000'0000ULL;
        rollover_once = false;
      }
      return low;
    }
    if (address == kBase + 0x14U)
      return static_cast<std::uint32_t>(now >> 32U);
    if (address == kSoundBase)
      return rpcmp::platform::pocket::kSoundResetId;
    if (address == kSoundBase + 4U)
      return 0x0001'0100U;
    if (address == kSoundBase + 8U) {
      if ((sound_status & 1U) != 0U && sound_polls_before_complete != 0U) {
        --sound_polls_before_complete;
        if (sound_polls_before_complete == 0U)
          sound_status = (sound_status + 0x0001'0000U) & 0xFFFF'0000U;
      }
      return sound_status;
    }
    if (address == kSoundBase + 0x10U)
      return sound_audio_status;
    return 0;
  }

  void write(const std::uintptr_t address, const std::uint32_t value) noexcept override {
    if (write_count < writes.size())
      writes[write_count++] = {address, value};
    if (address == kBase + 0x0CU && (value & 3U) != 0U)
      status &= ~0x600U;
    if (address == kSoundBase + 0x0CU && (value & 1U) != 0U)
      sound_status |= 1U;
  }
};

} // namespace

int main() {
  rpcmp::test::Suite suite;

  MockMmio sound_mock;
  sound_mock.sound_status = 0x0007'0000U;
  sound_mock.sound_polls_before_complete = 3;
  rpcmp::platform::pocket::SoundResetMmio sound_reset(sound_mock);
  RPCMP_CHECK(suite,
              sound_reset.initialize() == rpcmp::platform::pocket::SoundResetMmioResult::Complete);
  RPCMP_CHECK(suite, sound_reset.initialized());
  RPCMP_CHECK(suite,
              sound_reset.reset(4) == rpcmp::platform::pocket::SoundResetMmioResult::Complete);
  RPCMP_CHECK(suite, sound_mock.sound_status == 0x0008'0000U);
  std::uint32_t observed_audio_status{};
  RPCMP_CHECK(suite, sound_reset.read_audio_status(observed_audio_status) ==
                         rpcmp::platform::pocket::SoundResetMmioResult::Complete);
  RPCMP_CHECK(suite, observed_audio_status == 8U);
  sound_mock.sound_audio_status = 1U;
  RPCMP_CHECK(suite, sound_reset.read_audio_status(observed_audio_status) ==
                         rpcmp::platform::pocket::SoundResetMmioResult::DeviceFault);

  MockMmio sound_timeout_mock;
  sound_timeout_mock.sound_polls_before_complete = 5;
  rpcmp::platform::pocket::SoundResetMmio sound_timeout(sound_timeout_mock);
  RPCMP_CHECK(suite, sound_timeout.initialize() ==
                         rpcmp::platform::pocket::SoundResetMmioResult::Complete);
  RPCMP_CHECK(suite,
              sound_timeout.reset(4) == rpcmp::platform::pocket::SoundResetMmioResult::Timeout);

  MockMmio sound_fault_mock;
  sound_fault_mock.sound_status = 2U;
  rpcmp::platform::pocket::SoundResetMmio sound_fault(sound_fault_mock);
  RPCMP_CHECK(suite, sound_fault.initialize() ==
                         rpcmp::platform::pocket::SoundResetMmioResult::DeviceFault);

  MockMmio mock;
  rpcmp::platform::pocket::DeviceQueueMmio queue(mock);
  RPCMP_CHECK(suite, queue.initialize() == DeviceQueueMmioResult::Accepted);
  RPCMP_CHECK(suite, queue.initialized());
  RPCMP_CHECK(suite, queue.cpu_epoch() == mock.now);
  std::uint32_t observed_queue_status{};
  RPCMP_CHECK(suite, queue.read_status(observed_queue_status) == DeviceQueueMmioResult::Accepted);
  RPCMP_CHECK(suite, observed_queue_status == 0U);

  RPCMP_CHECK(suite, queue.enqueue_ym2151(2, rpcmp::contracts::WriteRegister{0x28, 0x7F}) ==
                         DeviceQueueMmioResult::Accepted);
  RPCMP_CHECK(suite, mock.write_count == 3);
  const auto due = mock.now + 3'750U;
  RPCMP_CHECK(suite, mock.writes[0].address == kBase + 0x18U);
  RPCMP_CHECK(suite, mock.writes[0].value == static_cast<std::uint32_t>(due));
  RPCMP_CHECK(suite, mock.writes[1].address == kBase + 0x1CU);
  RPCMP_CHECK(suite, mock.writes[1].value == static_cast<std::uint32_t>(due >> 32U));
  RPCMP_CHECK(suite, mock.writes[2].address == kBase + 0x20U);
  RPCMP_CHECK(suite, mock.writes[2].value == 0x8001'287FU);

  mock.status = 0x100U;
  const auto before_full = mock.write_count;
  RPCMP_CHECK(suite, queue.enqueue_ym2151(3, rpcmp::contracts::ResetDevice{}) ==
                         DeviceQueueMmioResult::Backpressure);
  RPCMP_CHECK(suite, mock.write_count == before_full);

  mock.status = 0;
  RPCMP_CHECK(suite, queue.enqueue_ym2151(3, rpcmp::contracts::ResetDevice{}) ==
                         DeviceQueueMmioResult::Accepted);
  RPCMP_CHECK(suite, mock.writes[mock.write_count - 1].value == 0x8000'0000U);

  MockMmio rollover;
  rollover.now = 0x0000'0001'FFFF'FFFFULL;
  rollover.rollover_once = true;
  rpcmp::platform::pocket::DeviceQueueMmio rollover_queue(rollover);
  RPCMP_CHECK(suite, rollover_queue.initialize() == DeviceQueueMmioResult::Accepted);
  RPCMP_CHECK(suite, rollover_queue.cpu_epoch() == 0x0000'0002'0000'0000ULL);

  MockMmio incompatible;
  incompatible.capability = (2U << 16U) | 8U;
  rpcmp::platform::pocket::DeviceQueueMmio incompatible_queue(incompatible);
  RPCMP_CHECK(suite,
              incompatible_queue.initialize() == DeviceQueueMmioResult::IncompatibleHardware);

  MockMmio fault;
  fault.status = 0x200U;
  rpcmp::platform::pocket::DeviceQueueMmio fault_queue(fault);
  RPCMP_CHECK(suite, fault_queue.initialize() == DeviceQueueMmioResult::DeviceFault);
  RPCMP_CHECK(suite, fault_queue.faulted());
  RPCMP_CHECK(suite, fault_queue.clear_fault_flags() == DeviceQueueMmioResult::Accepted);
  RPCMP_CHECK(suite, !fault_queue.faulted());

  MockMmio overflow;
  overflow.now = std::numeric_limits<std::uint64_t>::max() - 1U;
  rpcmp::platform::pocket::DeviceQueueMmio overflow_queue(overflow);
  RPCMP_CHECK(suite, overflow_queue.initialize() == DeviceQueueMmioResult::Accepted);
  RPCMP_CHECK(suite, overflow_queue.enqueue_ym2151(1, rpcmp::contracts::ResetDevice{}) ==
                         DeviceQueueMmioResult::TimeOverflow);

  return suite.finish("device_queue_mmio_tests");
}
