#include "rpcmp/player/mdx_performance_mapper.hpp"

#include "rpcmp/runtime/mdx_ym2151_router.hpp"

#include <limits>

namespace rpcmp::player {
namespace api = contracts::v2;
namespace mdx = runtime::mdx;
namespace {
bool valid_channel(const mdx::Ym2151ObservedChannel& channel) noexcept {
  return !channel.pitch_64 || *channel.pitch_64 <= 0x17ff;
}
std::optional<api::PerformanceKind> kind(const mdx::Ym2151PerformanceKind input) noexcept {
  switch (input) {
  case mdx::Ym2151PerformanceKind::KeyOn:
    return api::PerformanceKind::KeyOn;
  case mdx::Ym2151PerformanceKind::KeyOff:
    return api::PerformanceKind::KeyOff;
  case mdx::Ym2151PerformanceKind::PitchChanged:
    return api::PerformanceKind::PitchChanged;
  case mdx::Ym2151PerformanceKind::InstrumentChanged:
    return api::PerformanceKind::InstrumentChanged;
  }
  return std::nullopt;
}
} // namespace

api::PerformanceChannel MdxPerformanceMapper::channel(const mdx::Ym2151ObservedChannel& input,
                                                      const std::uint8_t id) const noexcept {
  api::PerformanceChannel result;
  result.channel_id = id;
  if (input.gate_known)
    result.key_on = input.key_on;
  if (input.pitch_64) {
    const std::int32_t correction = chip_clock_hz_ == 4'000'000 ? 12'305 : 0;
    const std::int32_t cents_64 = (13 * 64 + *input.pitch_64) * 100 + correction;
    const auto note = (cents_64 + 3'200) / 6'400;
    const auto delta = cents_64 - note * 6'400;
    const auto cents = delta >= 0 ? (delta + 32) / 64 : -((-delta + 32) / 64);
    result.note = static_cast<std::uint8_t>(note);
    result.fine_pitch_cents = static_cast<std::int16_t>(cents);
  }
  if (input.voice)
    result.mdx_fm = api::MdxFmV1{input.voice};
  return result;
}

MdxPerformanceResult MdxPerformanceMapper::commit(const std::uint64_t play_generation,
                                                  const mdx::SequencedPerformance& source,
                                                  const MdxPerformanceBoundary& boundary) noexcept {
  if (play_generation != 0 && play_generation < history_.play_generation())
    return MdxPerformanceResult::Stale;
  const auto& batch = source.batch;
  if (play_generation == 0 || play_generation != history_.play_generation() ||
      (chip_clock_hz_ != 3'579'545 && chip_clock_hz_ != 4'000'000) ||
      boundary.source_tick != source.at_tick || batch.count > api::kPerformanceCapacity ||
      batch.write_count > mdx::kMdxMaxYm2151WritesPerBatch ||
      batch.lost_before > mdx::kMdxMaxYm2151WritesPerBatch ||
      (boundary.event_frames && boundary.event_frames->count != batch.count))
    return MdxPerformanceResult::Invalid;
  for (std::uint8_t i = 0; i < api::kPerformanceChannels; ++i) {
    if (!valid_channel(batch.channels[i]))
      return MdxPerformanceResult::Invalid;
    channels_[i] = channel(batch.channels[i], i);
  }
  std::uint16_t previous_write = 0;
  std::uint64_t previous_frame = 0;
  for (std::uint16_t i = 0; i < batch.count; ++i) {
    const auto& event = batch.events[i];
    const auto mapped_kind = kind(event.kind);
    const auto frame = boundary.event_frames ? boundary.event_frames->frames[i] : boundary.at_frame;
    if (!mapped_kind || event.channel >= api::kPerformanceChannels || event.after_write == 0 ||
        event.after_write < previous_write || event.after_write > batch.write_count ||
        !valid_channel(event.state) || frame < previous_frame || frame > boundary.at_frame ||
        (event.after_write == previous_write && frame != previous_frame))
      return MdxPerformanceResult::Invalid;
    changes_[i] = {frame, channel(event.state, event.channel), *mapped_kind};
    if (!api::valid_performance_change(changes_[i]))
      return MdxPerformanceResult::Invalid;
    previous_write = event.after_write;
    previous_frame = frame;
  }
  if (boundary.at_frame > boundary.through_frame)
    return MdxPerformanceResult::Future;
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  const auto lost_before = boundary.omitted_before > maximum - batch.lost_before
                               ? maximum
                               : boundary.omitted_before + batch.lost_before;
  PerformanceCommit input{play_generation,
                          boundary.through_frame,
                          channels_,
                          changes_,
                          lost_before,
                          batch.capture_lost || boundary.capture_lost,
                          boundary.omitted_before_events,
                          boundary.omitted_after};
  input.changes.count = batch.count;
  switch (history_.commit(input)) {
  case PerformanceCaptureResult::Applied:
    return MdxPerformanceResult::Applied;
  case PerformanceCaptureResult::Stale:
    return MdxPerformanceResult::Stale;
  case PerformanceCaptureResult::Invalid:
    return MdxPerformanceResult::Invalid;
  case PerformanceCaptureResult::Exhausted:
    return MdxPerformanceResult::Exhausted;
  }
  return MdxPerformanceResult::Invalid;
}

static_assert(mdx::kMdxPerformanceCapacity == api::kPerformanceCapacity);

} // namespace rpcmp::player
