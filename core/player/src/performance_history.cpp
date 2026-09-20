#include "rpcmp/player/performance_history.hpp"

#include <limits>

namespace rpcmp::player {
namespace api = contracts::v2;
namespace {
bool valid_commit(const PerformanceCommit& input, const std::uint64_t previous_frame) noexcept {
  if (input.through_frame < previous_frame || input.changes.count > api::kPerformanceCapacity ||
      (input.changes.count != 0 && input.changes.data == nullptr) ||
      (input.lost_before_changes.count != 0 &&
       (input.lost_before_changes.count != input.changes.count ||
        input.lost_before_changes.data == nullptr)))
    return false;
  for (std::uint16_t i = 0; i < api::kPerformanceChannels; ++i)
    if (input.channels[i].channel_id != i || !api::valid_performance_channel(input.channels[i]))
      return false;
  auto frame = previous_frame;
  for (std::size_t i = 0; i < input.changes.count; ++i) {
    const auto& change = input.changes.data[i];
    if (change.at_frame < frame || change.at_frame > input.through_frame ||
        !api::valid_performance_change(change))
      return false;
    frame = change.at_frame;
  }
  return true;
}
} // namespace

PerformanceHistory::PerformanceHistory() noexcept {
  retained_.channels = api::unknown_performance_channels();
}

bool PerformanceHistory::begin(const std::uint64_t play_generation) noexcept {
  if (play_generation == 0 || play_generation <= retained_.play_generation)
    return false;
  retained_ = {};
  retained_.channels = api::unknown_performance_channels();
  retained_.play_generation = play_generation;
  retained_.availability = api::PerformanceAvailability::Available;
  first_ = 0;
  return true;
}

PerformanceCaptureResult PerformanceHistory::commit(const PerformanceCommit& input) noexcept {
  if (input.play_generation != 0 && input.play_generation < retained_.play_generation)
    return PerformanceCaptureResult::Stale;
  if (retained_.play_generation == 0)
    return PerformanceCaptureResult::Invalid;
  if (input.play_generation != retained_.play_generation ||
      !valid_commit(input, retained_.observed_through_frame)) {
    retained_.capture_lost = true;
    if (retained_.availability != api::PerformanceAvailability::Exhausted)
      retained_.availability = api::PerformanceAvailability::Degraded;
    return PerformanceCaptureResult::Invalid;
  }
  retained_.observed_through_frame = input.through_frame;
  retained_.channels = input.channels;
  constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
  auto advance = input.lost_before;
  bool known_loss = input.lost_before != 0 || input.lost_after != 0;
  bool overflow = false;
  const auto add = [&advance, &overflow](const std::uint64_t amount) {
    if (amount > maximum - advance)
      overflow = true;
    else
      advance += amount;
  };
  add(input.lost_after);
  for (std::size_t i = 0; i < input.lost_before_changes.count; ++i) {
    const auto omitted = input.lost_before_changes.data[i];
    known_loss |= omitted != 0;
    add(omitted);
  }
  add(input.changes.count);
  if (retained_.availability == api::PerformanceAvailability::Exhausted || overflow ||
      advance > maximum - retained_.next_sequence) {
    retained_.next_sequence = maximum;
    retained_.capture_lost = true;
    retained_.availability = api::PerformanceAvailability::Exhausted;
    return PerformanceCaptureResult::Exhausted;
  }
  retained_.capture_lost |= known_loss || input.unknown_loss;
  retained_.next_sequence += input.lost_before;
  for (std::size_t i = 0; i < input.changes.count; ++i) {
    if (input.lost_before_changes.count != 0)
      retained_.next_sequence += input.lost_before_changes.data[i];
    const auto& change = input.changes.data[i];
    const auto index = (first_ + retained_.count) % api::kPerformanceCapacity;
    retained_.events[index] = {retained_.next_sequence++, change};
    if (retained_.count == api::kPerformanceCapacity) {
      first_ = static_cast<std::uint16_t>((first_ + 1) % api::kPerformanceCapacity);
      retained_.retention_lost = true;
    } else {
      ++retained_.count;
    }
  }
  retained_.next_sequence += input.lost_after;
  retained_.availability = retained_.retention_lost || retained_.capture_lost
                               ? api::PerformanceAvailability::Degraded
                               : api::PerformanceAvailability::Available;
  return PerformanceCaptureResult::Applied;
}

void PerformanceHistory::copy_to(api::PerformanceHistorySnapshot& output) const noexcept {
  output = retained_;
  for (std::uint16_t i = 0; i < retained_.count; ++i)
    output.events[i] = retained_.events[(first_ + i) % api::kPerformanceCapacity];
}

static_assert(sizeof(api::PerformanceEvent) <= 64);
static_assert(sizeof(api::PerformanceHistorySnapshot) * 4 <= std::size_t{64} * 1024);
static_assert(sizeof(PerformanceHistory) + sizeof(api::PerformanceHistorySnapshot) * 4 <=
              std::size_t{128} * 1024);

} // namespace rpcmp::player
