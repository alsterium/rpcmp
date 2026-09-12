#include "rpcmp/contracts/player_state_v2.hpp"

namespace rpcmp::contracts::v2 {
namespace {
bool valid_selection(const TrackSelection& selection) noexcept {
  return selection.library_generation.value != 0 && selection.track_id.value != 0;
}
bool valid_transport(const TransportState state) noexcept {
  return static_cast<std::uint8_t>(state) <= static_cast<std::uint8_t>(TransportState::Error);
}
} // namespace

bool valid_media_observation(const PlaybackMediaObservation& media) noexcept {
  if (media.play_generation == 0 || !valid_repeat_application(media.applied) ||
      media.loop_count_overflow == media.completed_loops.has_value() || media.gain > 240'000 ||
      media.ramp_elapsed > media.ramp_duration ||
      static_cast<std::uint8_t>(media.end) >
          static_cast<std::uint8_t>(PlaybackEndReason::LoopLimit))
    return false;
  switch (media.phase) {
  case PlaybackPhase::Steady:
    return media.ramp_duration == 0 && media.ramp_elapsed == 0 &&
           media.gain == (media.end == PlaybackEndReason::None ? 240'000U : 0U);
  case PlaybackPhase::Fading:
    return media.end == PlaybackEndReason::None && media.ramp_duration == 240'000;
  case PlaybackPhase::RestoringGain:
    return media.end == PlaybackEndReason::None && media.ramp_duration == 960;
  }
  return false;
}

bool valid_player_snapshot(const PlayerSnapshot& snapshot) noexcept {
  if (snapshot.schema_version != kSchemaVersion || snapshot.frame_rate != kFrameRate ||
      (snapshot.capabilities.bits & kTransportObservations) == 0 ||
      !valid_catalog_status(snapshot.library) || !valid_transport(snapshot.transport) ||
      !valid_transport(snapshot.projected))
    return false;
  if ((snapshot.transport == TransportState::Error) != snapshot.error.has_value() ||
      (snapshot.projected == TransportState::Error) != snapshot.error.has_value())
    return false;
  if (snapshot.error) {
    const auto code = static_cast<std::uint8_t>(snapshot.error->code);
    if (code < 1 || code > static_cast<std::uint8_t>(PlaybackErrorCode::ResourceExhausted))
      return false;
  }
  if (snapshot.track) {
    const auto& track = *snapshot.track;
    if (snapshot.library.phase != CatalogPhase::Ready || snapshot.play_generation == 0 ||
        track.item.track_id.value == 0 || track.item.album_id.value == 0 ||
        track.item.track_ordinal >= snapshot.library.track_count ||
        !valid_catalog_text(track.item.title) || !valid_catalog_text(track.album_name) ||
        track.album_name.length == 0 || snapshot.transport == TransportState::Empty)
      return false;
  } else if (snapshot.transport == TransportState::Playing ||
             snapshot.transport == TransportState::Paused ||
             snapshot.transport == TransportState::Ended || snapshot.position_frames != 0 ||
             snapshot.prepared) {
    return false;
  }
  if (snapshot.transport == TransportState::Stopped && snapshot.position_frames != 0)
    return false;
  if (snapshot.pending_intent) {
    const auto& intent = *snapshot.pending_intent;
    const bool selecting = intent.kind == TransportIntentKind::PlayTrack ||
                           intent.kind == TransportIntentKind::LoadTrack;
    if (intent.command_id == 0 ||
        static_cast<std::uint8_t>(intent.kind) >
            static_cast<std::uint8_t>(TransportIntentKind::Stop) ||
        selecting != intent.selection.has_value() ||
        (intent.selection && !valid_selection(*intent.selection)))
      return false;
  }
  if (snapshot.preparation) {
    const auto& pending = *snapshot.preparation;
    if (pending.operation_id == 0 || pending.play_generation == 0 ||
        !valid_selection(pending.selection))
      return false;
  }
  if (snapshot.audio_control) {
    const auto& pending = *snapshot.audio_control;
    if (pending.operation_id == 0 || pending.play_generation == 0 ||
        static_cast<std::uint8_t>(pending.kind) >
            static_cast<std::uint8_t>(AudioControlKind::SetPolicy) ||
        (pending.kind == AudioControlKind::Reset && pending.repeat) ||
        (pending.kind == AudioControlKind::SetPolicy && !pending.repeat) ||
        (pending.repeat && !valid_repeat_application(*pending.repeat)))
      return false;
  }
  const bool policy_observed = (snapshot.capabilities.bits & kPolicyObservations) != 0;
  if (policy_observed != snapshot.policy.has_value() ||
      ((snapshot.capabilities.bits & kRepeatControl) != 0 && !policy_observed))
    return false;
  if (snapshot.policy) {
    const auto& policy = *snapshot.policy;
    if (!valid_playback_policy(policy.desired) || policy.revision == 0 ||
        (policy.last_command_id && *policy.last_command_id == 0))
      return false;
    if (policy.media) {
      const auto& media = *policy.media;
      if (!snapshot.track || !valid_media_observation(media) ||
          media.play_generation != snapshot.play_generation ||
          media.position_frames != snapshot.position_frames ||
          media.applied.revision > policy.revision ||
          (media.applied.revision == policy.revision &&
           media.applied != repeat_application(policy.desired, policy.revision)))
        return false;
    }
  }
  return true;
}
} // namespace rpcmp::contracts::v2
