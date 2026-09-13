#include "rpcmp/player/player_session.hpp"

#include <algorithm>
#include <limits>

namespace rpcmp::player {
namespace {
namespace api = contracts::v2;
static_assert(api::kCommandQueueCapacity == kTransportBatchCapacity);

std::optional<TransportIntent> translate(const api::PlayerCommand& command) {
  TransportIntent result;
  result.command_id = command.command_id;
  switch (command.kind) {
  case api::CommandKind::PlayTrack:
    result.kind = TransportIntentKind::PlayTrack;
    break;
  case api::CommandKind::LoadTrack:
    result.kind = TransportIntentKind::LoadTrack;
    break;
  case api::CommandKind::Play:
    result.kind = TransportIntentKind::Play;
    break;
  case api::CommandKind::Pause:
    result.kind = TransportIntentKind::Pause;
    break;
  case api::CommandKind::Resume:
    result.kind = TransportIntentKind::Resume;
    break;
  case api::CommandKind::TogglePause:
    result.kind = TransportIntentKind::TogglePause;
    break;
  case api::CommandKind::Stop:
    result.kind = TransportIntentKind::Stop;
    break;
  case api::CommandKind::SetPlaybackPolicy:
    result.kind = TransportIntentKind::SetPolicy;
    break;
  case api::CommandKind::NextTrack:
    result.kind = TransportIntentKind::NextTrack;
    break;
  case api::CommandKind::PreviousTrack:
    result.kind = TransportIntentKind::PreviousTrack;
    break;
  default:
    return std::nullopt;
  }
  const bool selecting =
      command.kind == api::CommandKind::PlayTrack || command.kind == api::CommandKind::LoadTrack;
  if (selecting != command.selection.has_value() ||
      (command.kind == api::CommandKind::SetPlaybackPolicy) != command.policy.has_value() ||
      (command.policy && !api::valid_playback_policy(*command.policy)))
    return std::nullopt;
  result.policy = command.policy;
  if (command.selection) {
    if (command.selection->library_generation.value == 0 || command.selection->track_id.value == 0)
      return std::nullopt;
    result.selection = {command.selection->library_generation, command.selection->track_id};
  }
  return result;
}

api::CommandReason reason(const TransportRejection rejection) noexcept {
  switch (rejection) {
  case TransportRejection::None:
    return api::CommandReason::None;
  case TransportRejection::Malformed:
    return api::CommandReason::MalformedRequest;
  case TransportRejection::InvalidState:
    return api::CommandReason::InvalidState;
  case TransportRejection::LibraryUnavailable:
    return api::CommandReason::LibraryUnavailable;
  case TransportRejection::StaleLibrary:
    return api::CommandReason::StaleLibrary;
  case TransportRejection::UnknownTrack:
    return api::CommandReason::UnknownTrack;
  case TransportRejection::UnsupportedPause:
  case TransportRejection::UnsupportedPolicy:
    return api::CommandReason::UnsupportedCapability;
  case TransportRejection::Busy:
    return api::CommandReason::ResourceBusy;
  case TransportRejection::TerminalFailure:
    return api::CommandReason::TerminalFailure;
  case TransportRejection::BatchTooLarge:
    return api::CommandReason::QueueFull;
  case TransportRejection::NoNextTrack:
    return api::CommandReason::NoNextTrack;
  case TransportRejection::NoPreviousTrack:
    return api::CommandReason::NoPreviousTrack;
  }
  return api::CommandReason::MalformedRequest;
}
} // namespace

PlayerSession::PlayerSession(const CatalogSession& catalog, PreparationPort& preparation,
                             AudioTransportPort& audio, const TransportTiming timing,
                             RandomSource* random, const PerformanceReader* performance,
                             const SettingsConfiguration settings) noexcept
    : catalog_(catalog), transport_(catalog, preparation, audio, timing, {}, random),
      publisher_(catalog, 0, performance), settings_(settings) {}

api::PlayerSnapshot PlayerSession::latest() const noexcept {
  auto result = publisher_.latest();
  result.capabilities.bits |= api::kTransportCommands;
  return result;
}

void PlayerSession::remember(const api::CommandResult& result) noexcept {
  history_[history_next_] = result;
  history_next_ = static_cast<std::uint16_t>((history_next_ + 1) % api::kCommandHistoryCapacity);
  if (history_count_ < api::kCommandHistoryCapacity)
    ++history_count_;
}

api::CommandResult PlayerSession::submit(const api::PlayerCommand& command) {
  api::CommandResult result{api::kSchemaVersion,           command.command_id,
                            api::CommandOutcome::Rejected, api::CommandReason::None,
                            publisher_.sequence(),         std::nullopt};
  if (command.schema_version != api::kSchemaVersion) {
    result.reason = api::CommandReason::UnsupportedSchema;
    return result;
  }
  if (command.command_id == 0) {
    result.reason = api::CommandReason::InvalidCommandId;
    return result;
  }
  for (std::uint16_t i = 0; i < history_count_; ++i) {
    const auto& previous = history_[i];
    if (previous.command_id == command.command_id) {
      result.outcome = api::CommandOutcome::Duplicate;
      result.reason = api::CommandReason::DuplicateCommandId;
      result.original = {previous.outcome, previous.reason, previous.observed_snapshot_sequence};
      return result;
    }
  }
  if (command.command_id <= command_high_water_) {
    result.reason = api::CommandReason::StaleCommandId;
    return result;
  }
  command_high_water_ = command.command_id;
  if (command.expected_snapshot_sequence &&
      *command.expected_snapshot_sequence != result.observed_snapshot_sequence) {
    result.reason = api::CommandReason::StaleSnapshotSequence;
  } else {
    const auto intent = translate(command);
    if (!intent) {
      result.reason = api::CommandReason::MalformedRequest;
    } else if (settings_.enabled() && !settings_.ready() &&
               intent->kind != TransportIntentKind::Stop) {
      result.reason = api::CommandReason::ResourceBusy;
    } else {
      auto candidate = projected_;
      const auto& context = synchronized_;
      result.reason = reason(project_transport(catalog_, context.pause_supported, *intent,
                                               candidate, &transport_.navigation_index_)
                                 .rejection);
      if (result.reason == api::CommandReason::None) {
        if (!stepped_ || context.catalog != catalog_.status()) {
          result.reason = api::CommandReason::ResourceBusy;
        } else if (queue_.count == api::kCommandQueueCapacity) {
          result.reason = api::CommandReason::QueueFull;
        } else {
          if (queue_.count == 0)
            admission_ = context;
          queue_.intents[queue_.count++] = *intent;
          projected_ = candidate;
          result.outcome = api::CommandOutcome::Accepted;
        }
      }
    }
  }
  remember(result);
  return result;
}

PlayerStepResult PlayerSession::step(const std::uint64_t now_us, const bool publish) {
  PlayerStepResult result;
  const auto previous_sequence = publisher_.sequence();
  const auto maximum = std::numeric_limits<std::uint64_t>::max();
  if (previous_sequence == maximum || (publish && previous_sequence == maximum - 1))
    transport_.fail(TransportFailure::ResourceExhausted, true);
  const auto execution = transport_.step(now_us, queue_, admission_);
  for (std::uint16_t i = 0; i < queue_.count; ++i) {
    const auto& intent = queue_.intents[i];
    if (intent.kind != TransportIntentKind::SetPolicy)
      continue;
    const bool applied = i < execution.count &&
                         execution.decisions[i].command_id == intent.command_id &&
                         execution.decisions[i].accepted();
    policy_commands_.last = {intent.command_id, transport_.state_.policy.revision,
                             applied ? api::PolicyCommandOutcome::Applied
                                     : api::PolicyCommandOutcome::Failed};
  }
  queue_.count = 0;
  admission_.reset();
  stepped_ = true;
  if (settings_.enabled()) {
    const auto& policy = transport_.state_.policy;
    if (settings_loaded_)
      settings_.observe(policy.desired, policy.revision, now_us);
    settings_.step(now_us);
    if (settings_.ready() && !settings_loaded_) {
      if (!transport_.restore_policy(settings_.policy()))
        settings_.unsupported_backend();
      settings_loaded_ = true;
      settings_.observe(policy.desired, policy.revision, now_us);
    }
  }
  projected_ = transport_.projection();
  if (publish) {
    const auto publication_time = std::max(now_us, publisher_.published_at_us());
    const auto settings = settings_.snapshot();
    const auto* settings_value = settings_.enabled() ? &settings : nullptr;
    const auto outcome = publisher_.publish(publication_time, transport_.snapshot(), settings_value,
                                            &policy_commands_);
    result.publication = outcome;
    result.published = outcome == PublicationResult::Ok;
    if (outcome != PublicationResult::Ok) {
      transport_.fail(outcome == PublicationResult::SequenceExhausted
                          ? TransportFailure::ResourceExhausted
                          : TransportFailure::Protocol,
                      true);
      projected_ = transport_.projection();
      if (outcome != PublicationResult::SequenceExhausted)
        result.published =
            publisher_.publish(publication_time, transport_.snapshot(), settings_value,
                               &policy_commands_) == PublicationResult::Ok;
    }
  }
  synchronized_ = transport_.admission_context();
  result.state = transport_.snapshot();
  return result;
}

} // namespace rpcmp::player
