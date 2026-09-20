#include "rpcmp/player/mdx_output_history.hpp"

#include <limits>

namespace rpcmp::player {
namespace {
constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
std::uint64_t sum(const std::uint64_t a, const std::uint64_t b) noexcept {
  return b > maximum - a ? maximum : a + b;
}
} // namespace

bool MdxOutputHistory::begin(const std::uint64_t generation, const std::uint64_t epoch) noexcept {
  if (generation == 0 || generation <= generation_ || epoch == 0 || epoch <= epoch_ ||
      !history_.begin(generation))
    return false;
  generation_ = generation;
  epoch_ = epoch;
  first_ = 0;
  count_ = 0;
  produced_token_ = 0;
  produced_tick_ = 0;
  last_sequence_ = 0;
  last_frame_ = 0;
  last_prefix_ = 0;
  committed_token_ = 0;
  checkpoint_frame_ = 0;
  pending_loss_ = 0;
  observed_frame_ = 0;
  open_ = true;
  have_produced_ = false;
  have_record_ = false;
  terminal_ = false;
  unknown_loss_ = false;
  current_valid_ = false;
  publish_ready_ = false;
  sequence_exhausted_ = false;
  return true;
}

void MdxOutputHistory::cancel() noexcept {
  open_ = false;
  count_ = 0;
  publish_ready_ = false;
}

void MdxOutputHistory::lose(const std::uint64_t events, const bool unknown) noexcept {
  pending_loss_ = sum(pending_loss_, events);
  unknown_loss_ |= unknown;
}

void MdxOutputHistory::discard_front() noexcept {
  const auto& retained = batches_[first_];
  const auto& item = retained.checkpoint;
  lose(static_cast<std::uint64_t>(item.performance.batch.count) +
           item.performance.batch.lost_before,
       item.performance.batch.capture_lost || retained.unknown_before);
  first_ = (first_ + 1) % kMdxDisplayBatchCapacity;
  --count_;
}

void MdxOutputHistory::invalidate() noexcept {
  while (count_ != 0)
    discard_front();
  lose(0, true);
  publish_ready_ = false;
}

MdxHistoryResult MdxOutputHistory::retain(const MdxProducedCheckpoint& checkpoint) noexcept {
  if (!open_)
    return MdxHistoryResult::Closed;
  if (checkpoint.generation != generation_ || checkpoint.epoch != epoch_)
    return MdxHistoryResult::Stale;
  const auto& batch = checkpoint.performance.batch;
  if (batch.count > batch.events.size() ||
      batch.write_count > runtime::mdx::kMdxMaxYm2151WritesPerBatch ||
      checkpoint.preceding_token > maximum - batch.write_count - 1 ||
      checkpoint.marker_token != checkpoint.preceding_token + batch.write_count + 1) {
    invalidate();
    return MdxHistoryResult::Invalid;
  }
  if (checkpoint.marker_token <= produced_token_)
    return MdxHistoryResult::Stale;
  if (checkpoint.preceding_token < produced_token_ ||
      (have_produced_ && checkpoint.performance.at_tick <= produced_tick_)) {
    invalidate();
    return MdxHistoryResult::Invalid;
  }
  std::uint16_t previous = 0;
  for (std::uint16_t i = 0; i < batch.count; ++i) {
    const auto write = batch.events[i].after_write;
    if (write == 0 || write < previous || write > batch.write_count) {
      invalidate();
      return MdxHistoryResult::Invalid;
    }
    previous = write;
  }
  const bool source_gap = checkpoint.preceding_token != produced_token_;
  bool degraded = source_gap;
  if (count_ == kMdxDisplayBatchCapacity) {
    discard_front();
    degraded = true;
  }
  auto& retained = batches_[(first_ + count_) % kMdxDisplayBatchCapacity];
  retained.checkpoint = checkpoint;
  retained.mapped = 0;
  retained.unknown_before = source_gap;
  ++count_;
  produced_token_ = checkpoint.marker_token;
  produced_tick_ = checkpoint.performance.at_tick;
  have_produced_ = true;
  return degraded ? MdxHistoryResult::Degraded : MdxHistoryResult::Accepted;
}

MdxHistoryResult MdxOutputHistory::consume(const MdxOutputRecord& record) noexcept {
  return apply(record, false);
}
MdxHistoryResult MdxOutputHistory::recover(const MdxOutputRecord& latest) noexcept {
  return apply(latest, true);
}

MdxHistoryResult MdxOutputHistory::apply(const MdxOutputRecord& record,
                                         const bool recovering) noexcept {
  if (!open_)
    return MdxHistoryResult::Closed;
  if (record.generation != generation_ || record.epoch != epoch_)
    return MdxHistoryResult::Stale;
  if (record.sequence != 0 && record.sequence <= last_sequence_)
    return MdxHistoryResult::Stale;
  // After sequence exhaustion, frame order identifies repeated latest copies.
  if (recovering && sequence_exhausted_ && record.sequence == 0 && record.frame <= last_frame_)
    return MdxHistoryResult::Stale;
  if (terminal_)
    return MdxHistoryResult::Closed;
  if (record.sequence == maximum || (record.sequence == 0 && !recovering) ||
      (sequence_exhausted_ && record.sequence != 0) || record.prefix == 0 ||
      record.prefix > produced_token_ || (have_record_ && record.frame <= last_frame_) ||
      record.prefix < last_prefix_ || (!record.natural_end && record.prefix == last_prefix_) ||
      (!record.natural_end && record.frame == maximum)) {
    invalidate();
    return MdxHistoryResult::Invalid;
  }
  const bool gap = recovering || sequence_exhausted_ || record.sequence != last_sequence_ + 1;
  unknown_loss_ |= gap;
  publish_ready_ = false;
  MdxHistoryResult result = gap ? MdxHistoryResult::Degraded : MdxHistoryResult::Accepted;
  while (count_ != 0) {
    auto& retained = batches_[first_];
    const auto& checkpoint = retained.checkpoint;
    const auto& source = checkpoint.performance.batch;
    while (retained.mapped < source.count &&
           checkpoint.preceding_token + source.events[retained.mapped].after_write <=
               record.prefix) {
      const auto token = checkpoint.preceding_token + source.events[retained.mapped].after_write;
      retained.known[retained.mapped] = !gap || (!record.natural_end && token == record.prefix);
      retained.frames[retained.mapped] = record.frame;
      ++retained.mapped;
    }
    if (checkpoint.marker_token > record.prefix)
      break;
    mapped_ = checkpoint.performance;
    mapped_.batch.count = 0;
    boundary_ = {};
    boundary_.source_tick = checkpoint.performance.at_tick;
    boundary_.at_frame = record.frame;
    boundary_.through_frame = record.frame;
    boundary_.omitted_before = pending_loss_;
    boundary_.capture_lost = unknown_loss_ || retained.unknown_before;
    boundary_.event_frames.emplace();
    std::uint64_t skipped = 0;
    for (std::uint16_t i = 0; i < source.count; ++i) {
      if (!retained.known[i]) {
        ++skipped;
        continue;
      }
      const auto next = mapped_.batch.count++;
      mapped_.batch.events[next] = source.events[i];
      boundary_.event_frames->frames[next] = retained.frames[i];
      losses_[next] = skipped;
      skipped = 0;
    }
    boundary_.event_frames->count = mapped_.batch.count;
    boundary_.omitted_before_events = {losses_.data(), mapped_.batch.count};
    boundary_.omitted_after = skipped;
    const auto applied = mapper_.commit(generation_, mapped_, boundary_);
    if (applied == MdxPerformanceResult::Applied || applied == MdxPerformanceResult::Exhausted) {
      current_ = checkpoint.performance;
      current_.batch.count = 0;
      current_.batch.lost_before = 0;
      current_.batch.capture_lost = false;
      current_valid_ = true;
      committed_token_ = checkpoint.marker_token;
      checkpoint_frame_ = record.frame;
      pending_loss_ = 0;
      unknown_loss_ = false;
      first_ = (first_ + 1) % kMdxDisplayBatchCapacity;
      --count_;
      if (applied == MdxPerformanceResult::Exhausted)
        result = MdxHistoryResult::Exhausted;
    } else {
      discard_front();
      lose(0, true);
      result = MdxHistoryResult::Invalid;
    }
  }
  if (record.sequence == 0)
    sequence_exhausted_ = true;
  else
    last_sequence_ = record.sequence;
  last_frame_ = record.frame;
  last_prefix_ = record.prefix;
  have_record_ = true;
  terminal_ = record.natural_end;
  return result;
}

MdxHistoryResult MdxOutputHistory::observe(const MdxHistoryObservation& observation) noexcept {
  if (!open_)
    return MdxHistoryResult::Closed;
  if (observation.generation != generation_ || observation.epoch != epoch_ ||
      observation.through_frame < observed_frame_)
    return MdxHistoryResult::Stale;
  observed_frame_ = observation.through_frame;
  publish_ready_ = false;
  if (!current_valid_ || observation.prefix != last_prefix_ ||
      observation.natural_end != terminal_ ||
      (terminal_ ? observation.through_frame != last_frame_
                 : observation.through_frame <= last_frame_) ||
      last_prefix_ > committed_token_)
    return MdxHistoryResult::Waiting;
  boundary_ = {};
  boundary_.source_tick = current_.at_tick;
  boundary_.at_frame = checkpoint_frame_;
  boundary_.through_frame = observation.through_frame;
  const auto applied = mapper_.commit(generation_, current_, boundary_);
  publish_ready_ =
      applied == MdxPerformanceResult::Applied || applied == MdxPerformanceResult::Exhausted;
  return publish_ready_ ? (applied == MdxPerformanceResult::Exhausted ? MdxHistoryResult::Exhausted
                                                                      : MdxHistoryResult::Accepted)
                        : MdxHistoryResult::Invalid;
}

void MdxOutputHistory::copy_to(contracts::v2::PerformanceHistorySnapshot& output) const noexcept {
  if (publish_ready_) {
    history_.copy_to(output);
    return;
  }
  output = {};
  output.play_generation = generation_;
  output.observed_through_frame = observed_frame_;
  output.channels = contracts::v2::unknown_performance_channels();
}

} // namespace rpcmp::player
