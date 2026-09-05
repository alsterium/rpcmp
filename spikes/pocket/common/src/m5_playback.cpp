#include "rpcmp/spike/m5_playback.hpp"

#include <limits>

namespace rpcmp::spike {

M5PlaybackPump::M5PlaybackPump(player::MdxLibrarySession& session,
                               player::MdxLibrarySessionWorkspace& workspace,
                               IM5PlaybackDevice& device) noexcept
    : session_(session), workspace_(workspace), device_(device) {}

M5PlaybackState M5PlaybackPump::fail(const M5PlaybackError error) noexcept {
  error_ = error;
  state_ = device_.reset() ? M5PlaybackState::Fault : M5PlaybackState::ResetFailed;
  return state_;
}

bool M5PlaybackPump::ended() const noexcept {
  for (std::size_t index = 0; index < runtime::mdx::kMdxFmTrackCount; ++index) {
    if (!session_.state.document.tracks[index].ended)
      return false;
  }
  return true;
}

bool M5PlaybackPump::shifted(const std::uint64_t tick, std::uint64_t& due) noexcept {
  if (tick > std::numeric_limits<std::uint64_t>::max() - kM5StartupLead) {
    static_cast<void>(fail(M5PlaybackError::Overflow));
    return false;
  }
  due = tick + kM5StartupLead;
  return true;
}

M5PlaybackState M5PlaybackPump::service() noexcept {
  if (state_ != M5PlaybackState::Running)
    return state_;
  std::uint64_t now{};
  if (!device_.now(now) || now < last_now_)
    return fail(M5PlaybackError::Clock);
  last_now_ = now;
  bool empty{};
  if (!device_.status(empty))
    return fail(M5PlaybackError::Device);

  unsigned ticks{};
  unsigned submissions{};
  for (;;) {
    while (pending_ < batch_.count) {
      const auto& write = batch_.writes[pending_];
      std::uint64_t due{};
      if (!shifted(write.at_tick, due))
        return state_;
      if (due > now && due - now > kM5Lookahead)
        return state_;
      if (submissions == 512)
        return state_;
      ++submissions;
      const auto submitted = device_.submit(due, write.write);
      if (submitted == M5Submit::Fault)
        return fail(M5PlaybackError::Device);
      if (submitted == M5Submit::Backpressure) {
        if (!blocked_) {
          blocked_ = true;
          blocked_at_ = now;
        }
        return now - blocked_at_ >= kM5DeviceTimeout ? fail(M5PlaybackError::Timeout) : state_;
      }
      blocked_ = false;
      ++pending_;
      ++writes_;
      digest_ = (digest_ * 1099511628211ULL) ^ write.at_tick;
      digest_ = (digest_ * 1099511628211ULL) ^ write.write.address;
      digest_ = (digest_ * 1099511628211ULL) ^ write.write.value;
    }

    std::uint64_t next_due{};
    if (!shifted(session_.state.timeline.scheduler_tick, next_due))
      return state_;
    if (ended()) {
      if (now < next_due)
        return state_;
      // Re-read after this call's submissions; the entry observation is stale.
      if (!device_.status(empty))
        return fail(M5PlaybackError::Device);
      if (!empty)
        return now - next_due >= kM5DeviceTimeout ? fail(M5PlaybackError::Timeout) : state_;
      state_ = device_.reset() ? M5PlaybackState::Complete : M5PlaybackState::ResetFailed;
      return state_;
    }
    if ((next_due > now && next_due - now > kM5Lookahead) || ticks == 4)
      return state_;
    ++ticks;
    if (!runtime::mdx::advance_mdx_tick(session_.document, 48'000, session_.state, batch_,
                                        workspace_.engine)
             .ok())
      return fail(M5PlaybackError::Engine);
    pending_ = 0;
  }
}

} // namespace rpcmp::spike
