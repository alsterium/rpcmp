#ifndef RPCMP_PLAYER_PERFORMANCE_HISTORY_HPP
#define RPCMP_PLAYER_PERFORMANCE_HISTORY_HPP

#include "rpcmp/contracts/performance_history_v2.hpp"

#include <cstddef>

namespace rpcmp::player {

// Core-only copied observations. Reads never poll or consume audio/engine work.
class PerformanceReader {
public:
  virtual ~PerformanceReader() = default;
  virtual void copy_to(contracts::v2::PerformanceHistorySnapshot& output) const noexcept = 0;
};

struct PerformanceChanges {
  const contracts::v2::PerformanceChange* data{};
  std::size_t count{};
  PerformanceChanges() = default;
  template <std::size_t N>
  PerformanceChanges(const std::array<contracts::v2::PerformanceChange, N>& source) noexcept
      : data(source.data()), count(N) {}
};

struct PerformanceCommit {
  std::uint64_t play_generation{};
  std::uint64_t through_frame{};
  contracts::v2::PerformanceChannels channels{};
  PerformanceChanges changes;
  std::uint64_t lost_before{};
  bool unknown_loss{};
};
enum class PerformanceCaptureResult : std::uint8_t { Applied, Stale, Invalid, Exhausted };

// Single Core owner; only already committed observations are accepted.
class PerformanceHistory final : public PerformanceReader {
public:
  PerformanceHistory() noexcept;
  [[nodiscard]] bool begin(std::uint64_t play_generation) noexcept;
  [[nodiscard]] std::uint64_t play_generation() const noexcept { return retained_.play_generation; }
  [[nodiscard]] PerformanceCaptureResult commit(const PerformanceCommit& input) noexcept;
  void copy_to(contracts::v2::PerformanceHistorySnapshot& output) const noexcept override;

private:
  contracts::v2::PerformanceHistorySnapshot retained_{};
  std::uint16_t first_{};
};

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_PERFORMANCE_HISTORY_HPP
