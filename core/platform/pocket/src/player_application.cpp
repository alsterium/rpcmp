#include "rpcmp/platform/pocket/player_application.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace rpcmp::platform::pocket {
ui::v2::InputBindings pocket_input_bindings() noexcept {
  return {{0x20, 0x100, 0x200, 0x10, 1, 2, 4, 8}, 350'000, 80'000};
}
ui::v2::InputSample pocket_input_sample(const std::uint32_t key) noexcept {
  const auto type = key >> 28U;
  return {static_cast<std::uint16_t>(key), type >= 1 && type <= 3};
}
PocketClock::PocketClock(IMmio32& mmio, const std::uint32_t cpu_hz) noexcept : mmio_(mmio) {
  if (cpu_hz >= 1'000'000 && cpu_hz % 1'000'000 == 0)
    divisor_ = cpu_hz / 1'000'000;
}
std::uint64_t PocketClock::now_us() noexcept {
  // Bounded hi/lo/hi retry handles the low-word wrap without user rdcycle.
  if (valid()) {
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
      const auto hi = mmio_.read(0x4000'0008);
      const auto lo = mmio_.read(0x4000'0004);
      if (hi != mmio_.read(0x4000'0008))
        continue;
      const auto now = ((static_cast<std::uint64_t>(hi) << 32U) | lo) / divisor_;
      if (now < last_)
        break;
      last_ = now;
      return now;
    }
  }
  failed_ = true;
  // A terminal sentinel makes the application stop before another Core step.
  return std::numeric_limits<std::uint64_t>::max();
}

PlayerApplication::PlayerApplication(const player::CatalogSession& catalog, IMmio32& mmio,
                                     SoundClock& clock, PlayerDisplay& display,
                                     player::RandomSource& random) noexcept
    : clock_(clock), display_(display), client_(mmio, clock), backend_(catalog, client_),
      player_(catalog, backend_, backend_, {5'000'000, 100'000}, &random, &backend_),
      ui_(player_, catalog, {pocket_input_bindings()}) {}

void PlayerApplication::fail(const ApplicationFailure reason) noexcept {
  if (failure_ == ApplicationFailure::None) {
    failure_ = reason;
    backend_.emergency_silence();
  }
}
std::uint64_t PlayerApplication::time() noexcept {
  const auto now = clock_.now_us();
  if (now < last_time_ || now == std::numeric_limits<std::uint64_t>::max()) {
    fail(ApplicationFailure::Clock);
    return last_time_;
  }
  last_time_ = now;
  return now;
}
bool PlayerApplication::initialize() noexcept {
  if (initialized_ || failure_ != ApplicationFailure::None)
    return false;
  static_cast<void>(time());
  if (failure_ != ApplicationFailure::None)
    return false;
  if (backend_.initialize() != SoundSubmit::Accepted) {
    fail(ApplicationFailure::SoundSetup);
    return false;
  }
  initialized_ = true;
  return true;
}
void PlayerApplication::service() {
  const auto now = time();
  if (failure_ != ApplicationFailure::None)
    return;
  if (metrics_.service_calls != 0)
    metrics_.max_service_gap_us = std::max(metrics_.max_service_gap_us, now - last_service_);
  last_service_ = now;
  ++metrics_.service_calls;
  backend_.service();
}
void PlayerApplication::step(const ui::v2::InputSample input) {
  if (!initialized_ || failure_ != ApplicationFailure::None)
    return;
  service();
  const auto now = time();
  if (failure_ != ApplicationFailure::None)
    return;
  const bool publish = !published_ || now - last_publication_ >= 5'000;
  // Audio supply is serviced every turn. Core command/projection work does
  // not need to be repeated for every small piece of a framebuffer.
  if (!stepped_ || now - last_step_ >= 1'000 || publish) {
    static_cast<void>(player_.step(now, publish));
    last_step_ = now;
    stepped_ = true;
    service();
  }
  if (failure_ != ApplicationFailure::None)
    return;
  if (publish) {
    published_ = true;
    last_publication_ = now;
    ui_.update(player_.latest());
    service();
  }
  ui_.input(input, now);
  if (failure_ != ApplicationFailure::None)
    return;
  if (!drawing_ && (!framed_ || now - last_frame_ >= 33'333)) {
    const auto start = time();
    frame_started_ = start;
    surface_ = display_.acquire();
    canvas_.begin();
    ui::v2::render_player(ui_.view(), canvas_);
    // Target-only measurements keep CPU timing out of the generic UI/Core
    // snapshot. F is the previous complete frame; S is the worst service gap.
    std::array<char, 64> diagnostic{};
    std::snprintf(diagnostic.data(), diagnostic.size(), "F%llums S%lluus",
                  static_cast<unsigned long long>(metrics_.last_frame_us / 1'000),
                  static_cast<unsigned long long>(metrics_.max_service_gap_us));
    canvas_.text({280, 4, 240, 16}, diagnostic.data(), ui::v2::palette::muted);
    if (ui_.view().snapshot.error) {
      std::snprintf(diagnostic.data(), diagnostic.size(), "ERR %u SND %04X REC %lluus FLIP %lluus",
                    static_cast<unsigned>(ui_.view().snapshot.error->code),
                    static_cast<unsigned>(client_.status()),
                    static_cast<unsigned long long>(metrics_.max_record_us),
                    static_cast<unsigned long long>(metrics_.max_present_us));
      canvas_.fill({16, 344, 608, 16}, ui::v2::palette::background);
      canvas_.text({16, 344, 608, 16}, diagnostic.data(), ui::v2::palette::warning);
    }
    if (!canvas_.seal())
      fail(ApplicationFailure::Canvas);
    metrics_.max_record_us = std::max(metrics_.max_record_us, time() - start);
    drawing_ = true;
    service();
  }
  if (!drawing_ || failure_ != ApplicationFailure::None)
    return;
  const auto start = time();
  static_cast<void>(canvas_.pump(surface_.pixels, surface_.bytes, surface_.stride, 4'096));
  if (canvas_.failed())
    fail(ApplicationFailure::Canvas);
  metrics_.max_pump_us = std::max(metrics_.max_pump_us, time() - start);
  service();
  if (failure_ != ApplicationFailure::None || !canvas_.complete())
    return;
  const auto present_start = time();
  const auto result = display_.present();
  metrics_.max_present_us = std::max(metrics_.max_present_us, time() - present_start);
  if (result == PresentResult::Failed)
    fail(ApplicationFailure::Display);
  if (result == PresentResult::Presented) {
    ++metrics_.frames;
    metrics_.last_frame_us = time() - frame_started_;
    drawing_ = false;
    framed_ = true;
    last_frame_ = now;
  }
  service();
}

player::CatalogChangeResult load_player_library(LibrarySlot& slot, std::uint8_t* storage,
                                                const std::size_t capacity,
                                                player::CatalogSession& catalog) {
  const auto opened = catalog.begin_open();
  if (!opened.ok())
    return opened;
  std::uint32_t size{};
  if (!storage || !slot.size(size) || size == 0 || size > library::kAlbumMaxFileBytes ||
      size > capacity)
    return catalog.fail_open(opened.generation);
  for (std::uint32_t offset = 0; offset < size;) {
    const auto count = std::min(kLibraryReadChunk, size - offset);
    if (!slot.read(offset, storage + offset, count))
      return catalog.fail_open(opened.generation);
    offset += count;
  }
  return catalog.complete_open(opened.generation, {storage, size});
}
} // namespace rpcmp::platform::pocket
