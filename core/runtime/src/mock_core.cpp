#include "rpcmp/runtime/mock_core.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace rpcmp::runtime {
namespace {

using namespace contracts;

struct FixtureTrack {
  TrackSummary summary;
  std::optional<std::uint64_t> duration_ticks;
};

const std::array<FixtureTrack, 3>& fixture_tracks() {
  static const std::array<FixtureTrack, 3> tracks{{
      {{TrackId{1}, "Synthetic Dawn", "RPCMP", std::string{"M0 Fixtures"}, std::nullopt},
       std::nullopt},
      {{TrackId{2}, "Clockwork Night", "RPCMP", std::string{"M0 Fixtures"},
        std::string{"Deterministic Core"}},
       std::nullopt},
      {{TrackId{3}, "Finite Trace", "RPCMP", std::string{"M0 Fixtures"}, std::nullopt},
       std::uint64_t{5'000}},
  }};
  return tracks;
}

std::optional<std::size_t> find_track(const TrackId id) noexcept {
  const auto& tracks = fixture_tracks();
  for (std::size_t index = 0; index < tracks.size(); ++index) {
    if (tracks[index].summary.track_id == id) {
      return index;
    }
  }
  return std::nullopt;
}

} // namespace

bool operator==(const FakeDeviceEvent& left, const FakeDeviceEvent& right) noexcept {
  return left.at_media_tick == right.at_media_tick && left.channel_id == right.channel_id &&
         left.key_on == right.key_on && left.note == right.note;
}

MockCore::MockCore(IFakeDeviceSink* const device_sink, ISnapshotObserver* const snapshot_observer)
    : device_sink_(device_sink), snapshot_observer_(snapshot_observer) {
  latest_ = make_snapshot(0, 0);
}

CommandResult MockCore::submit(const PlayerCommand& command) {
  if (command.schema_version != kSchemaVersion) {
    return {command.command_id, CommandOutcome::Rejected, CommandReason::UnsupportedSchema,
            latest_.sequence};
  }
  if (command.command_id == 0) {
    return {command.command_id, CommandOutcome::Rejected, CommandReason::InvalidCommandId,
            latest_.sequence};
  }
  const auto duplicate =
      std::find_if(history_.begin(), history_.end(), [&command](const CommandResult& result) {
        return result.command_id == command.command_id;
      });
  if (duplicate != history_.end()) {
    return {command.command_id, CommandOutcome::Duplicate, CommandReason::DuplicateCommandId,
            latest_.sequence};
  }
  if (command.command_id <= command_high_water_) {
    return {command.command_id, CommandOutcome::Rejected, CommandReason::StaleCommandId,
            latest_.sequence};
  }
  command_high_water_ = command.command_id;
  if (command.expected_snapshot_sequence.has_value() &&
      *command.expected_snapshot_sequence != latest_.sequence) {
    return reject_and_remember(command.command_id, CommandReason::StaleSnapshotSequence);
  }

  auto candidate = projected_;
  const auto reason = transition(command, candidate);
  if (reason != CommandReason::None) {
    return reject_and_remember(command.command_id, reason);
  }
  if (queue_.size() >= kCommandQueueCapacity) {
    return reject_and_remember(command.command_id, CommandReason::QueueFull);
  }

  projected_ = candidate;
  queue_.push_back(command);
  CommandResult result{command.command_id, CommandOutcome::Accepted, CommandReason::None,
                       latest_.sequence};
  remember(result);
  return result;
}

AdvanceResult MockCore::advance_to(const std::uint64_t clock_tick) {
  if (clock_tick < clock_tick_) {
    return AdvanceResult::TimeReversed;
  }
  const auto old_bucket = clock_tick_ / kSnapshotCadenceTicks;
  const auto new_bucket = clock_tick / kSnapshotCadenceTicks;
  if (new_bucket - old_bucket > kMaxCatchUpSnapshots) {
    return AdvanceResult::CatchUpLimit;
  }
  const auto drain_result = drain_commands();
  if (drain_result != AdvanceResult::Ok) {
    return drain_result;
  }

  auto cursor = clock_tick_;
  for (auto bucket = old_bucket + 1; bucket <= new_bucket; ++bucket) {
    const auto boundary = bucket * kSnapshotCadenceTicks;
    const auto media_result = advance_media(boundary - cursor);
    if (media_result != AdvanceResult::Ok) {
      return media_result;
    }
    cursor = boundary;
    publish(boundary);
  }
  const auto media_result = advance_media(clock_tick - cursor);
  if (media_result != AdvanceResult::Ok) {
    return media_result;
  }
  clock_tick_ = clock_tick;
  projected_ = actual_;
  return AdvanceResult::Ok;
}

PlayerSnapshot MockCore::latest() const { return latest_; }

std::size_t MockCore::pending_command_count() const noexcept { return queue_.size(); }

bool MockCore::control_equal(const ControlState& left, const ControlState& right) noexcept {
  return left.library_open == right.library_open && left.track_index == right.track_index &&
         left.transport == right.transport && left.muted == right.muted && left.solo == right.solo;
}

void MockCore::clear_overrides(ControlState& state) noexcept {
  state.muted.fill(false);
  state.solo.fill(false);
}

CommandReason MockCore::transition(const PlayerCommand& command, ControlState& state) const {
  return std::visit(
      [&state](const auto& payload) -> CommandReason {
        using Payload = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<Payload, OpenLibrary>) {
          if (payload.library_ref.size() > kMaxLibraryReferenceBytes ||
              !is_valid_utf8(payload.library_ref)) {
            return CommandReason::MalformedRequest;
          }
          if (state.library_open) {
            return CommandReason::ResourceBusy;
          }
          if (payload.library_ref != "m0:library") {
            return CommandReason::UnknownLibrary;
          }
          state.library_open = true;
          state.transport = TransportState::Empty;
          return CommandReason::None;
        } else if constexpr (std::is_same_v<Payload, CloseLibrary>) {
          if (!state.library_open) {
            return CommandReason::InvalidState;
          }
          state = ControlState{};
          return CommandReason::None;
        } else if constexpr (std::is_same_v<Payload, LoadTrack>) {
          if (!state.library_open) {
            return CommandReason::InvalidState;
          }
          const auto index = find_track(payload.track_id);
          if (!index.has_value()) {
            return CommandReason::UnknownTrack;
          }
          state.track_index = index;
          state.transport = TransportState::Stopped;
          clear_overrides(state);
          return CommandReason::None;
        } else if constexpr (std::is_same_v<Payload, Play>) {
          if (!state.track_index.has_value()) {
            return CommandReason::InvalidState;
          }
          if (state.transport != TransportState::Stopped &&
              state.transport != TransportState::Paused &&
              state.transport != TransportState::Playing &&
              state.transport != TransportState::Ended) {
            return CommandReason::InvalidState;
          }
          state.transport = TransportState::Playing;
          return CommandReason::None;
        } else if constexpr (std::is_same_v<Payload, Pause>) {
          if (state.transport == TransportState::Paused) {
            return CommandReason::None;
          }
          if (state.transport != TransportState::Playing) {
            return CommandReason::InvalidState;
          }
          state.transport = TransportState::Paused;
          return CommandReason::None;
        } else if constexpr (std::is_same_v<Payload, Resume>) {
          if (state.transport == TransportState::Playing) {
            return CommandReason::None;
          }
          if (state.transport != TransportState::Paused) {
            return CommandReason::InvalidState;
          }
          state.transport = TransportState::Playing;
          return CommandReason::None;
        } else if constexpr (std::is_same_v<Payload, Stop>) {
          if (!state.track_index.has_value()) {
            return CommandReason::InvalidState;
          }
          if (state.transport != TransportState::Stopped &&
              state.transport != TransportState::Playing &&
              state.transport != TransportState::Paused &&
              state.transport != TransportState::Ended) {
            return CommandReason::InvalidState;
          }
          state.transport = TransportState::Stopped;
          return CommandReason::None;
        } else if constexpr (std::is_same_v<Payload, TogglePause>) {
          if (state.transport == TransportState::Playing) {
            state.transport = TransportState::Paused;
            return CommandReason::None;
          }
          if (state.transport == TransportState::Paused) {
            state.transport = TransportState::Playing;
            return CommandReason::None;
          }
          return CommandReason::InvalidState;
        } else if constexpr (std::is_same_v<Payload, NextTrack> ||
                             std::is_same_v<Payload, PreviousTrack>) {
          if (!state.library_open) {
            return CommandReason::InvalidState;
          }
          if (!state.track_index.has_value()) {
            state.track_index =
                std::is_same_v<Payload, NextTrack> ? 0U : fixture_tracks().size() - 1U;
          } else if constexpr (std::is_same_v<Payload, NextTrack>) {
            state.track_index = (*state.track_index + 1U) % fixture_tracks().size();
          } else {
            state.track_index =
                (*state.track_index + fixture_tracks().size() - 1U) % fixture_tracks().size();
          }
          state.transport = TransportState::Stopped;
          clear_overrides(state);
          return CommandReason::None;
        } else if constexpr (std::is_same_v<Payload, Seek>) {
          return CommandReason::UnsupportedCapability;
        } else if constexpr (std::is_same_v<Payload, SetChannelMute>) {
          if (!state.track_index.has_value()) {
            return CommandReason::InvalidState;
          }
          if (payload.channel_id.value >= kMockChannelCount) {
            return CommandReason::UnknownChannel;
          }
          state.muted[payload.channel_id.value] = payload.muted;
          return CommandReason::None;
        } else if constexpr (std::is_same_v<Payload, SetChannelSolo>) {
          if (!state.track_index.has_value()) {
            return CommandReason::InvalidState;
          }
          if (payload.channel_id.value >= kMockChannelCount) {
            return CommandReason::UnknownChannel;
          }
          state.solo[payload.channel_id.value] = payload.solo;
          return CommandReason::None;
        } else if constexpr (std::is_same_v<Payload, ClearChannelOverrides>) {
          if (!state.track_index.has_value()) {
            return CommandReason::InvalidState;
          }
          clear_overrides(state);
          return CommandReason::None;
        } else {
          return CommandReason::MalformedRequest;
        }
      },
      command.payload);
}

void MockCore::apply_command_effects(const PlayerCommand& command, const ControlState& before) {
  const bool track_changed = before.track_index != actual_.track_index;
  const bool became_empty = before.track_index.has_value() && !actual_.track_index.has_value();
  const bool stopped =
      actual_.transport == TransportState::Stopped && before.transport != TransportState::Stopped;
  const bool restarted =
      before.transport == TransportState::Ended && actual_.transport == TransportState::Playing;
  if (track_changed || became_empty || stopped || restarted) {
    position_ticks_ = 0;
    deactivate_observations(position_ticks_);
  }
  if (std::holds_alternative<Stop>(command.payload) &&
      actual_.transport == TransportState::Stopped) {
    position_ticks_ = 0;
    deactivate_observations(position_ticks_);
  }
}

AdvanceResult MockCore::drain_commands() {
  while (!queue_.empty()) {
    const auto command = queue_.front();
    queue_.pop_front();
    const auto before = actual_;
    const auto reason = transition(command, actual_);
    if (reason != CommandReason::None) {
      runtime_error_ = PlayerError{ErrorDomain::Internal, 1U, ErrorSeverity::Fatal, false,
                                   std::string{"projection mismatch"}};
      actual_.transport = TransportState::Error;
      projected_ = actual_;
      queue_.clear();
      return AdvanceResult::ProjectionMismatch;
    }
    apply_command_effects(command, before);
  }
  if (!control_equal(actual_, projected_)) {
    runtime_error_ = PlayerError{ErrorDomain::Internal, 1U, ErrorSeverity::Fatal, false,
                                 std::string{"projection mismatch"}};
    actual_.transport = TransportState::Error;
    projected_ = actual_;
    return AdvanceResult::ProjectionMismatch;
  }
  return AdvanceResult::Ok;
}

AdvanceResult MockCore::advance_media(const std::uint64_t delta_ticks) {
  if (actual_.transport != TransportState::Playing || delta_ticks == 0) {
    return AdvanceResult::Ok;
  }
  if (position_ticks_ > std::numeric_limits<std::uint64_t>::max() - delta_ticks) {
    return AdvanceResult::TickOverflow;
  }
  auto target = position_ticks_ + delta_ticks;
  std::optional<std::uint64_t> duration;
  if (actual_.track_index.has_value()) {
    duration = fixture_tracks()[*actual_.track_index].duration_ticks;
  }
  if (duration.has_value() && target >= *duration) {
    target = *duration;
  }
  const auto first_frame = position_ticks_ / kSnapshotCadenceTicks + 1U;
  const auto last_frame = target / kSnapshotCadenceTicks;
  for (auto frame = first_frame; frame <= last_frame; ++frame) {
    update_observations(frame * kSnapshotCadenceTicks);
  }
  position_ticks_ = target;
  if (duration.has_value() && position_ticks_ == *duration) {
    actual_.transport = TransportState::Ended;
    deactivate_observations(position_ticks_);
  }
  return AdvanceResult::Ok;
}

void MockCore::update_observations(const std::uint64_t media_tick) {
  const auto frame = media_tick / kSnapshotCadenceTicks;
  for (std::size_t index = 0; index < observations_.size(); ++index) {
    const auto phase = (frame + 3U * index) % 32U;
    Observation next;
    next.key_on = phase < 24U;
    if (next.key_on) {
      next.note = static_cast<std::uint8_t>(36U + 5U * index + ((frame / 8U) % 12U));
      next.activity = static_cast<std::uint8_t>(255U - 8U * phase);
    }
    if (next.key_on != observations_[index].key_on || next.note != observations_[index].note) {
      if (device_sink_ != nullptr) {
        device_sink_->write(
            {media_tick, ChannelId{static_cast<std::uint16_t>(index)}, next.key_on, next.note});
      }
    }
    observations_[index] = next;
  }
}

void MockCore::deactivate_observations(const std::uint64_t media_tick) {
  for (std::size_t index = 0; index < observations_.size(); ++index) {
    if (observations_[index].key_on || observations_[index].note.has_value()) {
      if (device_sink_ != nullptr) {
        device_sink_->write(
            {media_tick, ChannelId{static_cast<std::uint16_t>(index)}, false, std::nullopt});
      }
    }
    observations_[index] = {};
  }
}

void MockCore::publish(const std::uint64_t clock_tick) {
  latest_ = make_snapshot(clock_tick, latest_.sequence + 1U);
  if (snapshot_observer_ != nullptr) {
    snapshot_observer_->published(latest_);
  }
}

PlayerSnapshot MockCore::make_snapshot(const std::uint64_t clock_tick,
                                       const std::uint64_t sequence) const {
  PlayerSnapshot snapshot;
  snapshot.schema_version = kSchemaVersion;
  snapshot.sequence = sequence;
  snapshot.published_at_tick = clock_tick;
  snapshot.capabilities.bits = 0;
  snapshot.transport = actual_.transport;
  snapshot.position.position_ticks = position_ticks_;
  snapshot.position.tick_rate = kMockTickRate;
  snapshot.position.seekable = false;
  if (actual_.track_index.has_value()) {
    const auto& track = fixture_tracks()[*actual_.track_index];
    snapshot.track = track.summary;
    snapshot.position.duration_ticks = track.duration_ticks;
  }
  snapshot.devices.push_back(DeviceSummary{DeviceId{1}, DeviceType::Ym2151, 0,
                                           static_cast<std::uint16_t>(kMockChannelCount),
                                           DeviceOperationalState::Mock, CapabilitySet{0}});
  const bool any_solo =
      std::any_of(actual_.solo.begin(), actual_.solo.end(), [](const bool value) { return value; });
  snapshot.channels.reserve(kMockChannelCount);
  snapshot.visualization.recent_activity.reserve(kMockChannelCount);
  for (std::size_t index = 0; index < kMockChannelCount; ++index) {
    const bool enabled = !actual_.muted[index] && (!any_solo || actual_.solo[index]);
    const auto pan_cycle = index % 3U;
    const auto pan = static_cast<std::int8_t>(pan_cycle == 0U ? -96 : (pan_cycle == 1U ? 0 : 96));
    ChannelState channel;
    channel.channel_id = ChannelId{static_cast<std::uint16_t>(index)};
    channel.label = "FM " + std::to_string(index + 1U);
    channel.kind = ChannelKind::Fm;
    channel.enabled = enabled;
    channel.mute_capable = true;
    channel.muted = actual_.muted[index];
    channel.solo_capable = true;
    channel.solo = actual_.solo[index];
    channel.key_on = observations_[index].key_on;
    channel.note = observations_[index].note;
    channel.level = observations_[index].activity;
    channel.activity = observations_[index].activity;
    channel.pan = pan;
    channel.instrument_label = "Mock FM " + std::to_string(index + 1U);
    channel.device_id = DeviceId{1};
    channel.device_channel = static_cast<std::uint16_t>(index);
    snapshot.visualization.recent_activity.push_back(channel.activity);
    snapshot.channels.push_back(std::move(channel));
  }
  snapshot.error = runtime_error_;
  return snapshot;
}

CommandResult MockCore::reject_and_remember(const std::uint64_t command_id,
                                            const CommandReason reason) {
  CommandResult result{command_id, CommandOutcome::Rejected, reason, latest_.sequence};
  remember(result);
  return result;
}

void MockCore::remember(const CommandResult& result) {
  if (history_.size() == kCommandHistoryCapacity) {
    history_.pop_front();
  }
  history_.push_back(result);
}

} // namespace rpcmp::runtime
