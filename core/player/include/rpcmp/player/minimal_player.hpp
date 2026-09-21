#ifndef RPCMP_PLAYER_MINIMAL_PLAYER_HPP
#define RPCMP_PLAYER_MINIMAL_PLAYER_HPP

#include "rpcmp/contracts/minimal_player.hpp"

namespace rpcmp::player::minimal {
namespace api = contracts::minimal;
class PlaybackPort {
public:
  virtual ~PlaybackPort() = default;
  virtual api::Error open(contracts::TrackId id) = 0;
  virtual bool stop() = 0;
  virtual api::Error set_paused(bool paused) = 0;
  virtual api::Error set_repeat(api::RepeatMode mode) = 0;
  virtual api::Error service(bool& ended) = 0;
  [[nodiscard]] virtual std::uint64_t elapsed_seconds() const noexcept = 0;
};
class Player final : public api::CommandSink {
public:
  Player(const api::TrackList& list, PlaybackPort& playback) : list_(list), playback_(playback) {}
  bool initialize();
  void submit(api::PlayerCommand command) override;
  void service();
  [[nodiscard]] api::PlayerSnapshot snapshot() const noexcept { return snapshot_; }

private:
  void publish(api::State state, api::Error error = api::Error::None);
  void start(contracts::TrackId track);
  void finish(api::Error error);
  void update_time();
  [[nodiscard]] std::uint64_t list_end() const noexcept;
  const api::TrackList& list_;
  PlaybackPort& playback_;
  api::PlayerSnapshot snapshot_{};
  bool initialized_{};
};
} // namespace rpcmp::player::minimal
#endif
