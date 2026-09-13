#include "rpcmp/player/snapshot_publisher.hpp"

#include <limits>

namespace rpcmp::player {
namespace {
namespace api = contracts::v2;

std::optional<api::TransportIntentKind> intent_kind(const TransportIntentKind kind) noexcept {
  switch (kind) {
  case TransportIntentKind::PlayTrack:
    return api::TransportIntentKind::PlayTrack;
  case TransportIntentKind::LoadTrack:
    return api::TransportIntentKind::LoadTrack;
  case TransportIntentKind::Play:
    return api::TransportIntentKind::Play;
  case TransportIntentKind::Pause:
    return api::TransportIntentKind::Pause;
  case TransportIntentKind::Resume:
    return api::TransportIntentKind::Resume;
  case TransportIntentKind::TogglePause:
    return api::TransportIntentKind::TogglePause;
  case TransportIntentKind::Stop:
    return api::TransportIntentKind::Stop;
  case TransportIntentKind::SetPolicy:
    return std::nullopt; // Policy command identity is published with desired settings.
  case TransportIntentKind::NextTrack:
    return api::TransportIntentKind::NextTrack;
  case TransportIntentKind::PreviousTrack:
    return api::TransportIntentKind::PreviousTrack;
  }
  return std::nullopt;
}
std::optional<api::AudioControlKind> control_kind(const AudioControlKind kind) noexcept {
  switch (kind) {
  case AudioControlKind::Reset:
    return api::AudioControlKind::Reset;
  case AudioControlKind::Start:
    return api::AudioControlKind::Start;
  case AudioControlKind::Pause:
    return api::AudioControlKind::Pause;
  case AudioControlKind::Resume:
    return api::AudioControlKind::Resume;
  case AudioControlKind::SetPolicy:
    return api::AudioControlKind::SetPolicy;
  }
  return std::nullopt;
}
std::optional<api::PlaybackErrorCode> error_code(const TransportFailure failure) noexcept {
  switch (failure) {
  case TransportFailure::None:
    return std::nullopt;
  case TransportFailure::Library:
    return api::PlaybackErrorCode::Library;
  case TransportFailure::Preparation:
    return api::PlaybackErrorCode::Preparation;
  case TransportFailure::PreparationTimeout:
    return api::PlaybackErrorCode::PreparationTimeout;
  case TransportFailure::AudioControl:
    return api::PlaybackErrorCode::AudioControl;
  case TransportFailure::DeviceFault:
    return api::PlaybackErrorCode::DeviceFault;
  case TransportFailure::AudioTimeout:
    return api::PlaybackErrorCode::AudioTimeout;
  case TransportFailure::ResetFailed:
    return api::PlaybackErrorCode::ResetFailed;
  case TransportFailure::Protocol:
    return api::PlaybackErrorCode::Protocol;
  case TransportFailure::Clock:
    return api::PlaybackErrorCode::Clock;
  case TransportFailure::InvalidConfiguration:
    return api::PlaybackErrorCode::InvalidConfiguration;
  case TransportFailure::ResourceExhausted:
    return api::PlaybackErrorCode::ResourceExhausted;
  }
  return std::nullopt;
}
api::TrackSelection selection(const PlaybackSelection value) noexcept {
  return {value.library_generation, value.track_id};
}
} // namespace

SnapshotPublisher::SnapshotPublisher(const contracts::CatalogReader& catalog,
                                     const std::uint64_t last_sequence,
                                     const PerformanceReader* performance) noexcept
    : catalog_(catalog), performance_(performance) {
  latest_.sequence = last_sequence;
}

bool SnapshotPublisher::describe(const PlaybackSelection selected,
                                 api::SelectedTrack& output) const {
  if (latest_.track && latest_.library.generation == selected.library_generation &&
      latest_.track->item.track_id == selected.track_id) {
    output = *latest_.track;
    return true;
  }
  const auto status = catalog_.status();
  std::uint32_t accounted_tracks = 0;
  for (std::uint32_t album_start = 0; album_start < status.album_count;
       album_start += contracts::kCatalogPageCapacity) {
    const auto albums = catalog_.albums({1, selected.library_generation, album_start});
    if (!contracts::valid_catalog_page(albums) || !albums.header.ok())
      return false;
    for (std::uint16_t i = 0; i < albums.header.count; ++i) {
      const auto& album = albums.items[i];
      if (album.track_count > status.track_count - accounted_tracks)
        return false;
      accounted_tracks += album.track_count;
      for (std::uint32_t start = 0; start < album.track_count;
           start += contracts::kCatalogPageCapacity) {
        const auto tracks =
            catalog_.tracks(album.album_id, {1, selected.library_generation, start});
        if (!contracts::valid_catalog_page(tracks) || !tracks.header.ok())
          return false;
        for (std::uint16_t j = 0; j < tracks.header.count; ++j) {
          if (tracks.items[j].track_id == selected.track_id) {
            output = {tracks.items[j], album.name};
            return true;
          }
        }
      }
    }
  }
  return false;
}

void SnapshotPublisher::describe_performance(api::PlayerSnapshot& output) const noexcept {
  if (performance_ == nullptr)
    return;
  output.capabilities.bits |= api::kPerformanceHistory;
  auto& history = output.performance_history.emplace();
  auto availability = api::PerformanceAvailability::Waiting;
  if (output.track && (output.transport == contracts::TransportState::Playing ||
                       output.transport == contracts::TransportState::Paused ||
                       output.transport == contracts::TransportState::Ended)) {
    performance_->copy_to(history);
    const bool valid_history = api::valid_performance_history(history);
    if (history.play_generation == output.play_generation &&
        history.observed_through_frame == output.position_frames && valid_history)
      return;
    if (history.play_generation > output.play_generation ||
        (history.play_generation == output.play_generation &&
         (history.observed_through_frame > output.position_frames || !valid_history)))
      availability = api::PerformanceAvailability::Invalid;
  }
  history = {};
  history.play_generation = output.play_generation;
  history.observed_through_frame = output.position_frames;
  history.channels = api::unknown_performance_channels();
  history.availability = availability;
}

PublicationResult SnapshotPublisher::publish(const std::uint64_t now_us,
                                             const TransportSnapshot& state) {
  if (now_us < latest_.published_at_us)
    return PublicationResult::ClockReversed;
  if (latest_.sequence == std::numeric_limits<std::uint64_t>::max())
    return PublicationResult::SequenceExhausted;
  if (state.catalog != catalog_.status())
    return PublicationResult::CatalogChanged;
  if (!contracts::valid_catalog_status(state.catalog))
    return PublicationResult::InvalidObservation;
  api::PlayerSnapshot next;
  next.sequence = latest_.sequence + 1;
  next.published_at_us = now_us;
  if (state.pause_supported)
    next.capabilities.bits |= api::kStatePreservingPause;
  next.capabilities.bits |= api::kPolicyObservations;
  if (state.policy_supported)
    next.capabilities.bits |= api::kRepeatControl;
  next.policy = state.policy;
  next.navigation = state.navigation;
  if (state.navigation)
    next.capabilities.bits |= api::kPlaybackNavigation;
  next.library = state.catalog;
  next.transport = state.transport;
  next.projected = state.projected;
  next.play_generation = state.play_generation;
  next.position_frames = state.media_frame;
  next.prepared = state.prepared;
  next.silence_confirmed = state.silence_confirmed;
  if (state.selection) {
    api::SelectedTrack track;
    if (state.selection->library_generation != state.catalog.generation ||
        !describe(*state.selection, track))
      return PublicationResult::InvalidObservation;
    next.track = track;
  }
  if (state.pending_intent) {
    const auto& intent = *state.pending_intent;
    const auto kind = intent_kind(intent.kind);
    if (!kind)
      return PublicationResult::InvalidObservation;
    api::PendingIntent pending{intent.command_id, *kind, std::nullopt};
    if (intent.kind == TransportIntentKind::PlayTrack ||
        intent.kind == TransportIntentKind::LoadTrack ||
        intent.kind == TransportIntentKind::NextTrack ||
        intent.kind == TransportIntentKind::PreviousTrack)
      pending.selection = selection(intent.selection);
    else if (intent.selection.library_generation.value != 0 || intent.selection.track_id.value != 0)
      return PublicationResult::InvalidObservation;
    next.pending_intent = pending;
  }
  if (state.preparation) {
    const auto& pending = *state.preparation;
    next.preparation = {pending.request.operation_id, pending.request.play_generation,
                        selection(pending.request.selection), pending.cancel_requested};
  }
  if (state.audio_control) {
    const auto& pending = state.audio_control->request;
    const auto kind = control_kind(pending.kind);
    if (!kind)
      return PublicationResult::InvalidObservation;
    next.audio_control = {pending.operation_id, pending.play_generation, *kind, pending.repeat};
  }
  const auto error = error_code(state.failure);
  if (error)
    next.error = {*error, state.terminal};
  else if (state.failure != TransportFailure::None || state.terminal)
    return PublicationResult::InvalidObservation;
  describe_performance(next);
  if (!api::valid_player_snapshot(next))
    return PublicationResult::InvalidObservation;
  latest_ = next;
  return PublicationResult::Ok;
}
} // namespace rpcmp::player
