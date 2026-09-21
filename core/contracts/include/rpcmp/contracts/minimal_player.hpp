#ifndef RPCMP_CONTRACTS_MINIMAL_PLAYER_HPP
#define RPCMP_CONTRACTS_MINIMAL_PLAYER_HPP

#include "rpcmp/contracts/catalog.hpp"

namespace rpcmp::contracts::minimal {
inline constexpr std::uint16_t kVersion = 3;
enum class State : std::uint8_t { Stopped, Playing, Paused, Advancing, Ended, Error };
enum class Error : std::uint8_t { None, Load, Renderer, Audio, Timeout, Reset };
enum class CommandKind : std::uint8_t { PlayTrack, Stop, TogglePause };
struct PlayerCommand {
  CommandKind kind{CommandKind::Stop};
  TrackId track{};
};
struct PlayerSnapshot {
  std::uint16_t version{kVersion};
  std::uint64_t sequence{};
  State state{State::Stopped};
  TrackId track{};
  Error error{Error::None};
  TrackId skipped_track{};
  Error skipped_error{Error::None};
  std::uint32_t skipped_count{};
};
class TrackList {
public:
  virtual ~TrackList() = default;
  [[nodiscard]] virtual std::uint32_t count() const noexcept = 0;
  [[nodiscard]] virtual CatalogText title(TrackId id) const noexcept = 0;
};
class CommandSink {
public:
  virtual ~CommandSink() = default;
  virtual void submit(PlayerCommand command) = 0;
};
} // namespace rpcmp::contracts::minimal
#endif
