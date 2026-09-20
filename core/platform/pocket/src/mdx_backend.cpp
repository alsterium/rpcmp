#include "rpcmp/platform/pocket/mdx_backend.hpp"

#include <limits>

namespace rpcmp::platform::pocket {
namespace {
using player::AudioControlKind;
using player::AudioControlOutcome;
using player::MdxSourceResult;
using player::PreparationProgress;
constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
bool terminal(const player::MediaEnd end) noexcept {
  return end == player::MediaEnd::NaturalEnd || end == player::MediaEnd::RepeatOne ||
         end == player::MediaEnd::LoopLimit;
}
bool reset_pending(const std::optional<player::AudioControlRequest>& request) noexcept {
  return request && request->kind == AudioControlKind::Reset;
}
} // namespace

SoundSubmit PocketMdxBackend::initialize() noexcept {
  const auto result = client_.initialize();
  if (result == SoundSubmit::Accepted)
    initialized_ = true;
  return result;
}
void PocketMdxBackend::stop_source() noexcept {
  supplying_ = false;
  feed_closed_ = false;
  offer_.reset();
  source_.cancel();
  history_ready_ = false;
}
void PocketMdxBackend::fail() noexcept {
  fault_ = true;
  stop_source();
  capture_.reset();
  quiescent_ = false;
  if (!client_.faulted())
    client_.emergency_silence();
  known_inhibit_ = true;
}
void PocketMdxBackend::emergency_silence() {
  stop_source();
  capture_.reset();
  quiescent_ = false;
  client_.emergency_silence();
  known_inhibit_ = true;
  if (control_ && !control_submitted_) {
    completion_ = player::AudioControlCompletion{*control_, AudioControlOutcome::Failed, 0};
    control_.reset();
  }
}

bool PocketMdxBackend::begin(const player::PreparationRequest& request) {
  if (!initialized_ || fault_ || known_inhibit_ || !quiescent_ || preparation_ || prepared_ ||
      control_ || request.operation_id == 0 || request.play_generation != reset_generation_ ||
      epoch_ == 0 || !catalog_.borrow_library(request.selection.library_generation))
    return false;
  preparation_ = request;
  preparation_state_ = PreparationProgress::Pending;
  document_loaded_ = false;
  prefilled_ = false;
  source_ended_ = false;
  return true;
}
void PocketMdxBackend::cancel(const std::uint64_t operation_id) {
  if (!preparation_ || preparation_->operation_id != operation_id)
    return;
  stop_source();
  document_loaded_ = false;
  preparation_state_ = PreparationProgress::Cancelled;
}
PreparationProgress PocketMdxBackend::poll(const std::uint64_t operation_id) {
  if (!preparation_ || preparation_->operation_id != operation_id)
    return PreparationProgress::Failed;
  const auto result = preparation_state_;
  if (result != PreparationProgress::Pending) {
    prepared_ = result == PreparationProgress::Ready;
    preparation_.reset();
  }
  return result;
}
void PocketMdxBackend::release() {
  if (!quiescent_ || preparation_) {
    fail();
    return;
  }
  stop_source();
  prepared_ = false;
  document_loaded_ = false;
  session_ = {};
}
bool PocketMdxBackend::begin(const player::AudioControlRequest& request) {
  if (!initialized_ || control_ || completion_ || request.operation_id == 0 ||
      request.play_generation == 0)
    return false;
  if (request.kind == AudioControlKind::Start &&
      (!prepared_ || !prefilled_ || request.play_generation != reset_generation_))
    return false;
  control_ = request;
  control_submitted_ = false;
  capture_.reset();
  capture_authority_ = 0;
  history_ready_ = false;
  drained_pause_control_ = 0;
  if (request.kind == AudioControlKind::Reset) {
    stop_source();
    quiescent_ = false;
  }
  return true;
}

void PocketMdxBackend::control_result(const SoundControlCompletion& result) noexcept {
  if (!control_ || !control_submitted_)
    return;
  const auto request = *control_;
  control_.reset();
  control_submitted_ = false;
  const bool success =
      result.transfer == SoundTransfer::Complete && result.result == SoundReply::Success;
  completion_ = player::AudioControlCompletion{
      request, success ? AudioControlOutcome::Success : AudioControlOutcome::Failed, result.frame};
  if (result.transfer != SoundTransfer::Complete) {
    fail();
    return;
  }
  if (!success)
    return;
  acknowledged_control_ = request.operation_id;
  if (request.kind == AudioControlKind::Reset) {
    if (client_.faulted() || result.epoch <= epoch_) {
      fail();
      return;
    }
    epoch_ = result.epoch;
    reset_generation_ = request.play_generation;
    started_generation_ = 0;
    fault_ = false;
    known_inhibit_ = false;
    quiescent_ = true;
    capture_.reset();
    latest_record_.reset();
    pop_sequence_ = 0;
    drained_pause_control_ = 0;
    history_ready_ = false;
    journal_available_ = false;
  } else if (request.kind == AudioControlKind::Start) {
    started_generation_ = request.play_generation;
  }
}
void PocketMdxBackend::capture_result(const SoundCaptureCompletion& result) noexcept {
  if (result.transfer != SoundTransfer::Complete) {
    fail();
    return;
  }
  if (result.epoch != epoch_ || reset_pending(control_))
    return;
  if (result.fault || result.media.failure != player::MediaFailure::None ||
      (result.inhibited && !known_inhibit_ && epoch_ != 0)) {
    fail();
    return;
  }
  if (result.result != SoundReply::Success || control_ || capture_authority_ == 0 ||
      capture_authority_ != acknowledged_control_ || fault_ || known_inhibit_)
    return;
  if (feed_closed_ && result.request.ticket > closed_capture_ticket_) {
    if (!terminal(result.media.end)) {
      fail();
      return;
    }
    stop_source();
  }
  if (started_generation_ == 0)
    return;
  if (result.prepared_generation != reset_generation_ ||
      result.media.play_generation != started_generation_) {
    fail();
    return;
  }
  capture_ = result;
  if (terminal(result.media.end))
    stop_source();
}
void PocketMdxBackend::feed_result(const SoundFeedCompletion& result) noexcept {
  if (result.transfer != SoundTransfer::Complete) {
    fail();
    return;
  }
  if (!supplying_ || result.item.generation != reset_generation_ || result.item.epoch != epoch_)
    return;
  if (result.item.status == player::MdxFeedStatus::Closed) {
    feed_closed_ = true;
    closed_capture_ticket_ = capture_ticket_;
    return;
  }
  const auto advanced = source_.complete(result.item);
  if (advanced != MdxSourceResult::Accepted && advanced != MdxSourceResult::Full) {
    fail();
    return;
  }
  if (advanced == MdxSourceResult::Full ||
      (source_ended_ && source_.availability() == MdxSourceResult::Closed)) {
    prefilled_ = true;
  }
  if (preparation_ && preparation_state_ == PreparationProgress::Pending && prefilled_)
    preparation_state_ = PreparationProgress::Ready;
}

void PocketMdxBackend::journal_result(const SoundJournalCompletion& result) noexcept {
  if (result.transfer != SoundTransfer::Complete) {
    history_ready_ = false;
    journal_available_ = false;
    pop_sequence_ = 0;
    drained_pause_control_ = 0;
    return;
  }
  if (result.epoch != epoch_ || result.request.epoch != epoch_ || reset_pending(control_))
    return;
  pop_sequence_ = 0;
  journal_available_ =
      result.result == SoundJournalReply::Success || result.result == SoundJournalReply::Empty;
  if (result.result == SoundJournalReply::Success) {
    static_cast<void>(history_.consume(result.head));
    if (result.request.head_sequence == 0)
      pop_sequence_ = result.head.sequence;
  } else if (result.result == SoundJournalReply::Empty) {
    if (result.latest_valid) {
      static_cast<void>(history_.recover(result.latest));
      latest_record_ = result.latest;
    }
    drained_pause_control_ = journal_pause_control_;
  } else {
    history_ready_ = false;
    drained_pause_control_ = 0;
  }
}

void PocketMdxBackend::prepare_document() noexcept {
  if (!preparation_ || preparation_state_ != PreparationProgress::Pending || document_loaded_)
    return;
  if (fault_ || !quiescent_ || preparation_->play_generation != reset_generation_) {
    preparation_state_ = PreparationProgress::Failed;
    return;
  }
  const auto* library = catalog_.borrow_library(preparation_->selection.library_generation);
  if (!library ||
      !player::prepare_mdx_library_session(*library, preparation_->selection.track_id, session_,
                                           document_scratch_, validation_scratch_,
                                           workspace_.engine)
           .ok() ||
      source_.begin(reset_generation_, epoch_) != MdxSourceResult::Accepted) {
    preparation_state_ = PreparationProgress::Failed;
    return;
  }
  document_loaded_ = true;
  supplying_ = true;
  static_cast<void>(history_.begin(reset_generation_, epoch_));
}
void PocketMdxBackend::supply() noexcept {
  if (!supplying_ || feed_closed_ || fault_ || known_inhibit_ || client_.busy(SoundChannel::Feed) ||
      (!started_generation_ && prefilled_))
    return;
  if (source_.availability() == MdxSourceResult::Accepted) {
    const auto produced = player::produce_mdx_tick(session_.document, session_.state, source_,
                                                   checkpoint_, workspace_);
    if (produced.status != MdxSourceResult::Accepted) {
      fail();
      return;
    }
    static_cast<void>(history_.retain(checkpoint_));
    source_ended_ = session_.state.progress.ended;
  }
  if (!offer_)
    offer_ = source_.take_offer();
  if (!offer_)
    return;
  const auto submitted = client_.feed(*offer_);
  if (submitted == SoundSubmit::Accepted)
    offer_.reset();
  else if (submitted != SoundSubmit::Busy)
    fail();
}
void PocketMdxBackend::request_control() noexcept {
  if (!control_ || control_submitted_)
    return;
  const auto submitted = client_.control(*control_);
  if (submitted == SoundSubmit::Accepted) {
    control_submitted_ = true;
    if (control_->kind == AudioControlKind::Start)
      quiescent_ = false;
  } else if (submitted != SoundSubmit::Busy) {
    completion_ = player::AudioControlCompletion{*control_, AudioControlOutcome::Failed, 0};
    control_.reset();
    if (submitted != SoundSubmit::Invalid)
      fail();
  }
}
void PocketMdxBackend::request_capture() noexcept {
  if (client_.busy(SoundChannel::Capture))
    return;
  if (capture_ticket_ == maximum) {
    fail();
    return;
  }
  const auto submitted = client_.capture({capture_ticket_ + 1, epoch_});
  if (submitted == SoundSubmit::Accepted) {
    ++capture_ticket_;
    capture_authority_ = control_ ? 0 : acknowledged_control_;
  } else if (submitted != SoundSubmit::Busy) {
    fail();
  }
}
void PocketMdxBackend::request_journal() noexcept {
  if (!document_loaded_ || epoch_ == 0 || client_.busy(SoundChannel::Journal) ||
      reset_pending(control_) || journal_ticket_ == maximum)
    return;
  const auto submitted = client_.journal({journal_ticket_ + 1, epoch_, pop_sequence_});
  if (submitted == SoundSubmit::Accepted) {
    ++journal_ticket_;
    journal_pause_control_ = capture_ && capture_->media.paused ? acknowledged_control_ : 0;
  } else if (submitted != SoundSubmit::Busy) {
    history_ready_ = false;
    journal_available_ = false;
  }
}
void PocketMdxBackend::publish_history() noexcept {
  history_ready_ = false;
  if (!capture_ || control_ || fault_ || known_inhibit_ || !journal_available_)
    return;
  const auto& captured = *capture_;
  player::MdxHistoryObservation observation{started_generation_, epoch_, captured.media.frame, 0,
                                            false};
  if (captured.media.end == player::MediaEnd::NaturalEnd ||
      captured.media.end == player::MediaEnd::RepeatOne) {
    if (!latest_record_ || !latest_record_->natural_end ||
        latest_record_->frame != captured.media.frame)
      return;
    observation.prefix = latest_record_->prefix;
    observation.natural_end = true;
  } else if (captured.media.paused) {
    // Later captures of the same acknowledged held pause do not invalidate
    // the Empty query's proof. A new control clears this authority in begin().
    if (drained_pause_control_ != acknowledged_control_ || !latest_record_)
      return;
    observation.prefix = latest_record_->prefix;
  } else if (captured.output.valid) {
    observation.prefix = captured.output.prefix;
  } else {
    return;
  }
  const auto result = history_.observe(observation);
  history_ready_ =
      result == player::MdxHistoryResult::Accepted || result == player::MdxHistoryResult::Degraded;
}
void PocketMdxBackend::service() noexcept {
  if (!initialized_)
    return;
  client_.service();
  if (const auto result = client_.take_control())
    control_result(*result);
  if (const auto result = client_.take_capture())
    capture_result(*result);
  if (const auto result = client_.take_feed())
    feed_result(*result);
  if (const auto result = client_.take_journal())
    journal_result(*result);
  if (client_.faulted() && !known_inhibit_)
    fail();
  prepare_document();
  request_control();
  supply();
  publish_history();
  request_capture();
  request_journal();
}
player::AudioObservation PocketMdxBackend::observe() {
  service();
  player::AudioObservation result;
  result.fault = fault_;
  result.completion = completion_;
  completion_.reset();
  if (capture_ && !control_ && !fault_ && !known_inhibit_) {
    result.play_generation = capture_->media.play_generation;
    result.media_frame = capture_->media.frame;
    result.ended = terminal(capture_->media.end);
    result.media = capture_->media;
  }
  return result;
}
void PocketMdxBackend::copy_to(contracts::v2::PerformanceHistorySnapshot& output) const noexcept {
  if (history_ready_)
    history_.copy_to(output);
  else {
    output = {};
    output.channels = contracts::v2::unknown_performance_channels();
    output.availability = contracts::v2::PerformanceAvailability::Waiting;
  }
}

} // namespace rpcmp::platform::pocket
