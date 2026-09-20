#include "rpcmp/platform/pocket/sound_mmio_client.hpp"

#include <limits>

namespace rpcmp::platform::pocket {
namespace {
using C = SoundChannel;
constexpr std::size_t index(const C channel) noexcept { return static_cast<std::size_t>(channel); }
struct Mailbox {
  std::uintptr_t submit, release;
  unsigned shift;
};
constexpr std::array<Mailbox, 4> boxes{
    {{0x044, 0x048, 0}, {0x0b0, 0x0b4, 2}, {0x110, 0x114, 4}, {0x31c, 0x320, 12}}};
constexpr std::uint32_t all_mailboxes = 0x303f;
constexpr std::uint32_t online = 1U << 11U;
constexpr std::uint32_t emergency_pending = 1U << 10U;
constexpr std::uint32_t diagnostics = 3U << 8U;
constexpr std::uint32_t status_mask = 0x3fff;
constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
bool valid_control(const player::AudioControlRequest& request) noexcept {
  return request.operation_id != 0 && request.play_generation != 0 &&
         static_cast<unsigned>(request.kind) <=
             static_cast<unsigned>(player::AudioControlKind::SetPolicy) &&
         (request.kind == player::AudioControlKind::Reset
              ? !request.repeat
              : request.repeat && contracts::v2::valid_repeat_application(*request.repeat));
}
bool valid_item(const player::MdxSourceOffer& item) noexcept {
  if (item.generation == 0 || item.epoch == 0 || item.token == 0)
    return false;
  if (!item.marker)
    return !item.ended && item.until_tick == 0 && item.completed_loops == 0;
  return item.address == 0 && item.value == 0 &&
         (item.ended ? item.until_tick == 0 : item.until_tick > item.at_tick);
}
} // namespace

std::uint32_t SoundMmioClient::read(const std::uintptr_t offset) noexcept {
  return mmio_.read(kSoundMmioBase + offset);
}
void SoundMmioClient::write(const std::uintptr_t offset, const std::uint32_t value) noexcept {
  mmio_.write(kSoundMmioBase + offset, value);
}
std::uint64_t SoundMmioClient::read64(const std::uintptr_t offset) noexcept {
  const auto low = read(offset);
  return (static_cast<std::uint64_t>(read(offset + 4)) << 32U) | low;
}
void SoundMmioClient::write64(const std::uintptr_t offset, const std::uint64_t value) noexcept {
  write(offset, static_cast<std::uint32_t>(value));
  write(offset + 4, static_cast<std::uint32_t>(value >> 32U));
}

SoundSubmit SoundMmioClient::initialize() noexcept {
  if (initialized_)
    return clock_failed_ ? SoundSubmit::ClockFailure : SoundSubmit::Accepted;
  if (read(0) != 0x52534d31 || read(4) != 0x00010001 || read(8) != 15 ||
      read(0x018) != 12'288'000 || read(0x01c) != 48'000)
    return SoundSubmit::Incompatible;
  status_ = read(0x00c);
  if ((status_ & ~status_mask) != 0)
    return SoundSubmit::Incompatible;
  if ((status_ & all_mailboxes) != 0)
    return SoundSubmit::Busy;
  if ((status_ & diagnostics) != 0)
    return SoundSubmit::Faulted;
  last_clock_ = clock_.now_us();
  initialized_ = true;
  return SoundSubmit::Accepted;
}

bool SoundMmioClient::busy(const C channel) const noexcept {
  const auto i = index(channel);
  return i >= slots_.size() || slots_[i].borrowed || slots_[i].result_ready;
}
void SoundMmioClient::emergency_silence() noexcept {
  if (!initialized_)
    return;
  critical_failure_ = true;
  reset_can_recover_ = false;
  write(0x010, 1);
}
void SoundMmioClient::fail(const C channel, const SoundTransfer reason) noexcept {
  auto& slot = slots_[index(channel)];
  if (slot.abandoned)
    return;
  slot.abandoned = true;
  slot.result_ready = true;
  switch (channel) {
  case C::Control:
    control_.transfer = reason;
    break;
  case C::Feed:
    feed_.transfer = reason;
    break;
  case C::Capture:
    capture_.transfer = reason;
    break;
  case C::Journal:
    journal_.transfer = reason;
    break;
  }
  if (channel != C::Journal && reason != SoundTransfer::ClockFailure)
    emergency_silence();
}
bool SoundMmioClient::read_clock(std::uint64_t& value) noexcept {
  if (clock_failed_)
    return false;
  value = clock_.now_us();
  if (value < last_clock_) {
    clock_failed_ = true;
    emergency_silence();
    for (std::size_t i = 0; i < slots_.size(); ++i)
      if (slots_[i].borrowed)
        fail(static_cast<C>(i), SoundTransfer::ClockFailure);
    return false;
  }
  last_clock_ = value;
  return true;
}

SoundSubmit SoundMmioClient::prepare(const C channel, const bool reset) noexcept {
  if (!initialized_)
    return SoundSubmit::NotInitialized;
  if (busy(channel))
    return SoundSubmit::Busy;
  std::uint64_t now = 0;
  if (!read_clock(now))
    return SoundSubmit::ClockFailure;
  if (critical_failure_ && (channel == C::Feed || (channel == C::Control && !reset)))
    return SoundSubmit::Faulted;
  status_ = read(0x00c);
  if ((status_ & (~status_mask | diagnostics)) != 0) {
    emergency_silence();
    return SoundSubmit::Faulted;
  }
  if ((status_ & online) == 0 || (status_ & (3U << boxes[index(channel)].shift)) != 0 ||
      (reset && (status_ & emergency_pending) != 0))
    return SoundSubmit::Busy;
  return SoundSubmit::Accepted;
}
void SoundMmioClient::submit(const C channel) noexcept {
  auto& slot = slots_[index(channel)];
  slot = {};
  slot.borrowed = true;
  write(boxes[index(channel)].submit, 1);
  if (!read_clock(slot.submitted_at))
    fail(channel, SoundTransfer::ClockFailure);
}

SoundSubmit SoundMmioClient::control(const player::AudioControlRequest& request) noexcept {
  if (!valid_control(request))
    return SoundSubmit::Invalid;
  const auto ready = prepare(C::Control, request.kind == player::AudioControlKind::Reset);
  if (ready != SoundSubmit::Accepted)
    return ready;
  control_ = {};
  control_.request = request;
  reset_can_recover_ = request.kind == player::AudioControlKind::Reset;
  write64(0x020, request.operation_id);
  write64(0x028, request.play_generation);
  write(0x030, static_cast<std::uint32_t>(request.kind));
  write64(0x034, request.repeat ? request.repeat->revision : 0);
  write(0x03c, request.repeat && request.repeat->target ? 1 : 0);
  write(0x040, request.repeat ? request.repeat->target.value_or(0) : 0);
  submit(C::Control);
  return SoundSubmit::Accepted;
}
SoundSubmit SoundMmioClient::feed(const player::MdxSourceOffer& item) noexcept {
  if (!valid_item(item))
    return SoundSubmit::Invalid;
  const auto ready = prepare(C::Feed);
  if (ready != SoundSubmit::Accepted)
    return ready;
  feed_ = {};
  feed_.item = {item.generation, item.epoch, item.token, player::MdxFeedStatus::Failed};
  write64(0x080, item.generation);
  write64(0x088, item.epoch);
  write64(0x090, item.at_tick);
  write64(0x098, item.until_tick);
  write64(0x0a0, item.completed_loops);
  write(0x0a8, (item.marker ? 1U : 0U) | (item.ended ? 2U : 0U));
  write(0x0ac, (static_cast<std::uint32_t>(item.address) << 8U) | item.value);
  submit(C::Feed);
  return SoundSubmit::Accepted;
}
SoundSubmit SoundMmioClient::capture(const SoundQuery query) noexcept {
  if (query.ticket == 0)
    return SoundSubmit::Invalid;
  const auto ready = prepare(C::Capture);
  if (ready != SoundSubmit::Accepted)
    return ready;
  capture_ = {};
  capture_.request = query;
  write64(0x100, query.ticket);
  write64(0x108, query.epoch);
  submit(C::Capture);
  return SoundSubmit::Accepted;
}
SoundSubmit SoundMmioClient::journal(const SoundJournalQuery query) noexcept {
  if (query.ticket == 0 || query.head_sequence == maximum)
    return SoundSubmit::Invalid;
  const auto ready = prepare(C::Journal);
  if (ready != SoundSubmit::Accepted)
    return ready;
  journal_ = {};
  journal_.request = query;
  write64(0x300, query.ticket);
  write64(0x308, query.epoch);
  write64(0x310, query.head_sequence);
  write(0x318, query.head_sequence != 0 ? 1 : 0);
  submit(C::Journal);
  return SoundSubmit::Accepted;
}

bool SoundMmioClient::read_control() noexcept {
  const auto& request = control_.request;
  const auto id = read64(0x180);
  const auto generation = read64(0x188);
  const auto revision = read64(0x190);
  const auto kind = read(0x198);
  const auto enabled = read(0x19c);
  const auto target = read(0x1a0);
  control_.frame = read64(0x1a4);
  const auto result = read(0x1ac);
  control_.epoch = read64(0x1b0);
  control_.prepared_generation = read64(0x1b8);
  control_.result = static_cast<SoundReply>(result);
  return id == request.operation_id && generation == request.play_generation &&
         kind == static_cast<std::uint32_t>(request.kind) &&
         revision == (request.repeat ? request.repeat->revision : 0) &&
         enabled == (request.repeat && request.repeat->target ? 1U : 0U) &&
         target == (request.repeat ? request.repeat->target.value_or(0) : 0) && result >= 1 &&
         result <= 4 &&
         (result != 1 ||
          (control_.epoch != 0 && control_.prepared_generation == generation &&
           (request.kind != player::AudioControlKind::Reset || control_.frame == 0) &&
           (request.kind != player::AudioControlKind::Start || control_.frame != 0)));
}
bool SoundMmioClient::read_feed() noexcept {
  const auto result = read(0x0b8);
  const auto generation = read64(0x0bc);
  const auto epoch = read64(0x0c4);
  if (result < 1 || result > 5 || generation != feed_.item.generation || epoch != feed_.item.epoch)
    return false;
  feed_.item.status = static_cast<player::MdxFeedStatus>(result);
  return true;
}
bool SoundMmioClient::read_prefix(const std::uintptr_t offset, SoundPrefix& prefix) noexcept {
  prefix.prefix = read64(offset);
  prefix.completed_loops = read64(offset + 8);
  const auto flags = read(offset + 16);
  prefix.valid = (flags & 1U) != 0;
  prefix.checkpoint = (flags & 2U) != 0;
  prefix.ended = (flags & 4U) != 0;
  return (flags & ~7U) == 0;
}
bool SoundMmioClient::read_capture() noexcept {
  const auto ticket = read64(0x200);
  const auto expected = read64(0x208);
  capture_.epoch = read64(0x210);
  capture_.prepared_generation = read64(0x218);
  auto& media = capture_.media;
  media.play_generation = read64(0x220);
  media.frame = read64(0x228);
  media.policy_revision = read64(0x230);
  media.completed_loops = read64(0x238);
  const auto target = read(0x240);
  const auto flags = read(0x244);
  const auto state = read(0x248);
  media.target = (flags & 1U) != 0 ? std::optional<std::uint32_t>{target} : std::nullopt;
  media.paused = (flags & 2U) != 0;
  capture_.quiescent = (flags & 4U) != 0;
  capture_.resetting = (flags & 8U) != 0;
  capture_.device_idle = (flags & 16U) != 0;
  capture_.media_enabled = (flags & 32U) != 0;
  capture_.fault = (flags & 64U) != 0;
  capture_.inhibited = (flags & 128U) != 0;
  const auto phase = state & 3U;
  const auto end = (state >> 2U) & 7U;
  const auto failure = (state >> 5U) & 7U;
  media.phase = static_cast<player::MediaPhase>(phase);
  media.end = static_cast<player::MediaEnd>(end);
  media.failure = static_cast<player::MediaFailure>(failure);
  media.gain = read(0x24c);
  media.ramp_elapsed = read(0x250);
  media.ramp_duration =
      phase == 1 ? player::kMediaGainDenominator : (phase == 2 ? player::kMediaRestoreFrames : 0);
  const auto queued = read(0x254);
  capture_.queued = static_cast<std::uint8_t>(queued);
  const bool output_valid = read_prefix(0x258, capture_.output);
  const bool pending_valid = read_prefix(0x26c, capture_.pending);
  const auto result = read(0x280);
  capture_.result = static_cast<SoundReply>(result);
  return ticket == capture_.request.ticket && expected == capture_.request.epoch && result >= 1 &&
         result <= 3 && (result != 1 || capture_.epoch == expected) && (flags & ~255U) == 0 &&
         (state & ~255U) == 0 && phase <= 2 && end <= 4 && failure <= 4 && queued <= 64 &&
         media.gain <= player::kMediaGainDenominator && media.ramp_elapsed <= media.ramp_duration &&
         (media.target ? target != 0 : target == 0) && output_valid && pending_valid;
}
bool SoundMmioClient::read_record(const std::uintptr_t offset, const std::uint64_t epoch,
                                  player::MdxOutputRecord& record) noexcept {
  record.epoch = epoch;
  record.sequence = read64(offset);
  record.generation = read64(offset + 8);
  record.frame = read64(offset + 16);
  record.prefix = read64(offset + 24);
  const auto natural = read(offset + 32);
  record.natural_end = natural != 0;
  return natural <= 1;
}
bool SoundMmioClient::read_journal() noexcept {
  const auto ticket = read64(0x340);
  const auto expected = read64(0x348);
  journal_.epoch = read64(0x350);
  const auto result = read(0x358);
  journal_.result = static_cast<SoundJournalReply>(result);
  const auto queued = read(0x35c);
  journal_.queued = static_cast<std::uint8_t>(queued);
  journal_.lost = read64(0x360);
  journal_.next_sequence = read64(0x368);
  const auto flags = read(0x370);
  journal_.head_valid = (flags & 1U) != 0;
  journal_.latest_valid = (flags & 2U) != 0;
  journal_.exhausted = (flags & 4U) != 0;
  const bool head_encoding = read_record(0x374, journal_.epoch, journal_.head);
  const bool latest_encoding = read_record(0x398, journal_.epoch, journal_.latest);
  const auto valid_record = [&](const player::MdxOutputRecord& record, const bool valid,
                                const bool latest) {
    if (!valid)
      return record.sequence == 0 && record.generation == 0 && record.frame == 0 &&
             record.prefix == 0 && !record.natural_end;
    return record.generation != 0 && record.prefix != 0 &&
           (record.sequence != 0 ? record.sequence < journal_.next_sequence
                                 : latest && journal_.exhausted);
  };
  return ticket == journal_.request.ticket && expected == journal_.request.epoch && result >= 1 &&
         result <= 4 && (result > 2 || journal_.epoch == expected) && queued <= 32 &&
         (flags & ~7U) == 0 && journal_.head_valid == (queued != 0) &&
         journal_.next_sequence != 0 && journal_.exhausted == (journal_.next_sequence == maximum) &&
         head_encoding && latest_encoding &&
         valid_record(journal_.head, journal_.head_valid, false) &&
         valid_record(journal_.latest, journal_.latest_valid, true) &&
         (result != 1 ||
          (journal_.head_valid && (journal_.request.head_sequence == 0 ||
                                   journal_.head.sequence == journal_.request.head_sequence))) &&
         (result != 2 || !journal_.head_valid);
}

void SoundMmioClient::service() noexcept {
  if (!initialized_)
    return;
  status_ = read(0x00c);
  if ((status_ & (~status_mask | diagnostics)) != 0) {
    emergency_silence();
    for (std::size_t i = 0; i < slots_.size(); ++i)
      if (slots_[i].borrowed)
        fail(static_cast<C>(i), SoundTransfer::Protocol);
    return;
  }
  for (std::size_t i = 0; i < slots_.size(); ++i) {
    auto& slot = slots_[i];
    if (!slot.borrowed)
      continue;
    const auto channel = static_cast<C>(i);
    if ((status_ & (2U << boxes[i].shift)) != 0) {
      if ((status_ & (1U << boxes[i].shift)) == 0)
        fail(channel, SoundTransfer::Protocol);
      if (!slot.abandoned) {
        bool valid = false;
        switch (channel) {
        case C::Control:
          valid = read_control();
          break;
        case C::Feed:
          valid = read_feed();
          break;
        case C::Capture:
          valid = read_capture();
          break;
        case C::Journal:
          valid = read_journal();
          break;
        }
        if (!valid)
          fail(channel, SoundTransfer::Protocol);
        else {
          slot.result_ready = true;
          if (channel == C::Control && control_.request.kind == player::AudioControlKind::Reset &&
              control_.result == SoundReply::Success && !clock_failed_ && reset_can_recover_)
            critical_failure_ = false;
        }
      }
      write(boxes[i].release, 1);
      slot.borrowed = false;
    } else if ((status_ & (1U << boxes[i].shift)) == 0)
      fail(channel, SoundTransfer::Protocol);
  }
  std::uint64_t now = 0;
  if (!read_clock(now))
    return;
  for (std::size_t i = 0; i < slots_.size(); ++i)
    if (slots_[i].borrowed && !slots_[i].abandoned &&
        now - slots_[i].submitted_at >= kSoundMailboxTimeoutUs)
      fail(static_cast<C>(i), SoundTransfer::Timeout);
}

std::optional<SoundControlCompletion> SoundMmioClient::take_control() noexcept {
  auto& slot = slots_[index(C::Control)];
  if (!slot.result_ready)
    return std::nullopt;
  slot.result_ready = false;
  return control_;
}
std::optional<SoundFeedCompletion> SoundMmioClient::take_feed() noexcept {
  auto& slot = slots_[index(C::Feed)];
  if (!slot.result_ready)
    return std::nullopt;
  slot.result_ready = false;
  return feed_;
}
std::optional<SoundCaptureCompletion> SoundMmioClient::take_capture() noexcept {
  auto& slot = slots_[index(C::Capture)];
  if (!slot.result_ready)
    return std::nullopt;
  slot.result_ready = false;
  return capture_;
}
std::optional<SoundJournalCompletion> SoundMmioClient::take_journal() noexcept {
  auto& slot = slots_[index(C::Journal)];
  if (!slot.result_ready)
    return std::nullopt;
  slot.result_ready = false;
  return journal_;
}

} // namespace rpcmp::platform::pocket
