#ifndef RPCMP_CONTRACTS_PLAYER_COMMAND_V2_HPP
#define RPCMP_CONTRACTS_PLAYER_COMMAND_V2_HPP

#include "rpcmp/contracts/player_state_v2.hpp"

namespace rpcmp::contracts::v2 {

inline constexpr std::uint16_t kCommandQueueCapacity = 32;
inline constexpr std::uint16_t kCommandHistoryCapacity = 64;
inline constexpr std::uint64_t kTransportCommands = 1ULL << 2U;

enum class CommandKind : std::uint8_t {
  PlayTrack = 0,
  LoadTrack = 1,
  Play = 2,
  Pause = 3,
  Resume = 4,
  TogglePause = 5,
  Stop = 6,
  SetPlaybackPolicy = 7
};
struct PlayerCommand {
  std::uint16_t schema_version{kSchemaVersion};
  std::uint64_t command_id{};
  std::optional<std::uint64_t> expected_snapshot_sequence;
  CommandKind kind{CommandKind::Stop};
  std::optional<TrackSelection> selection;
  std::optional<PlaybackPolicy> policy{std::nullopt};
};
enum class CommandOutcome : std::uint8_t { Accepted, Rejected, Duplicate };
enum class CommandReason : std::uint8_t {
  None = 0,
  UnsupportedSchema = 1,
  InvalidCommandId = 2,
  DuplicateCommandId = 3,
  StaleCommandId = 4,
  StaleSnapshotSequence = 5,
  MalformedRequest = 6,
  InvalidState = 7,
  LibraryUnavailable = 8,
  StaleLibrary = 9,
  UnknownTrack = 10,
  UnsupportedCapability = 11,
  ResourceBusy = 12,
  QueueFull = 13,
  TerminalFailure = 14
};
struct CommandAdmission {
  CommandOutcome outcome{CommandOutcome::Rejected};
  CommandReason reason{CommandReason::None};
  std::uint64_t observed_snapshot_sequence{};
};
struct CommandResult {
  std::uint16_t schema_version{kSchemaVersion};
  std::uint64_t command_id{};
  CommandOutcome outcome{CommandOutcome::Rejected};
  CommandReason reason{CommandReason::None};
  std::uint64_t observed_snapshot_sequence{};
  std::optional<CommandAdmission> original;
};

class CommandIngress {
public:
  virtual ~CommandIngress() = default;
  [[nodiscard]] virtual CommandResult submit(const PlayerCommand& command) = 0;
};

} // namespace rpcmp::contracts::v2

#endif // RPCMP_CONTRACTS_PLAYER_COMMAND_V2_HPP
