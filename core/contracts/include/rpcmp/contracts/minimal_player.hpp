#ifndef RPCMP_CONTRACTS_MINIMAL_PLAYER_HPP
#define RPCMP_CONTRACTS_MINIMAL_PLAYER_HPP

#include "rpcmp/contracts/catalog.hpp"

namespace rpcmp::contracts::minimal {
inline constexpr std::uint16_t kVersion = 6;
inline constexpr std::uint32_t kMaxPlaylists = 100;
inline constexpr std::uint32_t kMaxPlaylistTracks = 300;
inline constexpr std::uint32_t kMaxTracks = kMaxPlaylists * kMaxPlaylistTracks;
struct PlaylistId {
  std::uint32_t value{};
};
struct Playlist {
  PlaylistId id{};
  TrackId first{};
  std::uint32_t count{};
  CatalogText name{};
};
enum class State : std::uint8_t { Stopped, Playing, Paused, Advancing, Ended, Error };
enum class Error : std::uint8_t { None, Load, Renderer, Audio, Timeout, Reset };
enum class RepeatMode : std::uint8_t { Two, Three, Five, One };
enum class CommandKind : std::uint8_t { PlayTrack, Stop, PlayPause, CycleRepeat, Previous, Next };
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
  RepeatMode repeat{RepeatMode::Two};
  PlaylistId playlist{};
  TrackId last_played{};
  std::uint64_t elapsed_seconds{};
};
class TrackList {
public:
  virtual ~TrackList() = default;
  [[nodiscard]] virtual std::uint32_t count() const noexcept = 0;
  [[nodiscard]] virtual CatalogText title(TrackId id) const noexcept = 0;
  [[nodiscard]] virtual std::uint32_t playlist_count() const noexcept = 0;
  [[nodiscard]] virtual Playlist playlist(PlaylistId id) const noexcept = 0;
  [[nodiscard]] virtual PlaylistId playlist_for(TrackId id) const noexcept = 0;
};
class CommandSink {
public:
  virtual ~CommandSink() = default;
  virtual void submit(PlayerCommand command) = 0;
};
} // namespace rpcmp::contracts::minimal
#endif
