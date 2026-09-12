#include "rpcmp/player/media_loop_envelope.hpp"

#include <algorithm>

namespace rpcmp::player {
namespace {
bool valid_target(const std::optional<std::uint32_t> target) noexcept {
  return !target || *target != 0;
}
std::int16_t scale(const std::int16_t sample, const std::uint32_t gain) noexcept {
  return static_cast<std::int16_t>(static_cast<std::int64_t>(sample) * gain /
                                   kMediaGainDenominator);
}
} // namespace

MediaLoopEnvelope::MediaLoopEnvelope(const std::uint64_t max_frames) noexcept
    : max_frames_(max_frames) {}

MediaAdmission MediaLoopEnvelope::begin(const std::uint64_t generation,
                                        const std::optional<std::uint32_t> target,
                                        const std::uint64_t policy_revision) noexcept {
  if (generation == 0 || policy_revision == 0 || !valid_target(target) || max_frames_ == 0)
    return MediaAdmission::Invalid;
  if (generation <= state_.play_generation)
    return MediaAdmission::StaleGeneration;
  state_ = {};
  state_.play_generation = generation;
  state_.target = target;
  state_.policy_revision = policy_revision;
  head_ = 0;
  count_ = 0;
  last_sequence_ = 0;
  last_until_ = 0;
  last_loops_ = 0;
  covered_until_ = 0;
  end_enqueued_ = false;
  ramp_anchor_ = 0;
  ramp_start_gain_ = kMediaGainDenominator;
  ramp_duration_ = 0;
  return MediaAdmission::Accepted;
}

MediaAdmission MediaLoopEnvelope::enqueue(const MediaProgressInterval& progress) noexcept {
  if (progress.play_generation < state_.play_generation)
    return MediaAdmission::StaleGeneration;
  if (state_.play_generation == 0 || progress.play_generation != state_.play_generation)
    return MediaAdmission::Invalid;
  if (state_.failure != MediaFailure::None || state_.end != MediaEnd::None)
    return MediaAdmission::Closed;
  if (last_sequence_ == std::numeric_limits<std::uint64_t>::max())
    return MediaAdmission::ResourceExhausted;
  if (progress.sequence != last_sequence_ + 1 || progress.at_frame != last_until_ ||
      progress.at_frame < state_.frame || progress.until_frame <= progress.at_frame ||
      progress.completed_loops < last_loops_ || end_enqueued_)
    return MediaAdmission::Invalid;
  if (count_ == kMediaProgressCapacity)
    return MediaAdmission::Full;
  queue_[(head_ + count_) % kMediaProgressCapacity] = progress;
  ++count_;
  last_sequence_ = progress.sequence;
  last_until_ = progress.until_frame;
  last_loops_ = progress.completed_loops;
  end_enqueued_ = progress.ended;
  return MediaAdmission::Accepted;
}

std::uint32_t MediaLoopEnvelope::elapsed() const noexcept {
  return static_cast<std::uint32_t>(
      std::min<std::uint64_t>(state_.frame - ramp_anchor_, ramp_duration_));
}

std::uint32_t MediaLoopEnvelope::current_gain() const noexcept {
  if (state_.play_generation == 0 || state_.failure != MediaFailure::None ||
      state_.end != MediaEnd::None)
    return 0;
  if (state_.phase == MediaPhase::Steady)
    return kMediaGainDenominator;
  const std::int64_t endpoint = state_.phase == MediaPhase::Fading ? 0 : kMediaGainDenominator;
  const auto difference = endpoint - static_cast<std::int64_t>(ramp_start_gain_);
  return static_cast<std::uint32_t>(static_cast<std::int64_t>(ramp_start_gain_) +
                                    difference * elapsed() / ramp_duration_);
}

MediaEnvelopeSnapshot MediaLoopEnvelope::snapshot() const noexcept {
  auto result = state_;
  result.gain = current_gain();
  result.ramp_duration = ramp_duration_;
  result.ramp_elapsed = elapsed();
  return result;
}

void MediaLoopEnvelope::ramp(const MediaPhase phase, const std::uint32_t duration,
                             const std::uint32_t start_gain) noexcept {
  state_.phase = phase;
  ramp_anchor_ = state_.frame;
  ramp_start_gain_ = start_gain;
  ramp_duration_ = duration;
}

void MediaLoopEnvelope::finish(const MediaEnd reason) noexcept {
  state_.end = reason;
  state_.phase = MediaPhase::Steady;
  state_.paused = false;
  ramp_duration_ = 0;
  count_ = 0;
}

void MediaLoopEnvelope::fail(const MediaFailure reason) noexcept {
  if (state_.failure == MediaFailure::None)
    state_.failure = reason;
  finish(MediaEnd::None);
}

MediaFrameResult MediaLoopEnvelope::advance(const StereoFrame source,
                                            const MediaBoundaryControl& control) noexcept {
  MediaFrameResult result;
  if (control.device_fault)
    fail(MediaFailure::DeviceFault);
  if (state_.failure != MediaFailure::None)
    return result;
  auto effective = control;
  if (control.play_generation != 0 || control.action != MediaControlAction::Keep ||
      control.repeat) {
    if (control.play_generation != 0 && control.play_generation < state_.play_generation) {
      result.control = MediaControlDisposition::StaleGeneration;
      effective = {};
    } else if (control.play_generation == 0 || control.play_generation != state_.play_generation) {
      result.control = MediaControlDisposition::Rejected;
      fail(MediaFailure::Protocol);
      return result;
    } else {
      result.control = MediaControlDisposition::Applied;
    }
  }
  if (static_cast<std::uint8_t>(effective.action) >
      static_cast<std::uint8_t>(MediaControlAction::Stop)) {
    result.control = MediaControlDisposition::Rejected;
    fail(MediaFailure::Protocol);
    return result;
  }
  if (effective.repeat) {
    const auto& update = *effective.repeat;
    if (update.revision == 0 || !valid_target(update.target) ||
        update.revision < state_.policy_revision ||
        (update.revision == state_.policy_revision && update.target != state_.target)) {
      result.control = MediaControlDisposition::Rejected;
      fail(MediaFailure::Protocol);
      return result;
    }
    state_.policy_revision = update.revision;
    state_.target = update.target;
  }
  if (effective.action == MediaControlAction::Stop)
    finish(MediaEnd::Stopped);
  if (state_.play_generation == 0 || state_.end != MediaEnd::None)
    return result;
  if (effective.action == MediaControlAction::Pause)
    state_.paused = true;
  else if (effective.action == MediaControlAction::Resume)
    state_.paused = false;
  if (state_.paused)
    return result;

  if (count_ != 0 && queue_[head_].at_frame == state_.frame) {
    const auto progress = queue_[head_];
    head_ = static_cast<std::uint16_t>((head_ + 1) % kMediaProgressCapacity);
    --count_;
    state_.completed_loops = progress.completed_loops;
    covered_until_ = progress.until_frame;
    if (progress.ended) {
      finish(state_.target ? MediaEnd::NaturalEnd : MediaEnd::RepeatOne);
      return result;
    }
  }

  const auto gain = current_gain();
  const bool reached = state_.target && state_.completed_loops >= *state_.target;
  if (state_.phase == MediaPhase::Fading && !reached) {
    if (gain == kMediaGainDenominator)
      ramp(MediaPhase::Steady, 0, gain);
    else
      ramp(MediaPhase::RestoringGain, kMediaRestoreFrames, gain);
  } else if (state_.phase != MediaPhase::Fading && reached) {
    ramp(MediaPhase::Fading, kMediaGainDenominator, gain);
  }
  if (state_.phase == MediaPhase::Fading && elapsed() == ramp_duration_) {
    finish(MediaEnd::LoopLimit);
    return result;
  }
  if (state_.phase == MediaPhase::RestoringGain && elapsed() == ramp_duration_)
    ramp(MediaPhase::Steady, 0, kMediaGainDenominator);
  if (state_.frame == max_frames_) {
    fail(MediaFailure::ResourceExhausted);
    return result;
  }
  if (state_.frame >= covered_until_) {
    fail(MediaFailure::Underrun);
    return result;
  }
  const auto applied_gain = current_gain();
  const StereoFrame output{scale(source.left, applied_gain), scale(source.right, applied_gain)};
  ++state_.frame;
  return {output, true, result.control};
}

} // namespace rpcmp::player
