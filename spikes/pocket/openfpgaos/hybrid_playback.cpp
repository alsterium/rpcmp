#include "hybrid_playback.hpp"

#include "player_sdk_adapter.h"

#include <algorithm>
#include <cstdlib>
#include <memory>

extern "C" std::uint32_t rpcmp_pocket_time_us();
namespace rpcmp::platform::pocket {
namespace {
std::uint32_t read(unsigned word) { return rpcmp_player_mmio_read(0x40000400U + word * 4U); }
void write(unsigned word, std::uint32_t value) {
  rpcmp_player_mmio_write(0x40000400U + word * 4U, value);
}
bool ready() {
  const auto begin = rpcmp_pocket_time_us();
  while (!(read(1) & 1U)) {
    if (rpcmp_pocket_time_us() - begin > 5000)
      return false;
  }
  return true;
}
} // namespace
bool HybridPlayback::stop() {
  pending_ = nullptr;
  started_ = eof_ = paused_ = false;
  repeat_ = contracts::minimal::RepeatMode::Two;
  const bool ok = ready();
  if (ok)
    write(2, 1);
  rpcmp_hybrid_close();
  return ok && ready() && read(1) == 1;
}
contracts::minimal::Error HybridPlayback::set_paused(const bool paused) {
  using contracts::minimal::Error;
  if (paused == paused_)
    return Error::None;
  const auto begin = rpcmp_pocket_time_us();
  if (started_) {
    if (!ready() || (read(1) & 0xf0U) != 0)
      return Error::Audio;
    write(2, paused ? 4 : 8);
    for (;;) {
      const auto status = read(1);
      if ((status & 0xf0U) != 0)
        return Error::Audio;
      if (((status & 0x100U) != 0) == paused)
        break;
      if (rpcmp_pocket_time_us() - begin > 5000)
        return Error::Timeout;
    }
  }
  if (paused)
    pause_started_ = begin;
  else
    wait_started_ += rpcmp_pocket_time_us() - pause_started_;
  paused_ = paused;
  return Error::None;
}
contracts::minimal::Error HybridPlayback::set_repeat(const contracts::minimal::RepeatMode mode) {
  using contracts::minimal::Error;
  if (static_cast<unsigned>(mode) > 3)
    return Error::Renderer;
  while (repeat_ != mode) {
    if (!ready() || (read(1) & 0xf0U) != 0)
      return Error::Audio;
    const auto previous_ack = read(1) & 0x200U;
    const auto begin = rpcmp_pocket_time_us();
    write(2, 16);
    for (;;) {
      const auto status = read(1);
      if ((status & 0xf0U) != 0)
        return Error::Audio;
      if ((status & 0x200U) != previous_ack)
        break;
      if (rpcmp_pocket_time_us() - begin > 5000)
        return Error::Timeout;
    }
    repeat_ = static_cast<contracts::minimal::RepeatMode>((static_cast<unsigned>(repeat_) + 1) % 4);
  }
  return Error::None;
}
contracts::minimal::Error HybridPlayback::open(const contracts::TrackId id) {
  using contracts::minimal::Error;
  std::uint32_t a{}, b{};
  if (!tracks_.sizes(id, a, b))
    return Error::Load;
  using Buffer = std::unique_ptr<std::uint8_t, decltype(&std::free)>;
  Buffer mdx{static_cast<std::uint8_t*>(std::malloc(a + 16U)), &std::free};
  Buffer pdx{b ? static_cast<std::uint8_t*>(std::malloc(b + 16U)) : nullptr, &std::free};
  if (!tracks_.load(id, {mdx.get(), a + 16U}, {pdx.get(), b + 16U}))
    return Error::Load;
  if (rpcmp_hybrid_open(mdx.get(), a, pdx.get(), b) != 0)
    return Error::Renderer;
  metrics_ = {};
  wait_started_ = rpcmp_pocket_time_us();
  return Error::None;
}
contracts::minimal::Error HybridPlayback::service(bool& ended) {
  using contracts::minimal::Error;
  ended = false;
  if (paused_)
    return Error::None;
  const auto status = read(1);
  if ((status & 0xf0U) != 0)
    return Error::Audio;
  if ((status & 8U) != 0) {
    ended = true;
    return Error::None;
  }
  if (eof_)
    return rpcmp_pocket_time_us() - wait_started_ > 1'000'000 ? Error::Timeout : Error::None;
  const auto free_frames = read(6);
  if (free_frames > 4096)
    return Error::Audio;
  if (started_)
    metrics_.minimum_queued = std::min(metrics_.minimum_queued, 4096 - free_frames);
  if (!pending_ && free_frames >= RPCMP_HYBRID_FRAMES) {
    const auto begin = rpcmp_pocket_time_us();
    pending_ = rpcmp_hybrid_render();
    metrics_.render_us = std::max(metrics_.render_us, rpcmp_pocket_time_us() - begin);
    const auto remaining = read(6);
    if (remaining > 4096)
      return Error::Audio;
    if (started_)
      metrics_.minimum_queued = std::min(metrics_.minimum_queued, 4096 - remaining);
    if (!pending_)
      return Error::Renderer;
    wait_started_ = rpcmp_pocket_time_us();
  }
  if (!pending_)
    return Error::None;
  const auto needed = pending_->event_count + (pending_->ended ? 1U : 0U);
  if (pending_->event_count > RPCMP_HYBRID_EVENTS || needed > 1024 ||
      (pending_->frame_count == 0 && !pending_->ended) ||
      pending_->frame_count > RPCMP_HYBRID_FRAMES)
    return Error::Renderer;
  if (!started_ && pending_->ended && pending_->frame_count == 0) {
    ended = true;
    return Error::None;
  }
  if (rpcmp_pocket_time_us() - wait_started_ > 1'000'000)
    return Error::Timeout;
  if (read(6) < pending_->frame_count || read(10) < needed)
    return Error::None;
  const auto begin = rpcmp_pocket_time_us();
  for (unsigned i = 0; i < pending_->event_count; ++i) {
    write(8, pending_->events[i].sample);
    write(9, pending_->events[i].operation);
  }
  if (pending_->ended) {
    write(8, pending_->first_frame + pending_->frame_count);
    write(9, 0x10000);
    eof_ = true;
  }
  for (std::size_t i = 0; i < pending_->frame_count; ++i) {
    write(4, static_cast<std::uint32_t>(pending_->pcm[2 * i]));
    write(5, static_cast<std::uint32_t>(pending_->pcm[2 * i + 1]));
  }
  metrics_.feed_us = std::max(metrics_.feed_us, rpcmp_pocket_time_us() - begin);
  pending_ = nullptr;
  if (!started_) {
    write(2, 2);
    started_ = true;
  }
  return Error::None;
}
} // namespace rpcmp::platform::pocket
