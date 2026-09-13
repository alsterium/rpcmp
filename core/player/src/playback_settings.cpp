#include "rpcmp/player/playback_settings.hpp"

#include "rpcmp/library/container.hpp"

#include <algorithm>
#include <limits>

namespace rpcmp::player {
namespace api = contracts::v2;
namespace {
std::uint64_t integer(const SettingsBytes& bytes, std::size_t offset, std::size_t count) noexcept {
  std::uint64_t value = 0;
  for (std::size_t i = 0; i < count; ++i)
    value |= static_cast<std::uint64_t>(bytes[offset + i]) << (8 * i);
  return value;
}
void integer(SettingsBytes& bytes, std::size_t offset, std::size_t count,
             std::uint64_t value) noexcept {
  for (std::size_t i = 0; i < count; ++i)
    bytes[offset + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
} // namespace

bool encode_settings(const SettingsRecord& record, SettingsBytes& bytes) noexcept {
  if (record.sequence == 0 || !api::valid_playback_policy(record.policy))
    return false;
  bytes = {};
  bytes[0] = 'R';
  bytes[1] = 'P';
  bytes[2] = 'S';
  bytes[3] = '1';
  bytes[4] = 1;
  bytes[6] = 64;
  integer(bytes, 8, 8, record.sequence);
  bytes[16] = static_cast<std::uint8_t>(record.policy.order);
  bytes[17] = static_cast<std::uint8_t>(record.policy.repeat);
  bytes[18] = record.policy.count ? 1 : 0;
  integer(bytes, 20, 4, record.policy.count.value_or(0));
  integer(bytes, 60, 4, library::crc32({bytes.data(), 60}));
  return true;
}
SettingsDecode decode_settings(const SettingsSlot& slot, SettingsRecord& record) noexcept {
  const auto& b = slot.bytes;
  if (slot.state != SettingsSlotState::Bytes || slot.length > b.size() || slot.length < 6 ||
      b[0] != 'R' || b[1] != 'P' || b[2] != 'S' || b[3] != '1')
    return SettingsDecode::Invalid;
  if (integer(b, 4, 2) != 1)
    return SettingsDecode::Unsupported;
  if (slot.length != 64 || integer(b, 6, 2) != 64 ||
      integer(b, 60, 4) != library::crc32({b.data(), 60}) || b[16] > 1 || b[17] > 2 ||
      integer(b, 18, 2) > 1 ||
      std::any_of(b.begin() + 24, b.begin() + 60, [](auto byte) { return byte != 0; }))
    return SettingsDecode::Invalid;
  SettingsRecord candidate{
      integer(b, 8, 8),
      {static_cast<api::PlaybackOrder>(b[16]), static_cast<api::RepeatMode>(b[17]), std::nullopt}};
  if (b[18] != 0)
    candidate.policy.count = static_cast<std::uint32_t>(integer(b, 20, 4));
  else if (integer(b, 20, 4) != 0)
    return SettingsDecode::Invalid;
  if (candidate.sequence == 0 || !api::valid_playback_policy(candidate.policy))
    return SettingsDecode::Invalid;
  record = candidate;
  return SettingsDecode::Valid;
}

PlaybackSettings::PlaybackSettings(const SettingsConfiguration configuration) noexcept
    : config_(configuration) {
  if (!config_.port || config_.timing.read_timeout_us == 0 ||
      config_.timing.commit_timeout_us == 0 || config_.timing.quiesce_timeout_us == 0) {
    ready_ = true;
    state_.restore = api::SettingsRestore::IoError;
    set_error(api::SettingsError::InvalidConfiguration, true);
    phase_ = Phase::Disabled;
  }
}
void PlaybackSettings::set_error(const api::SettingsError error, const bool permanent) noexcept {
  state_.save = permanent ? api::SettingsSave::Unavailable : api::SettingsSave::Failed;
  state_.error = error;
  permanent_ |= permanent;
  dirty_ = false;
}
void PlaybackSettings::unsupported_backend() noexcept {
  policy_ = {};
  state_.restore = api::SettingsRestore::Unsupported;
  state_.persisted_revision.reset();
  set_error(api::SettingsError::UnsupportedBackend, true);
  phase_ = Phase::Disabled;
}
void PlaybackSettings::fail(const api::SettingsError error, const std::uint64_t now_us,
                            const bool permanent) {
  if (!ready_) {
    ready_ = true;
    state_.restore = error == api::SettingsError::Timeout ? api::SettingsRestore::TimedOut
                                                          : api::SettingsRestore::IoError;
  }
  set_error(error, permanent);
  needs_read_ = true;
  if (active_) {
    config_.port->cancel(active_->id);
    cancel_since_ = now_us;
    phase_ = Phase::Cancelling;
  } else {
    phase_ = permanent_ ? Phase::Disabled : Phase::Idle;
  }
}
void PlaybackSettings::observe(const api::PlaybackPolicy& policy, const std::uint64_t revision,
                               const std::uint64_t now_us) noexcept {
  if (!ready_ || !api::valid_playback_policy(policy) || revision == 0 ||
      revision < state_.policy_revision)
    return;
  if (revision == state_.policy_revision)
    return;
  policy_ = policy;
  state_.policy_revision = revision;
  if (permanent_)
    return;
  dirty_ = true;
  changed_at_ = now_us;
  state_.save = active_ && active_->operation == SettingsOperation::Commit
                    ? api::SettingsSave::Writing
                    : api::SettingsSave::Pending;
  state_.error.reset();
}
void PlaybackSettings::read_slots(const std::array<SettingsSlot, 2>& slots,
                                  const std::uint64_t now_us) {
  std::array<SettingsRecord, 2> records{};
  std::array<bool, 2> valid{};
  bool unsupported = false, io = false, invalid = false;
  max_sequence_ = 0;
  next_slot_ = 0;
  for (std::size_t i = 0; i < slots.size(); ++i) {
    switch (slots[i].state) {
    case SettingsSlotState::Missing:
      break;
    case SettingsSlotState::Unsupported:
      unsupported = true;
      break;
    case SettingsSlotState::IoError:
      io = true;
      break;
    case SettingsSlotState::Bytes: {
      const auto decoded = decode_settings(slots[i], records[i]);
      valid[i] = decoded == SettingsDecode::Valid;
      unsupported |= decoded == SettingsDecode::Unsupported;
      invalid |= decoded == SettingsDecode::Invalid;
      if (valid[i])
        max_sequence_ = std::max(max_sequence_, records[i].sequence);
      break;
    }
    default:
      io = true;
      break;
    }
  }
  const bool conflict = valid[0] && valid[1] && records[0].sequence == records[1].sequence &&
                        slots[0].bytes != slots[1].bytes;
  std::optional<std::size_t> chosen;
  if (!io && !conflict) {
    if (valid[0])
      chosen = 0;
    if (valid[1] && (!valid[0] || records[1].sequence > records[0].sequence))
      chosen = 1;
  }
  if (chosen)
    next_slot_ = static_cast<std::uint8_t>(1 - *chosen);
  const bool restoring = !ready_;
  if (restoring) {
    ready_ = true;
    if (chosen)
      policy_ = records[*chosen].policy;
    state_.restore =
        unsupported ? api::SettingsRestore::Unsupported
        : io        ? api::SettingsRestore::IoError
        : conflict  ? api::SettingsRestore::Conflict
        : chosen    ? (invalid ? api::SettingsRestore::Recovered : api::SettingsRestore::Restored)
        : invalid   ? api::SettingsRestore::Invalid
                    : api::SettingsRestore::Missing;
  }
  if (unsupported) {
    fail(api::SettingsError::Unsupported, now_us, true);
    return;
  }
  if (io) {
    fail(api::SettingsError::Io, now_us);
    return;
  }
  needs_read_ = false;
  if (restoring) {
    if (chosen) {
      state_.persisted_revision = state_.policy_revision;
      state_.save = api::SettingsSave::Saved;
    } else if (invalid || conflict) {
      set_error(conflict ? api::SettingsError::Conflict : api::SettingsError::InvalidRecord, false);
    } else {
      state_.save = api::SettingsSave::NotSaved;
    }
  }
}
void PlaybackSettings::complete(const SettingsCompletion& c, const std::uint64_t now_us) {
  if (!active_ || c.id != active_->id)
    return;
  if (c.operation != active_->operation || c.revision != active_->revision ||
      (c.operation == SettingsOperation::Commit && c.slot != active_->slot) ||
      !config_.port->quiescent(c.id)) {
    fail(api::SettingsError::Protocol, now_us);
    return;
  }
  const auto request = *active_;
  active_.reset();
  phase_ = Phase::Idle;
  if (c.result != SettingsIoResult::Success) {
    if (!ready_ && c.result == SettingsIoResult::Unsupported) {
      ready_ = true;
      state_.restore = api::SettingsRestore::Unsupported;
    }
    fail(c.result == SettingsIoResult::Unsupported ? api::SettingsError::Unsupported
                                                   : api::SettingsError::Io,
         now_us, c.result == SettingsIoResult::Unsupported);
    return;
  }
  if (request.operation == SettingsOperation::Read) {
    read_slots(c.slots, now_us);
    return;
  }
  if (!c.durable) {
    fail(api::SettingsError::NotDurable, now_us);
    return;
  }
  if (c.readback.state != SettingsSlotState::Bytes || c.readback.length != 64 ||
      c.readback.bytes != request.bytes) {
    fail(api::SettingsError::ReadbackMismatch, now_us);
    return;
  }
  max_sequence_ = integer(request.bytes, 8, 8);
  next_slot_ = static_cast<std::uint8_t>(1 - request.slot);
  state_.persisted_revision = request.revision;
  dirty_ = request.revision != state_.policy_revision;
  state_.save = dirty_ ? api::SettingsSave::Pending : api::SettingsSave::Saved;
  state_.error.reset();
}
void PlaybackSettings::begin(const bool read, const std::uint64_t now_us) {
  if (config_.last_request_id == std::numeric_limits<std::uint64_t>::max()) {
    fail(api::SettingsError::RequestExhausted, now_us, true);
    return;
  }
  SettingsRequest request;
  request.id = ++config_.last_request_id;
  if (!read) {
    if (max_sequence_ == std::numeric_limits<std::uint64_t>::max()) {
      fail(api::SettingsError::SequenceExhausted, now_us, true);
      return;
    }
    request.operation = SettingsOperation::Commit;
    request.revision = state_.policy_revision;
    request.slot = next_slot_;
    if (!encode_settings({max_sequence_ + 1, policy_}, request.bytes)) {
      fail(api::SettingsError::Protocol, now_us, true);
      return;
    }
  }
  if (!config_.port->begin(request)) {
    fail(api::SettingsError::Io, now_us);
    return;
  }
  active_ = request;
  active_since_ = now_us;
  phase_ = Phase::Active;
  if (!read)
    state_.save = api::SettingsSave::Writing;
}
void PlaybackSettings::step(const std::uint64_t now_us) {
  if (phase_ == Phase::Disabled)
    return;
  if (clock_started_ && now_us < last_now_) {
    if (phase_ == Phase::Cancelling) {
      set_error(api::SettingsError::Clock, true);
      phase_ = Phase::Disabled;
    } else {
      fail(api::SettingsError::Clock, now_us, true);
    }
    return;
  }
  clock_started_ = true;
  last_now_ = now_us;
  if (phase_ == Phase::Initial) {
    begin(true, now_us);
    return;
  }
  if (phase_ == Phase::Cancelling) {
    if (active_ && config_.port->quiescent(active_->id)) {
      active_.reset();
      phase_ = permanent_ ? Phase::Disabled : Phase::Idle;
    } else if (now_us - cancel_since_ >= config_.timing.quiesce_timeout_us) {
      set_error(api::SettingsError::NotQuiescent, true);
      phase_ = Phase::Disabled;
    }
    return;
  }
  if (phase_ == Phase::Active && active_) {
    const auto timeout = active_->operation == SettingsOperation::Read
                             ? config_.timing.read_timeout_us
                             : config_.timing.commit_timeout_us;
    if (now_us - active_since_ >= timeout) {
      fail(api::SettingsError::Timeout, now_us);
      return;
    }
    const auto result = config_.port->poll();
    if (result)
      complete(*result, now_us);
    return;
  }
  if (phase_ == Phase::Idle && dirty_ && now_us >= changed_at_ && now_us - changed_at_ >= 250'000)
    begin(needs_read_, now_us);
}
} // namespace rpcmp::player
