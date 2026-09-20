#include "rpcmp/ui/player_ui.hpp"

#include <algorithm>
#include <limits>

namespace rpcmp::ui::v2 {
namespace {
using contracts::TransportState;
bool matching_page(const contracts::CatalogPageHeader& header,
                   const contracts::CatalogPageQuery& query, const std::uint32_t total) noexcept {
  return header.ok() && header.query.schema_version == query.schema_version &&
         header.query.generation == query.generation &&
         header.query.start_ordinal == query.start_ordinal && header.query.limit == query.limit &&
         header.total == total;
}
bool pending(const api::PlayerSnapshot& snapshot) noexcept {
  return snapshot.pending_intent || snapshot.preparation || snapshot.audio_control;
}
bool captured(const api::PerformanceAvailability availability) noexcept {
  return availability == api::PerformanceAvailability::Available ||
         availability == api::PerformanceAvailability::Degraded ||
         availability == api::PerformanceAvailability::Exhausted;
}
} // namespace

PlayerUi::PlayerUi(api::CommandIngress& commands, const contracts::CatalogReader& catalog,
                   const UiConfiguration configuration) noexcept
    : commands_(commands), catalog_(catalog), input_(configuration.bindings),
      focus_(configuration.focus), next_command_id_(configuration.first_command_id) {
  focus_valid_ = true;
  for (const auto& row : focus_)
    for (const auto target : row)
      focus_valid_ = focus_valid_ &&
                     static_cast<std::uint8_t>(target) <= static_cast<std::uint8_t>(Focus::Shuffle);
}

void PlayerUi::diagnose(const Diagnostic diagnostic) noexcept {
  view_.diagnostic = diagnostic;
  view_.rejection = api::CommandReason::None;
}

void PlayerUi::update(const api::PlayerSnapshot& snapshot) {
  const auto& history = snapshot.performance_history;
  if (!api::valid_player_snapshot(snapshot) ||
      (observed_ && (snapshot.sequence < view_.snapshot.sequence ||
                     snapshot.published_at_us < view_.snapshot.published_at_us ||
                     snapshot.play_generation < view_.snapshot.play_generation)) ||
      (history_seen_ && snapshot.play_generation == view_.snapshot.play_generation && history &&
       captured(history->availability) && history->next_sequence < history_next_)) {
    view_.valid_snapshot = false;
    diagnose(Diagnostic::InvalidSnapshot);
    return;
  }
  if (observed_ && snapshot.sequence == view_.snapshot.sequence)
    return;
  view_.valid_snapshot = true;
  const bool changed_catalog = !observed_ || snapshot.library != view_.snapshot.library;
  if (!observed_ || snapshot.play_generation != view_.snapshot.play_generation) {
    history_seen_ = false;
    history_next_ = 1;
    view_.tracker = {};
  }
  observed_ = true;
  view_.snapshot = snapshot;
  if (changed_catalog)
    reset_browser();
  observe_history();
  observe_commands();
  pump_policy();
}

void PlayerUi::input(const InputSample sample, const std::uint64_t now_us) {
  if (!input_.valid()) {
    diagnose(Diagnostic::InvalidInput);
    return;
  }
  dispatch(input_.sample(sample, now_us));
}

void PlayerUi::rebind(const InputBindings bindings) noexcept { input_.rebind(bindings); }

void PlayerUi::dispatch(const Action action) {
  switch (action) {
  case Action::None:
    break;
  case Action::Back:
    back();
    break;
  case Action::Previous:
  case Action::Next:
    neighbour(action == Action::Next);
    break;
  case Action::Confirm:
    confirm();
    break;
  case Action::Up:
  case Action::Down:
  case Action::Left:
  case Action::Right:
    move(action);
    break;
  }
}

bool PlayerUi::ready_to_send() {
  if (!observed_ || !view_.valid_snapshot) {
    diagnose(Diagnostic::InvalidSnapshot);
    return false;
  }
  if ((view_.snapshot.capabilities.bits & api::kTransportCommands) == 0) {
    diagnose(Diagnostic::Unsupported);
    return false;
  }
  if (view_.snapshot.error && view_.snapshot.error->terminal) {
    diagnose(Diagnostic::CommandFailed);
    return false;
  }
  return true;
}

std::optional<api::CommandResult> PlayerUi::send(api::PlayerCommand command) {
  if (!ready_to_send())
    return std::nullopt;
  if (next_command_id_ == 0) {
    diagnose(Diagnostic::CommandExhausted);
    return std::nullopt;
  }
  command.command_id = next_command_id_;
  next_command_id_ =
      next_command_id_ == std::numeric_limits<std::uint64_t>::max() ? 0 : next_command_id_ + 1;
  command.expected_snapshot_sequence = view_.snapshot.sequence;
  const auto result = commands_.submit(command);
  const bool accepted = result.outcome == api::CommandOutcome::Accepted;
  if (result.schema_version != api::kSchemaVersion || result.command_id != command.command_id ||
      result.original || (result.outcome != api::CommandOutcome::Rejected && !accepted) ||
      (accepted != (result.reason == api::CommandReason::None)) ||
      static_cast<std::uint8_t>(result.reason) >
          static_cast<std::uint8_t>(api::CommandReason::NoPreviousTrack) ||
      result.observed_snapshot_sequence < view_.snapshot.sequence ||
      (accepted && result.observed_snapshot_sequence != view_.snapshot.sequence)) {
    diagnose(Diagnostic::Protocol);
    return std::nullopt;
  }
  if (!accepted) {
    diagnose(Diagnostic::CommandRejected);
    view_.rejection = result.reason;
    return std::nullopt;
  }
  diagnose(Diagnostic::None);
  return result;
}

void PlayerUi::transport(const api::CommandKind kind,
                         const std::optional<api::TrackSelection> selection) {
  api::PlayerCommand command;
  command.kind = kind;
  command.selection = selection;
  const auto result = send(command);
  if (!result)
    return;
  const bool new_generation = kind != api::CommandKind::Stop ||
                              view_.snapshot.transport != TransportState::Stopped ||
                              pending_transport_.has_value();
  if (!new_generation && !pending(view_.snapshot) && view_.snapshot.silence_confirmed)
    return;
  pending_transport_ = {kind, result->observed_snapshot_sequence, view_.snapshot.play_generation,
                        new_generation};
  view_.transport_pending = true;
  if (kind == api::CommandKind::PlayTrack) {
    view_.main = last_monitor_;
    view_.focus = Focus::PlayPause;
  }
}

void PlayerUi::observe_commands() {
  const auto& snapshot = view_.snapshot;
  if (pending_transport_ && snapshot.sequence > pending_transport_->sequence) {
    const auto& wait = *pending_transport_;
    bool done = snapshot.error.has_value();
    if (wait.kind == api::CommandKind::Stop) {
      done = done || ((snapshot.play_generation > wait.generation || !wait.new_generation) &&
                      snapshot.transport == TransportState::Stopped && snapshot.silence_confirmed &&
                      !pending(snapshot));
    } else {
      done = done || snapshot.play_generation > wait.generation;
      if (!pending(snapshot)) {
        done = done || snapshot.transport == TransportState::Ended;
        if (wait.kind == api::CommandKind::Pause)
          done = done || snapshot.transport == TransportState::Paused;
        if (wait.kind == api::CommandKind::Resume || wait.kind == api::CommandKind::Play)
          done = done || snapshot.transport == TransportState::Playing;
      }
    }
    if (done) {
      pending_transport_.reset();
      view_.transport_pending = false;
      if (snapshot.error)
        diagnose(Diagnostic::CommandFailed);
    }
  }
  if (!pending_policy_ || snapshot.sequence <= policy_observed_sequence_)
    return;
  if (!snapshot.policy_commands) {
    diagnose(Diagnostic::Protocol);
    pop_policy();
    return;
  }
  const auto& result = snapshot.policy_commands->last;
  if (!result || result->command_id < *pending_policy_)
    return;
  if (result->command_id != *pending_policy_)
    diagnose(Diagnostic::Protocol);
  else if (result->outcome == api::PolicyCommandOutcome::Failed)
    diagnose(Diagnostic::CommandFailed);
  pop_policy();
}

void PlayerUi::play_pause() {
  if (!ready_to_send())
    return;
  const auto& snapshot = view_.snapshot;
  if (!snapshot.track || snapshot.error) {
    diagnose(Diagnostic::NoSelection);
    return;
  }
  if (pending_transport_ || pending(snapshot)) {
    diagnose(Diagnostic::Busy);
    return;
  }
  if ((snapshot.transport == TransportState::Playing ||
       snapshot.transport == TransportState::Paused) &&
      (snapshot.capabilities.bits & api::kStatePreservingPause) == 0) {
    diagnose(Diagnostic::Unsupported);
    return;
  }
  switch (snapshot.transport) {
  case TransportState::Playing:
    transport(api::CommandKind::Pause);
    break;
  case TransportState::Paused:
    transport(api::CommandKind::Resume);
    break;
  case TransportState::Stopped:
  case TransportState::Ended:
    transport(api::CommandKind::Play);
    break;
  default:
    diagnose(Diagnostic::Busy);
    break;
  }
}

void PlayerUi::neighbour(const bool next) {
  if (!ready_to_send())
    return;
  const auto& navigation = view_.snapshot.navigation;
  if (!navigation) {
    diagnose(Diagnostic::Unsupported);
    return;
  }
  if (!(next ? navigation->can_next : navigation->can_previous)) {
    diagnose(Diagnostic::NoSelection);
    return;
  }
  transport(next ? api::CommandKind::NextTrack : api::CommandKind::PreviousTrack);
}

void PlayerUi::stop() {
  if (pending_transport_ && pending_transport_->kind == api::CommandKind::Stop)
    return;
  if (view_.snapshot.transport == TransportState::Empty && !pending_transport_) {
    diagnose(Diagnostic::NoSelection);
    return;
  }
  transport(api::CommandKind::Stop);
}

void PlayerUi::back() {
  if (!view_.valid_snapshot) {
    diagnose(Diagnostic::InvalidSnapshot);
    return;
  }
  if (view_.focus == Focus::Main && view_.main == View::Library) {
    if (view_.browser.level == BrowseLevel::Tracks) {
      const auto ordinal = view_.browser.album ? view_.browser.album->display_ordinal : 0;
      view_.browser.level = BrowseLevel::Albums;
      view_.browser.album.reset();
      view_.browser.cursor = ordinal;
      static_cast<void>(load_page());
    }
    return;
  }
  const auto state = view_.snapshot.transport;
  if (state != TransportState::Error &&
      (state == TransportState::Playing || state == TransportState::Paused ||
       state == TransportState::Loading || state == TransportState::Ended || pending_transport_ ||
       pending(view_.snapshot)))
    stop();
}

void PlayerUi::switch_view() {
  if (view_.main != View::Library)
    last_monitor_ = view_.main;
  view_.main = view_.main == View::Tracker    ? View::Keyboard
               : view_.main == View::Keyboard ? View::Library
                                              : View::Tracker;
  if (view_.main == View::Library && !view_.browser.valid)
    static_cast<void>(load_page());
}

void PlayerUi::move(const Action direction) {
  if (!focus_valid_) {
    diagnose(Diagnostic::InvalidFocus);
    return;
  }
  auto& browser = view_.browser;
  if (view_.focus == Focus::Main && view_.main == View::Library &&
      (direction == Action::Up || direction == Action::Down)) {
    if (!browser.valid)
      return;
    const auto& header =
        browser.level == BrowseLevel::Albums ? browser.albums.header : browser.tracks.header;
    if (direction == Action::Up && browser.cursor != 0)
      --browser.cursor;
    else if (direction == Action::Down && browser.cursor + 1 < header.total)
      ++browser.cursor;
    if (browser.cursor < header.query.start_ordinal ||
        browser.cursor >= header.query.start_ordinal + header.count)
      static_cast<void>(load_page());
    return;
  }
  const auto target =
      focus_[static_cast<std::size_t>(view_.focus)]
            [static_cast<std::size_t>(direction) - static_cast<std::size_t>(Action::Up)];
  view_.focus = target;
}

void PlayerUi::confirm() {
  switch (view_.focus) {
  case Focus::Main: {
    if (view_.main != View::Library) {
      play_pause();
      break;
    }
    auto& browser = view_.browser;
    if (!view_.valid_snapshot || !browser.valid || catalog_.status() != view_.snapshot.library) {
      browser.valid = false;
      diagnose(Diagnostic::CatalogFailure);
      break;
    }
    const auto& header =
        browser.level == BrowseLevel::Albums ? browser.albums.header : browser.tracks.header;
    if (browser.cursor < header.query.start_ordinal ||
        browser.cursor >= header.query.start_ordinal + header.count) {
      diagnose(Diagnostic::NoSelection);
      break;
    }
    const auto index = browser.cursor - header.query.start_ordinal;
    if (browser.level == BrowseLevel::Albums) {
      browser.album = browser.albums.items[index];
      browser.level = BrowseLevel::Tracks;
      browser.cursor = 0;
      static_cast<void>(load_page());
    } else {
      transport(api::CommandKind::PlayTrack,
                api::TrackSelection{browser.generation, browser.tracks.items[index].track_id});
    }
    break;
  }
  case Focus::Previous:
    neighbour(false);
    break;
  case Focus::PlayPause:
    play_pause();
    break;
  case Focus::Stop:
    stop();
    break;
  case Focus::Next:
    neighbour(true);
    break;
  case Focus::ViewSwitch:
    switch_view();
    break;
  case Focus::Repeat:
    enqueue_policy(PolicyAction::RepeatStep);
    break;
  case Focus::Shuffle:
    enqueue_policy(PolicyAction::ToggleShuffle);
    break;
  }
}

void PlayerUi::enqueue_policy(const PolicyAction action) {
  if (!ready_to_send())
    return;
  const auto needed = api::kRepeatControl | api::kPolicyCommandResults;
  if ((view_.snapshot.capabilities.bits & needed) != needed ||
      (action == PolicyAction::ToggleShuffle && !view_.snapshot.navigation)) {
    diagnose(Diagnostic::Unsupported);
    return;
  }
  if (view_.queued_policy_actions == policy_queue_.size()) {
    diagnose(Diagnostic::PolicyQueueFull);
    return;
  }
  policy_queue_[view_.queued_policy_actions++] = action;
  pump_policy();
}

void PlayerUi::pop_policy() noexcept {
  pending_policy_.reset();
  for (std::uint8_t i = 1; i < view_.queued_policy_actions; ++i)
    policy_queue_[i - 1] = policy_queue_[i];
  --view_.queued_policy_actions;
}

void PlayerUi::pump_policy() {
  if (pending_policy_ || !view_.valid_snapshot)
    return;
  while (view_.queued_policy_actions != 0) {
    const auto needed = api::kRepeatControl | api::kPolicyCommandResults;
    if (!view_.snapshot.policy || (view_.snapshot.capabilities.bits & needed) != needed ||
        (view_.snapshot.error && view_.snapshot.error->terminal)) {
      diagnose(Diagnostic::CommandFailed);
      view_.queued_policy_actions = 0;
      return;
    }
    auto policy = view_.snapshot.policy->desired;
    if (policy_queue_[0] == PolicyAction::ToggleShuffle) {
      policy.order = policy.order == api::PlaybackOrder::AlbumOrder
                         ? api::PlaybackOrder::ShuffleLibrary
                         : api::PlaybackOrder::AlbumOrder;
    } else if (policy.repeat == api::RepeatMode::Default ||
               (policy.repeat == api::RepeatMode::Counted && policy.count == 2U)) {
      policy.repeat = api::RepeatMode::Counted;
      policy.count = 3;
    } else if (policy.repeat == api::RepeatMode::Counted && policy.count == 3U) {
      policy.count = 5;
    } else if (policy.repeat == api::RepeatMode::Counted && policy.count == 5U) {
      policy.repeat = api::RepeatMode::RepeatOne;
      policy.count.reset();
    } else {
      policy.repeat = api::RepeatMode::Default;
      policy.count.reset();
    }
    api::PlayerCommand command;
    command.kind = api::CommandKind::SetPlaybackPolicy;
    command.policy = policy;
    const auto previous_diagnostic = view_.diagnostic;
    const auto previous_rejection = view_.rejection;
    const auto result = send(command);
    if (result) {
      pending_policy_ = result->command_id;
      policy_observed_sequence_ = result->observed_snapshot_sequence;
      if (previous_diagnostic != Diagnostic::None) {
        view_.diagnostic = previous_diagnostic;
        view_.rejection = previous_rejection;
      }
      return;
    }
    pop_policy();
  }
}

void PlayerUi::reset_browser() {
  view_.browser = {};
  view_.browser.generation = view_.snapshot.library.generation;
  if (view_.main == View::Library)
    static_cast<void>(load_page());
}

bool PlayerUi::load_page() {
  auto& browser = view_.browser;
  browser.valid = false;
  const auto& status = view_.snapshot.library;
  if (!view_.valid_snapshot || status.phase != contracts::CatalogPhase::Ready ||
      browser.generation != status.generation || catalog_.status() != status) {
    diagnose(Diagnostic::CatalogFailure);
    return false;
  }
  const contracts::CatalogPageQuery query{contracts::kCatalogSchemaVersion, browser.generation,
                                          browser.cursor / contracts::kCatalogPageCapacity *
                                              contracts::kCatalogPageCapacity,
                                          contracts::kCatalogPageCapacity};
  if (browser.level == BrowseLevel::Albums) {
    browser.albums = catalog_.albums(query);
    browser.valid = contracts::valid_catalog_page(browser.albums) &&
                    matching_page(browser.albums.header, query, status.album_count);
  } else if (browser.album) {
    browser.tracks = catalog_.tracks(browser.album->album_id, query);
    browser.valid = contracts::valid_catalog_page(browser.tracks) &&
                    matching_page(browser.tracks.header, query, browser.album->track_count) &&
                    browser.tracks.album_id == browser.album->album_id;
  }
  if (!browser.valid)
    diagnose(Diagnostic::CatalogFailure);
  return browser.valid;
}

void PlayerUi::observe_history() {
  auto& tracker = view_.tracker;
  tracker.count = 0;
  const auto& history = view_.snapshot.performance_history;
  tracker.supported = history.has_value();
  if (!history)
    return;
  tracker.availability = history->availability;
  tracker.retained_earlier = history->retention_lost;
  if (!captured(history->availability))
    return;
  tracker.gap =
      tracker.gap || history->capture_lost ||
      (history_seen_ && history->count != 0 && history->events[0].sequence > history_next_);
  history_next_ = history->next_sequence;
  history_seen_ = true;
  std::uint64_t previous = 0;
  std::size_t row = 0;
  for (std::uint16_t i = 0; i < history->count; ++i) {
    const auto& event = history->events[i];
    const auto channel = event.change.channel.channel_id;
    if (tracker.count == 0 || event.sequence != previous + 1 ||
        tracker.rows[row].at_frame != event.change.at_frame || tracker.rows[row].cells[channel]) {
      if (tracker.count != 0)
        row = (row + 1) % kTrackerRows;
      if (tracker.count < kTrackerRows)
        ++tracker.count;
      tracker.rows[row] = {event.sequence, event.change.at_frame, {}};
    }
    tracker.rows[row].cells[channel] = event.change;
    previous = event.sequence;
  }
  // Retain the newest rows in place, then restore chronological order once.
  if (tracker.count == kTrackerRows)
    std::rotate(tracker.rows.begin(),
                tracker.rows.begin() + static_cast<std::ptrdiff_t>((row + 1) % kTrackerRows),
                tracker.rows.end());
}

} // namespace rpcmp::ui::v2
