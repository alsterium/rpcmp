#ifndef RPCMP_CONTRACTS_PLAYER_COMMAND_HPP
#define RPCMP_CONTRACTS_PLAYER_COMMAND_HPP

#include "rpcmp/contracts/types.hpp"

namespace rpcmp::contracts {

struct OpenLibrary {
  std::string library_ref;
};
struct CloseLibrary {};
struct LoadTrack {
  TrackId track_id{};
};
struct Play {};
struct Pause {};
struct Resume {};
struct Stop {};
struct TogglePause {};
struct NextTrack {};
struct PreviousTrack {};
struct Seek {
  std::uint64_t position_ticks{};
};
struct SetChannelMute {
  ChannelId channel_id{};
  bool muted{};
};
struct SetChannelSolo {
  ChannelId channel_id{};
  bool solo{};
};
struct ClearChannelOverrides {};

using Command = std::variant<OpenLibrary, CloseLibrary, LoadTrack, Play, Pause, Resume, Stop,
                             TogglePause, NextTrack, PreviousTrack, Seek, SetChannelMute,
                             SetChannelSolo, ClearChannelOverrides>;

struct PlayerCommand {
  std::uint16_t schema_version{kSchemaVersion};
  std::uint64_t command_id{};
  std::optional<std::uint64_t> expected_snapshot_sequence;
  Command payload{Stop{}};
};

enum class CommandOutcome : std::uint8_t { Accepted, Rejected, Duplicate };
enum class CommandReason : std::uint8_t {
  None,
  UnsupportedSchema,
  InvalidCommandId,
  DuplicateCommandId,
  StaleCommandId,
  StaleSnapshotSequence,
  MalformedRequest,
  InvalidState,
  UnknownLibrary,
  UnknownTrack,
  UnknownChannel,
  UnsupportedCapability,
  QueueFull,
  ResourceBusy,
};

struct CommandResult {
  std::uint64_t command_id{};
  CommandOutcome outcome{CommandOutcome::Rejected};
  CommandReason reason{CommandReason::None};
  std::uint64_t observed_snapshot_sequence{};
};

} // namespace rpcmp::contracts

#endif // RPCMP_CONTRACTS_PLAYER_COMMAND_HPP
