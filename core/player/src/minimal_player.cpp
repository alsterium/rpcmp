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
  snapshot_.skipped_track = {};
  snapshot_.skipped_error = api::Error::None;
  snapshot_.skipped_count = 0;
  start(command.track);
}
void Player::start(const contracts::TrackId track) {
  snapshot_.track = track;
  const auto error = playback_.open(track);
  if (error != api::Error::None)
    finish(error);
  else
    publish(api::State::Playing);
}
void Player::finish(api::Error error) {
  if (!playback_.stop())
    error = api::Error::Reset;
  if (error == api::Error::Reset) {
    publish(api::State::Error, error);
    return;
  }
  if (error != api::Error::None) {
    snapshot_.skipped_track = snapshot_.track;
    snapshot_.skipped_error = error;
    ++snapshot_.skipped_count;
  }
  publish(snapshot_.track.value < list_.count() ? api::State::Advancing
          : error == api::Error::None           ? api::State::Ended
                                                : api::State::Error,
          error);
}
void Player::service() {
  if (snapshot_.state == api::State::Advancing) {
    // One attempt per call: input can cancel between consecutive bad entries.
    start({snapshot_.track.value + 1});
    return;
  }
  if (snapshot_.state != api::State::Playing)
    return;
  bool ended{};
  auto error = playback_.service(ended);
  if (error == api::Error::None && !ended)
    return;
  finish(error);
}
} // namespace rpcmp::player::minimal
