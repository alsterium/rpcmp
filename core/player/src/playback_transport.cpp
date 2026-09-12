#include "rpcmp/player/playback_transport.hpp"

#include <limits>

namespace rpcmp::player {
namespace {
using contracts::CatalogPhase;
using contracts::TransportState;

bool playback_target(const TransportState target) {
  return target == TransportState::Playing || target == TransportState::Paused;
}
} // namespace

TransportController::TransportController(const CatalogSession& catalog,
                                         PreparationPort& preparation, AudioTransportPort& audio,
                                         const TransportTiming timing,
                                         const TransportCounters counters) noexcept
    : catalog_(catalog), preparation_port_(preparation), audio_port_(audio), timing_(timing),
      counters_(counters) {
  state_.play_generation = counters.last_play_generation;
}

TransportSnapshot TransportController::snapshot() const noexcept {
  auto result = state_;
  result.catalog = catalog_status_;
  result.prepared = state_.selection.has_value() && prepared_generation_ != 0 &&
                    prepared_generation_ == state_.play_generation;
  return result;
}

TransportProjection TransportController::projection() const noexcept {
  return {state_.projected,
          state_.selection,
          prepared_generation_ != 0 && prepared_generation_ == state_.play_generation &&
              !reset_required_,
          state_.failure != TransportFailure::None,
          reset_required_ || !state_.silence_confirmed || state_.preparation.has_value() ||
              state_.audio_control.has_value() || fault_observed_,
          state_.terminal};
}

TransportAdmissionContext TransportController::admission_context() const noexcept {
  return {catalog_status_, state_.play_generation, state_.failure, state_.terminal,
          state_.pause_supported};
}

bool TransportController::advance_generation() {
  if (counters_.last_play_generation == std::numeric_limits<std::uint64_t>::max()) {
    fail(TransportFailure::ResourceExhausted, true);
    return false;
  }
  state_.play_generation = ++counters_.last_play_generation;
  return true;
}

std::optional<std::uint64_t> TransportController::next_operation() {
  if (counters_.last_operation_id == std::numeric_limits<std::uint64_t>::max()) {
    fail(TransportFailure::ResourceExhausted, true);
    return std::nullopt;
  }
  return ++counters_.last_operation_id;
}

void TransportController::invalidate() {
  if (state_.terminal)
    return;
  if (!advance_generation())
    return;
  reset_required_ = true;
  need_prepare_ = false;
  held_end_frame_.reset();
  state_.media_frame = 0;
}

void TransportController::fail(const TransportFailure failure, const bool terminal) {
  audio_port_.emergency_silence();
  if (state_.terminal)
    return;
  state_.failure = failure;
  state_.terminal = terminal;
  target_ = TransportState::Error;
  state_.transport = TransportState::Error;
  state_.projected = TransportState::Error;
  need_prepare_ = false;
  held_end_frame_.reset();
  reset_required_ = true;
  if (!terminal)
    static_cast<void>(advance_generation());
}

void TransportController::sync_catalog(const bool fault_this_step) {
  const auto current = catalog_.status();
  const bool changed = !catalog_seen_ || current.generation != catalog_status_.generation;
  const bool phase_changed = current.phase != catalog_status_.phase;
  catalog_seen_ = true;
  catalog_status_ = current;
  if (changed) {
    invalidate();
    state_.selection.reset();
    state_.pending_intent.reset();
  }
  if (state_.terminal || fault_this_step || (!changed && !phase_changed))
    return;
  if (!changed && state_.failure != TransportFailure::None)
    return; // A library completion cannot clear a shared playback failure.
  if (changed)
    state_.failure = TransportFailure::None;
  switch (current.phase) {
  case CatalogPhase::Ready:
    target_ = TransportState::Stopped;
    break;
  case CatalogPhase::Empty:
    target_ = TransportState::Empty;
    break;
  case CatalogPhase::Loading:
    target_ = TransportState::Loading;
    break;
  case CatalogPhase::Error:
    state_.failure = TransportFailure::Library;
    target_ = TransportState::Error;
    break;
  }
  state_.transport = target_ == TransportState::Error ? target_ : TransportState::Loading;
  state_.projected = target_;
}

void TransportController::select(const TransportIntent& intent, const bool play) {
  invalidate();
  if (state_.terminal)
    return;
  state_.failure = TransportFailure::None;
  state_.selection = intent.selection;
  state_.pending_intent = intent;
  need_prepare_ = true;
  target_ = play ? TransportState::Playing : TransportState::Stopped;
  state_.transport = TransportState::Loading;
  state_.projected = TransportState::Loading;
}

TransportTransition project_transport(const CatalogSession& catalog, const bool pause_supported,
                                      const TransportIntent& intent, TransportProjection& state) {
  const auto kind = intent.kind;
  const bool selecting =
      kind == TransportIntentKind::PlayTrack || kind == TransportIntentKind::LoadTrack;
  if (intent.command_id == 0 ||
      static_cast<std::uint8_t>(kind) > static_cast<std::uint8_t>(TransportIntentKind::Stop) ||
      (!selecting &&
       (intent.selection.library_generation.value != 0 || intent.selection.track_id.value != 0)))
    return {TransportRejection::Malformed};
  if (state.terminal)
    return {TransportRejection::TerminalFailure};
  if (selecting) {
    library::TrackView track;
    switch (catalog.resolve_track(intent.selection.library_generation, intent.selection.track_id,
                                  track)) {
    case CatalogTrackError::Unavailable:
      return {TransportRejection::LibraryUnavailable};
    case CatalogTrackError::StaleLibrary:
      return {TransportRejection::StaleLibrary};
    case CatalogTrackError::UnknownTrack:
      return {TransportRejection::UnknownTrack};
    case CatalogTrackError::None:
      break;
    }
    if (state.failure && state.recovery_busy)
      return {TransportRejection::Busy};
    state.state = TransportState::Loading;
    state.selection = intent.selection;
    state.prepared_for_start = false;
    state.failure = false;
    state.recovery_busy = true;
    return {TransportRejection::None,
            kind == TransportIntentKind::PlayTrack ? TransportAction::SelectPlay
                                                   : TransportAction::SelectLoad,
            intent.selection};
  }
  if ((kind == TransportIntentKind::Pause || kind == TransportIntentKind::Resume ||
       kind == TransportIntentKind::TogglePause ||
       (kind == TransportIntentKind::Play && state.state == TransportState::Paused)) &&
      !pause_supported)
    return {TransportRejection::UnsupportedPause};
  const auto projected = state.state;
  if (state.failure)
    return {TransportRejection::InvalidState};
  if (kind == TransportIntentKind::Stop) {
    if (projected == TransportState::Stopped)
      return {};
    if (!state.selection)
      return {TransportRejection::InvalidState};
    state.state = TransportState::Stopped;
    state.prepared_for_start = false;
    state.recovery_busy = true;
    return {TransportRejection::None, TransportAction::Stop};
  }
  if (kind == TransportIntentKind::Play &&
      (projected == TransportState::Stopped || projected == TransportState::Ended)) {
    if (!state.selection)
      return {TransportRejection::InvalidState};
    if (projected == TransportState::Stopped && state.prepared_for_start) {
      state.state = TransportState::Playing;
      return {TransportRejection::None, TransportAction::StartPrepared};
    }
    state.state = TransportState::Loading;
    state.prepared_for_start = false;
    state.recovery_busy = true;
    return {TransportRejection::None, TransportAction::SelectPlay, *state.selection};
  }
  const bool pause =
      kind == TransportIntentKind::Pause ||
      (kind == TransportIntentKind::TogglePause && projected == TransportState::Playing);
  const bool resume =
      kind == TransportIntentKind::Resume || kind == TransportIntentKind::Play ||
      (kind == TransportIntentKind::TogglePause && projected == TransportState::Paused);
  if (pause && projected == TransportState::Paused)
    return {};
  if (kind == TransportIntentKind::Play && projected == TransportState::Playing)
    return {};
  if ((pause && projected == TransportState::Playing) ||
      (resume && projected == TransportState::Paused)) {
    state.state = pause ? TransportState::Paused : TransportState::Playing;
    return {TransportRejection::None, pause ? TransportAction::Pause : TransportAction::Resume};
  }
  return {TransportRejection::InvalidState};
}

TransportRejection TransportController::apply(const TransportIntent& intent) {
  auto projected = projection();
  const auto transition = project_transport(catalog_, state_.pause_supported, intent, projected);
  if (transition.rejection != TransportRejection::None)
    return transition.rejection;
  switch (transition.action) {
  case TransportAction::None:
    return TransportRejection::None;
  case TransportAction::SelectPlay:
  case TransportAction::SelectLoad: {
    auto request = intent;
    request.selection = transition.selection;
    select(request, transition.action == TransportAction::SelectPlay);
    break;
  }
  case TransportAction::Stop:
    invalidate();
    if (state_.terminal)
      return TransportRejection::TerminalFailure;
    target_ = TransportState::Stopped;
    state_.transport = TransportState::Loading;
    state_.projected = target_;
    break;
  case TransportAction::StartPrepared:
  case TransportAction::Resume:
    target_ = TransportState::Playing;
    state_.projected = target_;
    break;
  case TransportAction::Pause:
    target_ = TransportState::Paused;
    state_.projected = target_;
    break;
  }
  if (state_.terminal)
    return TransportRejection::TerminalFailure;
  state_.pending_intent = intent;
  return TransportRejection::None;
}

void TransportController::consume_audio(const AudioObservation& observation) {
  if (!observation.completion || !state_.audio_control)
    return;
  const auto& completion = *observation.completion;
  const auto expected = state_.audio_control->request;
  if (completion.request.operation_id != expected.operation_id)
    return;
  if (completion.request.play_generation != expected.play_generation ||
      completion.request.kind != expected.kind ||
      (completion.outcome != AudioControlOutcome::Success &&
       completion.outcome != AudioControlOutcome::Failed)) {
    fail(TransportFailure::Protocol, true);
    return;
  }
  state_.audio_control.reset();
  if (completion.outcome == AudioControlOutcome::Failed) {
    fail(expected.kind == AudioControlKind::Reset ? TransportFailure::ResetFailed
                                                  : TransportFailure::AudioControl,
         expected.kind == AudioControlKind::Reset);
    return;
  }
  if (expected.kind == AudioControlKind::Reset) {
    if (completion.media_frame != 0 || observation.fault) {
      fail(TransportFailure::ResetFailed, true);
      return;
    }
    state_.silence_confirmed = true;
    started_generation_ = 0;
    if (expected.play_generation == state_.play_generation)
      reset_required_ = false;
  }
  if (state_.terminal) {
    audio_port_.emergency_silence(); // A late reset must not clear the terminal inhibit.
    return;
  }
  if (expected.play_generation != state_.play_generation || target_ == TransportState::Error)
    return;
  switch (expected.kind) {
  case AudioControlKind::Reset:
    break;
  case AudioControlKind::Start:
    started_generation_ = expected.play_generation;
    state_.transport = TransportState::Playing;
    state_.media_frame = completion.media_frame;
    if (state_.projected == TransportState::Loading)
      state_.projected = target_;
    break;
  case AudioControlKind::Pause:
    if (completion.media_frame < state_.media_frame) {
      fail(TransportFailure::Protocol, true);
      return;
    }
    state_.media_frame = completion.media_frame;
    state_.transport = TransportState::Paused;
    break;
  case AudioControlKind::Resume:
    if (completion.media_frame != state_.media_frame) {
      fail(TransportFailure::Protocol, true);
      return;
    }
    state_.transport = TransportState::Playing;
    break;
  }
}

void TransportController::consume_preparation() {
  if (!state_.preparation)
    return;
  const auto pending = *state_.preparation;
  const auto progress = preparation_port_.poll(pending.request.operation_id);
  if (progress == PreparationProgress::Pending)
    return;
  if (progress != PreparationProgress::Ready && progress != PreparationProgress::Failed &&
      progress != PreparationProgress::Cancelled) {
    fail(TransportFailure::Protocol, true);
    return;
  }
  state_.preparation.reset();
  const bool obsolete = pending.cancel_requested ||
                        pending.request.play_generation != state_.play_generation ||
                        state_.terminal;
  if (obsolete) {
    if (progress == PreparationProgress::Ready)
      preparation_port_.release();
    return;
  }
  if (progress != PreparationProgress::Ready) {
    fail(TransportFailure::Preparation, false);
    return;
  }
  prepared_generation_ = pending.request.play_generation;
  need_prepare_ = false;
}

void TransportController::check_timeouts(const std::uint64_t now_us) {
  if (state_.terminal)
    return;
  if (state_.audio_control &&
      now_us - state_.audio_control->started_at_us >= timing_.control_timeout_us) {
    fail(TransportFailure::AudioTimeout, true);
    return;
  }
  if (state_.preparation && state_.failure != TransportFailure::PreparationTimeout &&
      now_us - state_.preparation->started_at_us >= timing_.prepare_timeout_us)
    fail(TransportFailure::PreparationTimeout, false);
}

void TransportController::consume_position(const AudioObservation& observation) {
  if (state_.terminal || state_.failure != TransportFailure::None || reset_required_ ||
      !playback_target(state_.transport) || observation.play_generation != state_.play_generation)
    return;
  if (observation.media_frame < state_.media_frame ||
      (state_.transport == TransportState::Paused &&
       observation.media_frame != state_.media_frame)) {
    fail(TransportFailure::Protocol, true);
    return;
  }
  state_.media_frame = observation.media_frame;
  if (observation.ended)
    held_end_frame_ = observation.media_frame;
  if (held_end_frame_ && state_.transport == TransportState::Playing) {
    state_.media_frame = *held_end_frame_;
    held_end_frame_.reset();
    target_ = TransportState::Ended;
    state_.projected = target_;
    state_.transport = TransportState::Loading;
    state_.pending_intent.reset();
    reset_required_ = true;
  }
}

void TransportController::start_audio(const AudioControlKind kind, const std::uint64_t now_us) {
  const auto operation = next_operation();
  if (!operation)
    return;
  const AudioControlRequest request{*operation, state_.play_generation, kind};
  if (!audio_port_.begin(request)) {
    fail(kind == AudioControlKind::Reset ? TransportFailure::ResetFailed
                                         : TransportFailure::AudioControl,
         kind == AudioControlKind::Reset);
    return;
  }
  state_.audio_control = PendingAudioControl{request, now_us};
  state_.silence_confirmed = false;
}

void TransportController::settle() {
  if (state_.terminal || reset_required_ || state_.preparation || state_.audio_control ||
      need_prepare_)
    return;
  if (playback_target(target_)) {
    if (state_.transport == target_)
      state_.pending_intent.reset();
    return;
  }
  state_.transport = target_;
  state_.projected = target_;
  state_.pending_intent.reset();
  if (target_ != TransportState::Ended)
    state_.media_frame = 0;
}

void TransportController::pump(const std::uint64_t now_us) {
  if (state_.preparation && !state_.preparation->cancel_requested &&
      (state_.preparation->request.play_generation != state_.play_generation || state_.terminal)) {
    preparation_port_.cancel(state_.preparation->request.operation_id);
    state_.preparation->cancel_requested = true;
  }
  if (state_.terminal) {
    if (state_.silence_confirmed && !state_.audio_control && prepared_generation_ != 0) {
      preparation_port_.release();
      prepared_generation_ = 0;
    }
    return;
  }
  if (reset_required_) {
    if (!state_.audio_control)
      start_audio(AudioControlKind::Reset, now_us);
    return;
  }
  if (state_.audio_control)
    return;
  if (prepared_generation_ != 0 && prepared_generation_ != state_.play_generation &&
      state_.silence_confirmed) {
    preparation_port_.release();
    prepared_generation_ = 0;
  }
  if (state_.preparation)
    return;
  if (need_prepare_ && state_.selection) {
    const auto operation = next_operation();
    if (!operation)
      return;
    const PreparationRequest request{*operation, state_.play_generation, *state_.selection};
    if (!preparation_port_.begin(request)) {
      fail(TransportFailure::Preparation, false);
      return;
    }
    state_.preparation = PendingPreparation{request, now_us, false};
    return;
  }
  if (playback_target(target_) && prepared_generation_ == state_.play_generation) {
    if (started_generation_ != state_.play_generation)
      start_audio(AudioControlKind::Start, now_us);
    else if (target_ == TransportState::Paused && state_.transport == TransportState::Playing)
      start_audio(AudioControlKind::Pause, now_us);
    else if (target_ == TransportState::Playing && state_.transport == TransportState::Paused)
      start_audio(AudioControlKind::Resume, now_us);
  }
  settle();
}

TransportStepResult
TransportController::step(const std::uint64_t now_us, const TransportBatch& batch,
                          const std::optional<TransportAdmissionContext>& admitted) {
  if (timing_.prepare_timeout_us == 0 || timing_.control_timeout_us == 0)
    fail(TransportFailure::InvalidConfiguration, true);
  if (clock_started_ && now_us < last_now_us_)
    fail(TransportFailure::Clock, true);
  last_now_us_ = now_us;
  clock_started_ = true;
  const auto observation = audio_port_.observe();
  state_.pause_supported = audio_port_.supports_pause();
  const bool new_fault =
      observation.fault && (!fault_observed_ || state_.failure != TransportFailure::DeviceFault);
  fault_observed_ = observation.fault;
  if (new_fault && !state_.terminal)
    fail(TransportFailure::DeviceFault, false);
  else if (observation.fault)
    audio_port_.emergency_silence();
  sync_catalog(observation.fault);
  TransportStepResult result;
  if (batch.count > kTransportBatchCapacity) {
    result.batch_error = TransportRejection::BatchTooLarge;
    if (admitted && state_.failure == TransportFailure::None)
      fail(TransportFailure::Protocol, true);
  } else {
    const bool catalog_changed = admitted && admitted->catalog != catalog_status_;
    const bool capability_changed = admitted && admitted->pause_supported != state_.pause_supported;
    const bool interrupted =
        admitted && batch.count != 0 &&
        (observation.fault || catalog_changed || capability_changed ||
         admitted->play_generation != state_.play_generation ||
         admitted->failure != state_.failure || admitted->terminal != state_.terminal);
    if (interrupted) {
      if (state_.failure == TransportFailure::None) {
        if (catalog_changed)
          fail(TransportFailure::Library, false);
        else if (capability_changed)
          fail(TransportFailure::DeviceFault, false);
        else
          fail(TransportFailure::Protocol, true);
      }
      result.batch_error = TransportRejection::Busy;
    } else {
      result.count = batch.count;
      for (std::uint16_t i = 0; i < batch.count; ++i) {
        result.decisions[i] = {batch.intents[i].command_id, apply(batch.intents[i])};
        if (admitted && !result.decisions[i].accepted()) {
          if (state_.failure == TransportFailure::None)
            fail(TransportFailure::Protocol, true);
          result.count = static_cast<std::uint16_t>(i + 1);
          break;
        }
      }
    }
  }
  consume_audio(observation);
  consume_preparation();
  check_timeouts(now_us);
  consume_position(observation);
  pump(now_us);
  return result;
}

} // namespace rpcmp::player
