#include "rpcmp/platform/pocket/device_queue_mmio.hpp"
#include "rpcmp/spike/m5_library_loader.hpp"
#include "rpcmp/spike/m5_playback.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

extern "C" {
int rpcmp_pocket_slot_size(std::uint32_t slot_id, std::uint32_t* size);
int rpcmp_pocket_target_read_prepare(std::uint32_t length);
int rpcmp_pocket_target_read(std::uint32_t slot_id, std::uint32_t offset, void* destination,
                             std::uint32_t length, std::uint32_t* elapsed_us);
void rpcmp_pocket_terminal_init();
[[noreturn]] void rpcmp_pocket_hold_result();
}

namespace {

constexpr std::uint32_t kResetPollLimit = 1'000'000;
using rpcmp::platform::pocket::DeviceQueueMmioResult;
using rpcmp::platform::pocket::SoundResetMmioResult;

template <typename T, typename... Args> T& in_bss(Args&&... args) noexcept {
  static_assert(std::is_trivially_destructible_v<T>);
  alignas(T) static std::byte storage[sizeof(T)]{};
  return *::new (static_cast<void*>(storage)) T{std::forward<Args>(args)...};
}

class Slot final : public rpcmp::spike::IM5LibrarySlot {
public:
  bool size(std::uint32_t& bytes) noexcept override {
    return rpcmp_pocket_slot_size(4, &bytes) == 0;
  }
  bool read(const std::uint32_t offset, std::uint8_t* destination,
            const std::uint32_t length) noexcept override {
    if (!prepared_) {
      prepared_ = rpcmp_pocket_target_read_prepare(rpcmp::spike::kM5LibraryReadChunk) == 0;
      if (!prepared_)
        return false;
    }
    std::uint32_t elapsed{};
    return rpcmp_pocket_target_read(4, offset, destination, length, &elapsed) == 0;
  }

private:
  bool prepared_{};
};

class Device final : public rpcmp::spike::IM5PlaybackDevice {
public:
  Device(rpcmp::platform::pocket::DeviceQueueMmio& queue,
         rpcmp::platform::pocket::SoundResetMmio& sound)
      : queue_(queue), sound_(sound) {}
  bool now(std::uint64_t& tick) noexcept override {
    return queue_.read_media_time(tick) == DeviceQueueMmioResult::Accepted;
  }
  bool status(bool& empty) noexcept override {
    std::uint32_t queue_status{};
    std::uint32_t audio_status{};
    if (queue_.read_status(queue_status) != DeviceQueueMmioResult::Accepted ||
        sound_.read_audio_status(audio_status) != SoundResetMmioResult::Complete)
      return false;
    empty = (queue_status & 0x0000'080fU) == 0U;
    return true;
  }
  rpcmp::spike::M5Submit submit(const std::uint64_t tick,
                                const rpcmp::runtime::mdx::Ym2151Write& write) noexcept override {
    const auto result =
        queue_.enqueue_ym2151(tick, rpcmp::contracts::WriteRegister{write.address, write.value});
    if (result == DeviceQueueMmioResult::Accepted)
      return rpcmp::spike::M5Submit::Accepted;
    return result == DeviceQueueMmioResult::Backpressure ? rpcmp::spike::M5Submit::Backpressure
                                                         : rpcmp::spike::M5Submit::Fault;
  }
  bool reset() noexcept override {
    return sound_.reset(kResetPollLimit) == SoundResetMmioResult::Complete;
  }

private:
  rpcmp::platform::pocket::DeviceQueueMmio& queue_;
  rpcmp::platform::pocket::SoundResetMmio& sound_;
};

bool track_id(const char* text, rpcmp::contracts::TrackId& id) noexcept {
  if (text == nullptr || *text == '\0')
    return false;
  std::uint64_t value{};
  for (unsigned index = 0; text[index] != '\0'; ++index) {
    if (index >= 16)
      return false;
    const char digit = text[index];
    const unsigned nibble = digit >= '0' && digit <= '9'   ? static_cast<unsigned>(digit - '0')
                            : digit >= 'a' && digit <= 'f' ? static_cast<unsigned>(digit - 'a' + 10)
                            : digit >= 'A' && digit <= 'F' ? static_cast<unsigned>(digit - 'A' + 10)
                                                           : 16;
    if (nibble == 16 || value > (std::numeric_limits<std::uint64_t>::max() - nibble) / 16)
      return false;
    value = value * 16 + nibble;
  }
  id = rpcmp::contracts::TrackId{value};
  return value != 0;
}

[[noreturn]] void fail(const unsigned code, rpcmp::platform::pocket::SoundResetMmio& sound) {
  const bool reset =
      sound.initialized() && sound.reset(kResetPollLimit) == SoundResetMmioResult::Complete;
  std::printf("M5 FILE: FAIL %u\nRESET: %s\n", code, reset ? "OK" : "FAILED");
  rpcmp_pocket_hold_result();
}

} // namespace

int main(const int argc, char** argv) {
  rpcmp_pocket_terminal_init();
  std::printf("RPCMP M5 real file\n");
  rpcmp::platform::pocket::VolatileMmio32 mmio;
  rpcmp::platform::pocket::SoundResetMmio sound{mmio};
  if (sound.initialize() != SoundResetMmioResult::Complete ||
      sound.reset(kResetPollLimit) != SoundResetMmioResult::Complete)
    fail(1, sound);
  rpcmp::contracts::TrackId selected{};
  if (argc != 2 || !track_id(argv[1], selected))
    fail(2, sound);
  std::printf("READING LIBRARY...\n");
  static std::uint8_t bytes[rpcmp::spike::kM5LibraryMaximumBytes]{};
  Slot slot;
  const auto loaded = rpcmp::spike::load_m5_library(slot, bytes, sizeof(bytes));
  if (!loaded.ok()) {
    std::printf("LOAD=%u LIB=%u\n", static_cast<unsigned>(loaded.error),
                static_cast<unsigned>(loaded.library_error));
    fail(3, sound);
  }
  auto& session = in_bss<rpcmp::player::MdxLibrarySession>();
  auto& workspace = in_bss<rpcmp::player::MdxLibrarySessionWorkspace>();
  const auto prepared =
      rpcmp::player::prepare_mdx_library_session(loaded.library, selected, session, workspace);
  if (!prepared.ok()) {
    std::printf("SESSION=%u PARSE=%u MDX=%u\n", static_cast<unsigned>(prepared.error),
                static_cast<unsigned>(prepared.parse.error),
                static_cast<unsigned>(prepared.admission.error));
    fail(4, sound);
  }
  std::printf("LIBRARY: PASS\nMDX: PASS\nPLAYING (menu to exit)\n");
  rpcmp::platform::pocket::DeviceQueueMmio queue{mmio};
  if (queue.initialize() != DeviceQueueMmioResult::Accepted)
    fail(5, sound);
  Device device{queue, sound};
  auto& pump = in_bss<rpcmp::spike::M5PlaybackPump>(session, workspace, device);
  while (pump.service() == rpcmp::spike::M5PlaybackState::Running) {
  }
  std::printf("M5 FILE: %s\n",
              pump.state() == rpcmp::spike::M5PlaybackState::Complete ? "COMPLETE" : "FAIL");
  std::printf("STATE=%u ERROR=%u\n", static_cast<unsigned>(pump.state()),
              static_cast<unsigned>(pump.error()));
  std::printf("W=%llu D=%016llx\n", static_cast<unsigned long long>(pump.writes()),
              static_cast<unsigned long long>(pump.digest()));
  rpcmp_pocket_hold_result();
}
