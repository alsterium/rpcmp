#include "rpcmp/platform/pocket/device_queue_mmio.hpp"
#include "rpcmp/player/mdx_library_session.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <new>
#include <type_traits>

extern "C" {
extern const std::uint8_t _binary_m5_library_rpcmlib_start[];
void rpcmp_pocket_terminal_init();
std::uint32_t rpcmp_pocket_time_us();
[[noreturn]] void rpcmp_pocket_hold_result();
}

namespace {

constexpr rpcmp::contracts::TrackId kFixtureTrack{0x050E01724BD96C75ULL};
constexpr std::size_t kFixtureLibraryBytes = 752;
constexpr std::uint64_t kExpectedWrites = 33;
constexpr std::uint64_t kExpectedDigest = 0xf1f04f5a8695a112ULL;
constexpr std::uint32_t kResetPollLimit = 1'000'000;
constexpr std::uint32_t kQueueTimeoutUs = 2'000'000;
constexpr std::uint32_t kQueueActiveMask = 0x0000'080fU;
constexpr std::uint32_t kAudioIdle = 1U << 3U;

volatile std::uint64_t result_digest{};

template <typename T> T& construct_in_zero_backed_storage() noexcept {
  static_assert(std::is_trivially_destructible_v<T>);
  alignas(T) static std::byte storage[sizeof(T)]{};
  return *::new (static_cast<void*>(storage)) T{};
}

[[noreturn]] void fail(const unsigned code, rpcmp::platform::pocket::SoundResetMmio* sound) {
  if (sound != nullptr && sound->initialized()) {
    static_cast<void>(sound->reset(kResetPollLimit));
  }
  std::printf("M5 AUDIO: FAIL %u\n", code);
  std::printf("Reset requested when available.\n");
  rpcmp_pocket_hold_result();
}

bool timed_out(const std::uint32_t started) noexcept {
  return rpcmp_pocket_time_us() - started >= kQueueTimeoutUs;
}

} // namespace

int main() {
  rpcmp_pocket_terminal_init();
  std::printf("RPCMP M5 audio probe\n\n");

  rpcmp::platform::pocket::VolatileMmio32 mmio;
  rpcmp::platform::pocket::SoundResetMmio sound{mmio};
  if (sound.initialize() != rpcmp::platform::pocket::SoundResetMmioResult::Complete) {
    fail(1, nullptr);
  }
  if (sound.reset(kResetPollLimit) != rpcmp::platform::pocket::SoundResetMmioResult::Complete) {
    fail(2, &sound);
  }

  rpcmp::library::LogicalLibrary library;
  if (rpcmp::library::LogicalLibrary::open({_binary_m5_library_rpcmlib_start, kFixtureLibraryBytes},
                                           library) != rpcmp::library::LibraryError::None) {
    fail(3, &sound);
  }

  auto& session = construct_in_zero_backed_storage<rpcmp::player::MdxLibrarySession>();
  auto& workspace = construct_in_zero_backed_storage<rpcmp::player::MdxLibrarySessionWorkspace>();
  if (!rpcmp::player::prepare_mdx_library_session(library, kFixtureTrack, session, workspace)
           .ok()) {
    fail(4, &sound);
  }

  rpcmp::platform::pocket::DeviceQueueMmio queue{mmio};
  if (queue.initialize() != rpcmp::platform::pocket::DeviceQueueMmioResult::Accepted) {
    fail(5, &sound);
  }

  std::uint64_t writes{};
  std::uint64_t digest{};
  static rpcmp::runtime::mdx::TimedYm2151Batch batch;
  for (std::size_t tick = 0; tick < 32; ++tick) {
    if (!rpcmp::runtime::mdx::advance_mdx_tick(session.document, 48'000, session.state, batch,
                                               workspace.engine)
             .ok()) {
      fail(6, &sound);
    }
    writes += batch.count;
    for (std::size_t index = 0; index < batch.count; ++index) {
      const auto& write = batch.writes[index];
      digest = (digest * 1099511628211ULL) ^ write.at_tick;
      digest = (digest * 1099511628211ULL) ^ write.write.address;
      digest = (digest * 1099511628211ULL) ^ write.write.value;

      const rpcmp::contracts::DeviceOperation operation{
          rpcmp::contracts::WriteRegister{write.write.address, write.write.value}};
      const auto started = rpcmp_pocket_time_us();
      for (;;) {
        const auto result = queue.enqueue_ym2151(write.at_tick, operation);
        if (result == rpcmp::platform::pocket::DeviceQueueMmioResult::Accepted) {
          break;
        }
        if (result != rpcmp::platform::pocket::DeviceQueueMmioResult::Backpressure ||
            timed_out(started)) {
          fail(7, &sound);
        }
      }
    }
  }

  result_digest = digest;
  if (writes != kExpectedWrites || digest != kExpectedDigest) {
    fail(8, &sound);
  }

  const auto drain_started = rpcmp_pocket_time_us();
  std::uint32_t queue_status{};
  do {
    if (queue.read_status(queue_status) !=
            rpcmp::platform::pocket::DeviceQueueMmioResult::Accepted ||
        timed_out(drain_started)) {
      fail(9, &sound);
    }
  } while ((queue_status & kQueueActiveMask) != 0U);

  std::uint32_t audio_status{};
  if (sound.read_audio_status(audio_status) !=
          rpcmp::platform::pocket::SoundResetMmioResult::Complete ||
      (audio_status & kAudioIdle) == 0U) {
    fail(10, &sound);
  }

  std::printf("M5 AUDIO: PASS\n");
  std::printf("W=%llu D=%016llx\n", static_cast<unsigned long long>(writes),
              static_cast<unsigned long long>(digest));
  std::printf("Q=%08lx A=%08lx\n", static_cast<unsigned long>(queue_status),
              static_cast<unsigned long>(audio_status));
  std::printf("Open Pocket menu to exit.\n");
  rpcmp_pocket_hold_result();
}
