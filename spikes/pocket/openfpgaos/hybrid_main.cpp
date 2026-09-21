#include "hybrid_playback.hpp"
#include "player_sdk_adapter.h"
#include "rpcmp/platform/pocket/minimal_display.hpp"

#include <algorithm>
#include <cstdio>
#include <new>

extern "C" std::uint32_t rpcmp_pocket_time_us();
namespace {
namespace pocket = rpcmp::platform::pocket;
class Slot final : public rpcmp::library::PlaylistSource {
public:
  bool size(std::uint32_t& bytes) override { return rpcmp_pocket_slot_size(4, &bytes) == 0; }
  bool read(std::uint32_t offset, std::uint8_t* output, std::uint32_t bytes) override {
    std::uint32_t elapsed{};
    return rpcmp_pocket_target_read(4, offset, output, bytes, &elapsed) == 0;
  }
};
} // namespace
int main() {
  // Keep the catalog's large zeroed arrays in BSS, not initialized ELF data.
  alignas(rpcmp::library::PreparedPlaylist) static std::uint8_t
      track_storage[sizeof(rpcmp::library::PreparedPlaylist)]{};
  auto& tracks = *::new (static_cast<void*>(track_storage)) rpcmp::library::PreparedPlaylist;
  Slot slot;
  pocket::HybridPlayback playback{tracks};
  rpcmp::player::minimal::Player player{tracks, playback};
  if (rpcmp_player_mmio_read(0x40000400) != 0x48594233 || !player.initialize())
    return 1;
  rpcmp_pocket_terminal_init();
  std::printf("RPCMP: Loading playlist...\n");
  if (rpcmp_player_cpu_hz() != 90000000 || rpcmp_pocket_target_read_prepare(65536) != 0 ||
      !tracks.open(slot)) {
    std::printf("Playlist load failed. Check the file and restart.\n");
    rpcmp_pocket_hold_result();
    return 2;
  }
  if (rpcmp_player_video_init(pocket::kMinimalPalette.data(),
                              static_cast<std::uint32_t>(pocket::kMinimalPalette.size())) != 0)
    return 3;
  rpcmp::ui::minimal::Controller ui{tracks, player};
  static pocket::MinimalDisplay display;
  std::uint8_t* surface = nullptr;
  std::uint64_t shown{};
  std::uint32_t max_draw{}, max_flip{};
  bool drawing = false;
  for (;;) {
    const auto key = rpcmp_player_mmio_read(0x40000050);
    const auto type = key >> 28U;
    ui.input(key & 0xffffU, type >= 1 && type <= 3, rpcmp_pocket_time_us());
    player.service();
    ui.update(player.snapshot());
    const auto view = ui.view();
    if (!drawing && shown != view.revision) {
      const auto timings = playback.metrics();
      display.begin(
          view, {timings.render_us, timings.feed_us, max_draw, max_flip, timings.minimum_queued});
      shown = view.revision;
      surface = rpcmp_player_video_surface();
      drawing = true;
    }
    if (drawing) {
      const auto draw_begin = rpcmp_pocket_time_us();
      if (!display.pump(surface, std::size_t{640} * 480)) {
        player.submit({rpcmp::contracts::minimal::CommandKind::Stop, {}});
        return 4;
      }
      max_draw = std::max(max_draw, rpcmp_pocket_time_us() - draw_begin);
      player.service();
      if (display.complete()) {
        const auto flip_begin = rpcmp_pocket_time_us();
        if (rpcmp_player_video_present() == 0)
          drawing = false;
        max_flip = std::max(max_flip, rpcmp_pocket_time_us() - flip_begin);
      }
    }
  }
}
