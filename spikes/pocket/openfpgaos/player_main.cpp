#include "player_sdk_adapter.h"
#include "rpcmp/platform/pocket/player_application.hpp"

#include <cstdio>
#include <new>
#include <utility>

namespace {
namespace pocket = rpcmp::platform::pocket;

// Construct once; these owners live until the core is unloaded. Raw zeroed BSS
// prevents multi-megabyte constant-initialized objects in the ELF .data section.
template <typename T, typename... Args> T& in_bss(Args&&... args) noexcept {
  alignas(T) static std::byte storage[sizeof(T)]{};
  return *::new (static_cast<void*>(storage)) T{std::forward<Args>(args)...};
}
class Slot final : public pocket::LibrarySlot {
public:
  bool size(std::uint32_t& bytes) noexcept override {
    return rpcmp_pocket_slot_size(4, &bytes) == 0;
  }
  bool read(std::uint32_t offset, std::uint8_t* output, std::uint32_t bytes) noexcept override {
    std::uint32_t elapsed{};
    return rpcmp_pocket_target_read(4, offset, output, bytes, &elapsed) == 0;
  }
};
class Display final : public pocket::PlayerDisplay {
public:
  pocket::DrawSurface acquire() noexcept override {
    return {rpcmp_player_video_surface(), std::size_t{640} * 480, 640};
  }
  pocket::PresentResult present() noexcept override {
    return rpcmp_player_video_present() == 0 ? pocket::PresentResult::Presented
                                             : pocket::PresentResult::Busy;
  }
};
class Random final : public rpcmp::player::RandomSource {
public:
  explicit Random(std::uint32_t seed) : state_(seed) {}
  bool next(std::uint32_t& value) override {
    state_ = state_ * 1'664'525U + 1'013'904'223U;
    value = state_;
    return true;
  }

private:
  std::uint32_t state_;
};
[[noreturn]] void fail(unsigned code, pocket::IMmio32& mmio,
                       const pocket::ApplicationMetrics* metrics = nullptr) {
  mmio.write(pocket::kSoundMmioBase + 0x10, 1);
  rpcmp_pocket_terminal_init();
  std::printf("RPCMP M6: FAIL %u\nSOUND INHIBIT requested\n", code);
  if (metrics) {
    std::printf("max us service=%llu record=%llu\npump=%llu present=%llu\n",
                static_cast<unsigned long long>(metrics->max_service_gap_us),
                static_cast<unsigned long long>(metrics->max_record_us),
                static_cast<unsigned long long>(metrics->max_pump_us),
                static_cast<unsigned long long>(metrics->max_present_us));
  }
  rpcmp_pocket_hold_result();
  for (;;) {
  }
}
} // namespace

int main() {
  pocket::VolatileMmio32 mmio;
  // Silence before filesystem/video work. The application later performs the
  // acknowledged Reset/Start sequence; INHIBIT here is not a quiescence claim.
  mmio.write(pocket::kSoundMmioBase + 0x10, 1);
  rpcmp_pocket_terminal_init();
  std::printf("RPCMP M6 Album Player\nREADING LIBRARY...\n");
  pocket::PocketClock clock{mmio, rpcmp_player_cpu_hz()};
  if (!clock.valid())
    fail(1, mmio);
  auto& catalog = in_bss<rpcmp::player::CatalogSession>();
  static std::uint8_t library[rpcmp::library::kAlbumMaxFileBytes]{};
  Slot slot;
  if (rpcmp_pocket_target_read_prepare(pocket::kLibraryReadChunk) != 0 ||
      !pocket::load_player_library(slot, library, sizeof(library), catalog).ok())
    fail(2, mmio);
  if (rpcmp_player_video_init(pocket::kBitmapPalette.data(),
                              static_cast<std::uint32_t>(pocket::kBitmapPalette.size())) != 0)
    fail(3, mmio);
  Display display;
  Random random{static_cast<std::uint32_t>(clock.now_us())};
  auto& application = in_bss<pocket::PlayerApplication>(catalog, mmio, clock, display, random);
  if (!application.initialize())
    fail(4, mmio);
  for (;;) {
    application.step(pocket::pocket_input_sample(mmio.read(0x4000'0050)));
    if (application.failure() != pocket::ApplicationFailure::None)
      fail(10 + static_cast<unsigned>(application.failure()), mmio, &application.metrics());
  }
}
