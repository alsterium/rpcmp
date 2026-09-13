#ifndef RPCMP_PLAYER_SNAPSHOT_PUBLISHER_HPP
#define RPCMP_PLAYER_SNAPSHOT_PUBLISHER_HPP

#include "rpcmp/contracts/player_state_v2.hpp"
#include "rpcmp/player/performance_history.hpp"
#include "rpcmp/player/playback_transport.hpp"

namespace rpcmp::player {

enum class PublicationResult : std::uint8_t {
  Ok,
  ClockReversed,
  SequenceExhausted,
  CatalogChanged,
  InvalidObservation
};

// The Core owner publishes after stepping transport. UI gets only SnapshotSource.
class SnapshotPublisher final : public contracts::v2::SnapshotSource {
public:
  explicit SnapshotPublisher(const contracts::CatalogReader& catalog,
                             std::uint64_t last_sequence = 0,
                             const PerformanceReader* performance = nullptr) noexcept;
  SnapshotPublisher(const SnapshotPublisher&) = delete;
  SnapshotPublisher& operator=(const SnapshotPublisher&) = delete;
  [[nodiscard]] PublicationResult
  publish(std::uint64_t now_us, const TransportSnapshot& state,
          const contracts::v2::PlaybackSettingsObservation* settings = nullptr,
          const contracts::v2::PolicyCommandObservation* policy_commands = nullptr);
  [[nodiscard]] contracts::v2::PlayerSnapshot latest() const noexcept override { return latest_; }
  [[nodiscard]] std::uint64_t sequence() const noexcept { return latest_.sequence; }
  [[nodiscard]] std::uint64_t published_at_us() const noexcept { return latest_.published_at_us; }

private:
  [[nodiscard]] bool describe(PlaybackSelection selected,
                              contracts::v2::SelectedTrack& output) const;
  void describe_performance(contracts::v2::PlayerSnapshot& output) const noexcept;
  const contracts::CatalogReader& catalog_;
  const PerformanceReader* performance_{};
  contracts::v2::PlayerSnapshot latest_{};
};

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_SNAPSHOT_PUBLISHER_HPP
