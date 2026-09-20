#include "rpcmp/player/mdx_source_producer.hpp"

#include <limits>

namespace rpcmp::player {

MdxSourceResult validate_mdx_source_tick(const runtime::mdx::TimedYm2151Batch& writes,
                                         const runtime::mdx::SequencedProgress& progress,
                                         const std::uint64_t next_tick,
                                         const MdxSourceContinuity& continuity) noexcept {
  if (writes.count > writes.writes.size() || progress.at_tick != continuity.at_tick ||
      progress.completed_loops < continuity.completed_loops ||
      (progress.ended ? next_tick != 0 : next_tick <= progress.at_tick))
    return MdxSourceResult::Invalid;
  for (std::size_t index = 0; index < writes.count; ++index) {
    if (writes.writes[index].at_tick != progress.at_tick ||
        writes.writes[index].write.logical_channel >= runtime::mdx::kMdxFmTrackCount)
      return MdxSourceResult::Invalid;
  }
  if (writes.count + 1 > std::numeric_limits<std::uint64_t>::max() - continuity.accepted_token)
    return MdxSourceResult::Exhausted;
  return MdxSourceResult::Accepted;
}

MdxSourceResult RetainedMdxSource::begin(const std::uint64_t generation,
                                         const std::uint64_t epoch) noexcept {
  if (generation == 0 || generation < generation_ || epoch == 0 || epoch <= epoch_)
    return MdxSourceResult::Invalid;
  generation_ = generation;
  epoch_ = epoch;
  continuity_ = {};
  progress_ = {};
  next_tick_ = 0;
  count_ = 0;
  cursor_ = 0;
  in_flight_ = false;
  state_ = State::Ready;
  return MdxSourceResult::Accepted;
}

void RetainedMdxSource::cancel() noexcept {
  state_ = State::Cancelled;
  in_flight_ = false;
}

MdxSourceResult RetainedMdxSource::availability() const noexcept {
  if (state_ == State::Ready)
    return MdxSourceResult::Accepted;
  if (state_ == State::Pending)
    return MdxSourceResult::Busy;
  return state_ == State::Faulted ? MdxSourceResult::DeviceFailure : MdxSourceResult::Closed;
}

MdxSourceResult RetainedMdxSource::retain(const runtime::mdx::TimedYm2151Batch& writes,
                                          const runtime::mdx::SequencedProgress& progress,
                                          const std::uint64_t next_tick) noexcept {
  const auto available = availability();
  if (available != MdxSourceResult::Accepted)
    return available;
  const auto checked = validate_mdx_source_tick(writes, progress, next_tick, continuity_);
  if (checked != MdxSourceResult::Accepted)
    return checked;
  for (std::size_t index = 0; index < writes.count; ++index)
    writes_[index] = {writes.writes[index].write.address, writes.writes[index].write.value};
  progress_ = progress;
  next_tick_ = next_tick;
  count_ = static_cast<std::uint16_t>(writes.count);
  cursor_ = 0;
  state_ = State::Pending;
  return MdxSourceResult::Accepted;
}

std::optional<MdxSourceOffer> RetainedMdxSource::take_offer() noexcept {
  if (state_ != State::Pending || in_flight_)
    return std::nullopt;
  MdxSourceOffer offer;
  offer.generation = generation_;
  offer.epoch = epoch_;
  offer.token = continuity_.accepted_token + 1;
  offer.at_tick = progress_.at_tick;
  offer.marker = cursor_ == count_;
  if (offer.marker) {
    offer.until_tick = next_tick_;
    offer.completed_loops = progress_.completed_loops;
    offer.ended = progress_.ended;
  } else {
    offer.address = writes_[cursor_].address;
    offer.value = writes_[cursor_].value;
  }
  in_flight_ = true;
  return offer;
}

MdxSourceResult RetainedMdxSource::complete(const MdxFeedCompletion& completion) noexcept {
  if (completion.generation != generation_ || completion.epoch != epoch_ ||
      state_ == State::Cancelled || state_ == State::Uninitialized)
    return MdxSourceResult::Stale;
  if (state_ == State::Faulted)
    return MdxSourceResult::DeviceFailure;
  if (!in_flight_ || completion.token != continuity_.accepted_token + 1) {
    state_ = State::Faulted;
    return MdxSourceResult::ProtocolError;
  }
  in_flight_ = false;
  switch (completion.status) {
  case MdxFeedStatus::Full:
    return MdxSourceResult::Full;
  case MdxFeedStatus::Accepted:
    ++continuity_.accepted_token;
    if (cursor_ == count_) {
      continuity_.at_tick = next_tick_;
      continuity_.completed_loops = progress_.completed_loops;
      state_ = progress_.ended ? State::Ended : State::Ready;
    } else {
      ++cursor_;
    }
    return MdxSourceResult::Accepted;
  case MdxFeedStatus::Invalid:
  case MdxFeedStatus::Closed:
  case MdxFeedStatus::Stale:
  case MdxFeedStatus::Failed:
    state_ = State::Faulted;
    return MdxSourceResult::DeviceFailure;
  }
  state_ = State::Faulted;
  return MdxSourceResult::ProtocolError;
}

MdxProductionResult produce_mdx_tick(const runtime::mdx::MdxDocument& document,
                                     runtime::mdx::MdxEngineState& state, RetainedMdxSource& source,
                                     MdxProducedCheckpoint& checkpoint,
                                     MdxSourceWorkspace& workspace,
                                     const runtime::mdx::PlaybackLimits limits) noexcept {
  const auto available = source.availability();
  if (available != MdxSourceResult::Accepted)
    return {available, {}};
  workspace.candidate = state;
  const auto advanced = runtime::mdx::advance_mdx_tick(
      document, kMdxSourceTickRate, workspace.candidate, workspace.batch, workspace.engine, limits);
  if (!advanced.ok())
    return {MdxSourceResult::EngineFailure, advanced};
  const auto& candidate = workspace.candidate;
  const auto retained =
      source.retain(workspace.batch, candidate.progress,
                    candidate.progress.ended ? 0 : candidate.timeline.scheduler_tick);
  if (retained != MdxSourceResult::Accepted)
    return {retained, {}};
  checkpoint.generation = source.generation();
  checkpoint.epoch = source.epoch();
  checkpoint.preceding_token = source.accepted_token();
  checkpoint.marker_token = checkpoint.preceding_token + workspace.batch.count + 1;
  checkpoint.performance = candidate.performance;
  state = candidate;
  return {MdxSourceResult::Accepted, {}};
}

} // namespace rpcmp::player
