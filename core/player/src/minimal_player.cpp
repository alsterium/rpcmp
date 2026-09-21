#include "rpcmp/player/minimal_player.hpp"

namespace rpcmp::player::minimal {
void Player::publish(const api::State state, const api::Error error) {
  snapshot_.state = state;
  snapshot_.error = error;
  ++snapshot_.sequence;
}
bool Player::initialize() {
  if (initialized_)
    return false;
  initialized_ = true;
  const bool ok = playback_.stop();
  publish(ok ? api::State::Stopped : api::State::Error, ok ? api::Error::None : api::Error::Reset);
  return ok;
}
void Player::submit(const api::PlayerCommand command) {
  if (!initialized_ || snapshot_.error == api::Error::Reset)
    return;
  if (command.kind != api::CommandKind::PlayTrack && command.kind != api::CommandKind::Stop)
    return;
  if (command.kind == api::CommandKind::PlayTrack &&
      (command.track.value == 0 || command.track.value > list_.count()))
    return;
  if (!playback_.stop()) {
    publish(api::State::Error, api::Error::Reset);
    return;
  }
  if (command.kind == api::CommandKind::Stop) {
    publish(api::State::Stopped);
    return;
  }
  snapshot_.track = command.track;
  auto error = playback_.open(command.track);
  if (error != api::Error::None && !playback_.stop())
    error = api::Error::Reset;
  publish(error == api::Error::None ? api::State::Playing : api::State::Error, error);
}
void Player::service() {
  if (snapshot_.state != api::State::Playing)
    return;
  bool ended{};
  auto error = playback_.service(ended);
  if (error == api::Error::None && !ended)
    return;
  if (!playback_.stop())
    error = api::Error::Reset;
  publish(error == api::Error::None ? api::State::Ended : api::State::Error, error);
}
} // namespace rpcmp::player::minimal
