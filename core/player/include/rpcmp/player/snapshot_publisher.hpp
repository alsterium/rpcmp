#ifndef RPCMP_PLAYER_SNAPSHOT_PUBLISHER_HPP
#define RPCMP_PLAYER_SNAPSHOT_PUBLISHER_HPP

#include "rpcmp/contracts/player_state_v2.hpp"
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
                             std::uint64_t last_sequence = 0) noexcept;
  SnapshotPublisher(const SnapshotPublisher&) = delete;
  SnapshotPublisher& operator=(const SnapshotPublisher&) = delete;
  [[nodiscard]] PublicationResult publish(std::uint64_t now_us, const TransportSnapshot& state);
  [[nodiscard]] contracts::v2::PlayerSnapshot latest() const noexcept override { return latest_; }

private:
  [[nodiscard]] bool describe(PlaybackSelection selected,
                              contracts::v2::SelectedTrack& output) const;
  const contracts::CatalogReader& catalog_;
  contracts::v2::PlayerSnapshot latest_{};
};

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_SNAPSHOT_PUBLISHER_HPP
