#include "rpcmp/contracts/performance_history_v2.hpp"

#include <limits>

namespace rpcmp::contracts::v2 {

PerformanceChannels unknown_performance_channels() noexcept {
  PerformanceChannels result{};
  for (std::uint16_t i = 0; i < kPerformanceChannels; ++i)
    result[i].channel_id = i;
  return result;
}

bool valid_performance_channel(const PerformanceChannel& channel) noexcept {
  return channel.channel_id < kPerformanceChannels && (!channel.note || *channel.note <= 127) &&
         (!channel.fine_pitch_cents || channel.note.has_value());
}

bool valid_performance_change(const PerformanceChange& change) noexcept {
  if (!valid_performance_channel(change.channel) || !change.channel.key_on)
    return false;
  switch (change.kind) {
  case PerformanceKind::KeyOn:
    return *change.channel.key_on;
  case PerformanceKind::KeyOff:
    return !*change.channel.key_on;
  case PerformanceKind::PitchChanged:
  case PerformanceKind::InstrumentChanged:
    return true;
  }
  return false;
}

bool valid_performance_history(const PerformanceHistorySnapshot& history) noexcept {
  if (history.version != 1 || history.count > kPerformanceCapacity || history.next_sequence == 0)
    return false;
  for (std::uint16_t i = 0; i < kPerformanceChannels; ++i)
    if (history.channels[i].channel_id != i || !valid_performance_channel(history.channels[i]))
      return false;
  const bool lost = history.retention_lost || history.capture_lost;
  switch (history.availability) {
  case PerformanceAvailability::Waiting:
  case PerformanceAvailability::Invalid:
    return history.count == 0 && history.next_sequence == 1 && !lost &&
           history.channels == unknown_performance_channels();
  case PerformanceAvailability::Available:
    if (lost)
      return false;
    break;
  case PerformanceAvailability::Degraded:
    if (!lost)
      return false;
    break;
  case PerformanceAvailability::Exhausted:
    if (history.next_sequence != std::numeric_limits<std::uint64_t>::max() || !history.capture_lost)
      return false;
    break;
  default:
    return false;
  }
  if (history.play_generation == 0)
    return false;
  std::uint64_t previous = 0;
  std::uint64_t frame = 0;
  for (std::uint16_t i = 0; i < history.count; ++i) {
    const auto& event = history.events[i];
    if (event.sequence <= previous || event.sequence >= history.next_sequence ||
        event.change.at_frame < frame || event.change.at_frame > history.observed_through_frame ||
        !valid_performance_change(event.change) ||
        (i != 0 && event.sequence != previous + 1 && !history.capture_lost))
      return false;
    previous = event.sequence;
    frame = event.change.at_frame;
  }
  if (!history.capture_lost && previous != history.next_sequence - 1)
    return false;
  if (history.count != 0 && history.events[0].sequence != 1 && !lost)
    return false;
  return true;
}

} // namespace rpcmp::contracts::v2
