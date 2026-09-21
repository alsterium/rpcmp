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
  if (command.kind == api::CommandKind::CycleRepeat) {
    const auto mode =
        static_cast<api::RepeatMode>((static_cast<unsigned>(snapshot_.repeat) + 1) % 4);
    if (snapshot_.state == api::State::Playing || snapshot_.state == api::State::Paused) {
      const auto error = playback_.set_repeat(mode);
      if (error != api::Error::None) {
        finish(error);
        return;
      }
    }
    snapshot_.repeat = mode;
    ++snapshot_.sequence;
    return;
  }
  if (command.kind == api::CommandKind::TogglePause) {
    if (snapshot_.state != api::State::Playing && snapshot_.state != api::State::Paused)
      return;
    const bool paused = snapshot_.state == api::State::Playing;
    const auto error = playback_.set_paused(paused);
    if (error != api::Error::None)
      finish(error);
    else
      publish(paused ? api::State::Paused : api::State::Playing);
    return;
  }
  if (command.kind != api::CommandKind::PlayTrack && command.kind != api::CommandKind::Stop)
    return;
  if (command.kind == api::CommandKind::PlayTrack &&
      (command.track.value == 0 || command.track.value > list_.count() ||
       list_.playlist_for(command.track).value == 0))
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
  snapshot_.playlist = list_.playlist_for(command.track);
  start(command.track);
}
std::uint64_t Player::list_end() const noexcept {
  const auto list = list_.playlist(snapshot_.playlist);
  return list.first.value + list.count;
}
void Player::start(const contracts::TrackId track) {
  snapshot_.track = track;
  auto error = playback_.open(track);
  if (error == api::Error::None)
    error = playback_.set_repeat(snapshot_.repeat);
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
  publish(snapshot_.track.value + 1 < list_end() ||
                  (error == api::Error::None && snapshot_.repeat == api::RepeatMode::One)
              ? api::State::Advancing
          : error == api::Error::None ? api::State::Ended
                                      : api::State::Error,
          error);
}
void Player::service() {
  if (snapshot_.state == api::State::Advancing) {
    // One attempt per call: input can cancel between consecutive bad entries.
    const auto next =
        snapshot_.track.value +
        (snapshot_.error == api::Error::None && snapshot_.repeat == api::RepeatMode::One ? 0U : 1U);
    if (next >= list_end())
      publish(api::State::Ended);
    else
      start({next});
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
