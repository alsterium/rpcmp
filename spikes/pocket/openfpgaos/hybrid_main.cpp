#include "hybrid_renderer.h"
#include "player_sdk_adapter.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" std::uint32_t rpcmp_pocket_time_us();
extern "C" std::uint32_t rpcmp_pocket_poll_actions();

namespace {
std::uint32_t read(unsigned word) { return rpcmp_player_mmio_read(0x40000400U + word * 4U); }
void write(unsigned word, std::uint32_t value) {
  rpcmp_player_mmio_write(0x40000400U + word * 4U, value);
}
constexpr std::uint32_t kInputLimit = 16 * 1024 * 1024;
struct Blob {
  void* data{};
  std::uint32_t size{};
};
Blob mdx, pdx;
bool playing;
unsigned selected;
std::uint32_t max_render_us, max_feed_us, min_queued;

bool ready() {
  const auto begin = rpcmp_pocket_time_us();
  while (!(read(1) & 1U)) {
    if (rpcmp_pocket_time_us() - begin > 5000)
      return false;
  }
  return true;
}
bool stop() {
  playing = false;
  rpcmp_hybrid_close();
  if (!ready())
    return false;
  write(2, 1);
  return ready() && read(1) == 1;
}
bool load(unsigned slot, Blob& blob) {
  if (rpcmp_pocket_slot_size(slot, &blob.size) != 0 || blob.size < 10 || blob.size > kInputLimit)
    return false;
  blob.data = std::calloc(blob.size + 16U, 1);
  if (!blob.data)
    return false;
  for (std::uint32_t offset = 0; offset < blob.size;) {
    const auto count = blob.size - offset > 65536U ? 65536U : blob.size - offset;
    std::uint32_t elapsed{};
    if (rpcmp_pocket_target_read(slot, offset, static_cast<unsigned char*>(blob.data) + offset,
                                 count, &elapsed) != 0)
      return false;
    offset += count;
  }
  return true;
}
void screen(const char* state) {
  std::printf("\033[2J\033[HRPCMP HYB1 CPU/FPGA probe\n\n");
  std::printf("START: select    A: play    B: stop\n");
  const char* choices[] = {"Loaded MDX/PDX", "8 FM + 8 ADPCM", "8 FM + 8 PCM16", "8 FM + 8 PCM8"};
  std::printf("\nSelected: %s\n%s\n", choices[selected], state);
  std::printf("Render max: %lu us\nFeed max: %lu us\nQueue minimum: %lu frames\n",
              static_cast<unsigned long>(max_render_us), static_cast<unsigned long>(max_feed_us),
              static_cast<unsigned long>(min_queued));
}
} // namespace

int main() {
  if (read(0) != 0x48594231 || !stop())
    return 1;
  rpcmp_pocket_terminal_init();
  if (rpcmp_player_cpu_hz() != 90000000 || rpcmp_pocket_target_read_prepare(65536) != 0)
    return 2;
  std::printf("HYB1: loading prepared MDX/PDX...\n");
  bool loaded = load(4, mdx);
  if (loaded) {
    const auto* header = static_cast<const unsigned char*>(mdx.data);
    if (header[2] != 0xff || header[3] != 0xff)
      loaded = load(5, pdx);
  }
  screen(loaded ? "Ready (silent)" : "File load failed; authored cases available");
  const RpcmpHybridBlock* pending = nullptr;
  bool started = false;
  bool eof = false;
  std::uint32_t wait_started = 0;
  for (;;) {
    const auto actions = rpcmp_pocket_poll_actions();
    if (actions & 2U) {
      if (!stop())
        return 3;
      pending = nullptr;
      screen("Stopped");
    }
    if (!playing && (actions & 4U)) {
      selected = (selected + 1) % 4;
      screen("Ready (silent)");
    }
    if (!playing && (actions & 1U)) {
      if (!stop())
        return 4;
      const int result = selected ? rpcmp_hybrid_authored(selected + 3)
                         : loaded ? rpcmp_hybrid_open(mdx.data, mdx.size, pdx.data, pdx.size)
                                  : -1;
      if (result != 0) {
        screen("Open failed");
        continue;
      }
      pending = nullptr;
      started = eof = false;
      max_render_us = max_feed_us = 0;
      min_queued = 4096;
      screen("Playing; display frozen until stop/end");
      playing = true;
      wait_started = rpcmp_pocket_time_us();
    }
    if (!playing)
      continue;
    const auto status = read(1);
    if ((status & 0xf0U) || (status & 8U)) {
      const bool fault = (status & 0xf0U) != 0;
      if (!stop())
        return 5;
      pending = nullptr;
      screen(fault ? "Audio fault" : "Ended");
      std::printf("Status: %08lx\n", static_cast<unsigned long>(status));
      continue;
    }
    if (eof)
      continue;
    const auto free_frames = read(6);
    if (started && 4096 - free_frames < min_queued)
      min_queued = 4096 - free_frames;
    if (!pending && free_frames >= RPCMP_HYBRID_FRAMES) {
      const auto begin = rpcmp_pocket_time_us();
      pending = rpcmp_hybrid_render();
      const auto duration = rpcmp_pocket_time_us() - begin;
      if (duration > max_render_us)
        max_render_us = duration;
      const auto queued = 4096 - read(6);
      if (started && queued < min_queued)
        min_queued = queued;
      if (!pending) {
        if (!stop())
          return 6;
        screen("Renderer failed");
        continue;
      }
      wait_started = rpcmp_pocket_time_us();
    }
    if (!pending)
      continue;
    // The final marker also needs a slot. This bounded fixture rejects a full
    // 1024-event final block instead of deadlocking before its first Start.
    const auto needed = pending->event_count + (pending->ended ? 1U : 0U);
    if (needed > 1024 || rpcmp_pocket_time_us() - wait_started > 1000000) {
      if (!stop())
        return 7;
      pending = nullptr;
      screen("Queue capacity/timeout");
      continue;
    }
    if (read(6) < pending->frame_count || read(10) < needed)
      continue;
    const auto begin = rpcmp_pocket_time_us();
    for (unsigned i = 0; i < pending->event_count; ++i) {
      write(8, pending->events[i].sample);
      write(9, pending->events[i].operation);
    }
    if (pending->ended) {
      write(8, pending->first_frame + pending->frame_count);
      write(9, 0x10000);
      eof = true;
    }
    for (std::size_t i = 0; i < pending->frame_count; ++i) {
      write(4, static_cast<std::uint32_t>(pending->pcm[2 * i]));
      write(5, static_cast<std::uint32_t>(pending->pcm[2 * i + 1]));
    }
    const auto duration = rpcmp_pocket_time_us() - begin;
    if (duration > max_feed_us)
      max_feed_us = duration;
    pending = nullptr;
    if (!started) {
      write(2, 2);
      started = true;
    }
  }
}
