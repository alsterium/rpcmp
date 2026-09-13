#ifndef RPCMP_PLAYER_PLAYER_SESSION_HPP
#define RPCMP_PLAYER_PLAYER_SESSION_HPP

#include "rpcmp/contracts/player_command_v2.hpp"
#include "rpcmp/player/playback_settings.hpp"
#include "rpcmp/player/snapshot_publisher.hpp"

namespace rpcmp::player {

struct PlayerStepResult {
  TransportSnapshot state{};
  std::optional<PublicationResult> publication;
  bool published{};
};

// One Core control owner; ports and catalog outlive all pending work.
// UI receives only CommandIngress / SnapshotSource / const CatalogReader.
class PlayerSession final : public contracts::v2::CommandIngress,
                            public contracts::v2::SnapshotSource {
public:
  PlayerSession(const CatalogSession& catalog, PreparationPort& preparation,
                AudioTransportPort& audio, TransportTiming timing, RandomSource* random = nullptr,
                const PerformanceReader* performance = nullptr,
                SettingsConfiguration settings = {}) noexcept;
  PlayerSession(const PlayerSession&) = delete;
  PlayerSession& operator=(const PlayerSession&) = delete;
  [[nodiscard]] contracts::v2::CommandResult
  submit(const contracts::v2::PlayerCommand& command) override;
  [[nodiscard]] contracts::v2::PlayerSnapshot latest() const noexcept override;
  [[nodiscard]] PlayerStepResult step(std::uint64_t now_us, bool publish = true);

private:
  void remember(const contracts::v2::CommandResult& result) noexcept;
  const CatalogSession& catalog_;
  TransportController transport_;
  SnapshotPublisher publisher_;
  PlaybackSettings settings_;
  contracts::v2::PolicyCommandObservation policy_commands_{};
  TransportProjection projected_{};
  TransportAdmissionContext synchronized_{};
  TransportBatch queue_{};
  std::optional<TransportAdmissionContext> admission_;
  std::array<contracts::v2::CommandResult, contracts::v2::kCommandHistoryCapacity> history_{};
  std::uint16_t history_count_{};
  std::uint16_t history_next_{};
  std::uint64_t command_high_water_{};
  bool stepped_{};
  bool settings_loaded_{};
};

} // namespace rpcmp::player

#endif // RPCMP_PLAYER_PLAYER_SESSION_HPP
