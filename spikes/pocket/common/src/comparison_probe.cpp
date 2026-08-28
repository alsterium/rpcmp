#include "rpcmp/spike/comparison_probe.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace rpcmp::spike {
namespace {

class SemanticHash {
public:
  void add_byte(const std::uint8_t value) noexcept {
    value_ ^= value;
    value_ *= 1'099'511'628'211ULL;
  }

  template <typename Value> void add_integer(const Value value) noexcept {
    using Unsigned = std::make_unsigned_t<Value>;
    auto remaining = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Value); ++index) {
      add_byte(static_cast<std::uint8_t>(remaining & static_cast<Unsigned>(0xFFU)));
      remaining >>= 8U;
    }
  }

  template <typename Enum> void add_enum(const Enum value) noexcept {
    add_integer(static_cast<std::underlying_type_t<Enum>>(value));
  }

  void add_bool(const bool value) noexcept { add_byte(value ? 1U : 0U); }

  void add_string(const std::string& value) noexcept {
    add_integer(static_cast<std::uint64_t>(value.size()));
    for (const auto character : value) {
      add_byte(static_cast<std::uint8_t>(character));
    }
  }

  std::uint64_t value() const noexcept { return value_; }

private:
  std::uint64_t value_{14'695'981'039'346'656'037ULL};
};

template <typename Value, typename AddValue>
void add_optional(SemanticHash& hash, const std::optional<Value>& value,
                  AddValue add_value) noexcept {
  hash.add_bool(value.has_value());
  if (value.has_value()) {
    add_value(*value);
  }
}

void add_snapshot(SemanticHash& hash, const contracts::PlayerSnapshot& snapshot) noexcept {
  hash.add_integer(snapshot.schema_version);
  hash.add_integer(snapshot.sequence);
  hash.add_integer(snapshot.published_at_tick);
  hash.add_integer(snapshot.capabilities.bits);
  hash.add_enum(snapshot.transport);

  add_optional(hash, snapshot.track, [&hash](const contracts::TrackSummary& track) {
    hash.add_integer(track.track_id.value);
    hash.add_string(track.title);
    hash.add_string(track.artist);
    add_optional(hash, track.album, [&hash](const std::string& value) { hash.add_string(value); });
    add_optional(hash, track.composer,
                 [&hash](const std::string& value) { hash.add_string(value); });
  });

  hash.add_integer(snapshot.position.position_ticks);
  hash.add_integer(snapshot.position.tick_rate);
  add_optional(hash, snapshot.position.duration_ticks,
               [&hash](const std::uint64_t value) { hash.add_integer(value); });
  add_optional(hash, snapshot.position.loop_start_ticks,
               [&hash](const std::uint64_t value) { hash.add_integer(value); });
  add_optional(hash, snapshot.position.loop_count,
               [&hash](const std::uint32_t value) { hash.add_integer(value); });
  hash.add_bool(snapshot.position.seekable);

  hash.add_integer(static_cast<std::uint64_t>(snapshot.channels.size()));
  for (const auto& channel : snapshot.channels) {
    hash.add_integer(channel.channel_id.value);
    hash.add_string(channel.label);
    hash.add_enum(channel.kind);
    hash.add_bool(channel.enabled);
    hash.add_bool(channel.mute_capable);
    hash.add_bool(channel.muted);
    hash.add_bool(channel.solo_capable);
    hash.add_bool(channel.solo);
    hash.add_bool(channel.key_on);
    add_optional(hash, channel.note,
                 [&hash](const std::uint8_t value) { hash.add_integer(value); });
    add_optional(hash, channel.fine_pitch_cents,
                 [&hash](const std::int16_t value) { hash.add_integer(value); });
    hash.add_integer(channel.level);
    hash.add_integer(channel.activity);
    add_optional(hash, channel.pan, [&hash](const std::int8_t value) { hash.add_integer(value); });
    add_optional(hash, channel.instrument_label,
                 [&hash](const std::string& value) { hash.add_string(value); });
    hash.add_integer(channel.device_id.value);
    hash.add_integer(channel.device_channel);
  }

  hash.add_integer(static_cast<std::uint64_t>(snapshot.devices.size()));
  for (const auto& device : snapshot.devices) {
    hash.add_integer(device.device_id.value);
    hash.add_enum(device.type);
    hash.add_integer(device.first_channel);
    hash.add_integer(device.channel_count);
    hash.add_enum(device.operational_state);
    hash.add_integer(device.capabilities.bits);
  }

  hash.add_integer(static_cast<std::uint64_t>(snapshot.visualization.recent_activity.size()));
  for (const auto activity : snapshot.visualization.recent_activity) {
    hash.add_integer(activity);
  }

  add_optional(hash, snapshot.error, [&hash](const contracts::PlayerError& error) {
    hash.add_enum(error.domain);
    hash.add_integer(error.code);
    hash.add_enum(error.severity);
    hash.add_bool(error.recoverable);
    add_optional(hash, error.detail, [&hash](const std::string& value) { hash.add_string(value); });
  });

  hash.add_integer(static_cast<std::uint64_t>(snapshot.extensions.size()));
  for (const auto& extension : snapshot.extensions) {
    hash.add_integer(extension.type_id);
    hash.add_integer(extension.version);
    hash.add_integer(static_cast<std::uint64_t>(extension.payload.size()));
    for (const auto byte : extension.payload) {
      hash.add_integer(byte);
    }
  }
}

void add_command_result(SemanticHash& hash, const contracts::CommandResult& result) noexcept {
  hash.add_integer(result.command_id);
  hash.add_enum(result.outcome);
  hash.add_enum(result.reason);
  hash.add_integer(result.observed_snapshot_sequence);
}

void add_event(SemanticHash& hash, const runtime::FakeDeviceEvent& event) noexcept {
  hash.add_integer(event.at_media_tick);
  hash.add_integer(event.channel_id.value);
  hash.add_bool(event.key_on);
  add_optional(hash, event.note, [&hash](const std::uint8_t value) { hash.add_integer(value); });
}

class SnapshotRecorder final : public runtime::ISnapshotObserver {
public:
  explicit SnapshotRecorder(IProbeRenderer* renderer) : renderer_(renderer) {}

  void published(const contracts::PlayerSnapshot& snapshot) override {
    ++count_;
    add_snapshot(hash_, snapshot);
    if (renderer_ != nullptr) {
      renderer_->render(snapshot);
    }
  }

  std::uint64_t count() const noexcept { return count_; }
  std::uint64_t digest() const noexcept { return hash_.value(); }

private:
  IProbeRenderer* renderer_{};
  SemanticHash hash_;
  std::uint64_t count_{};
};

class EventRecorder final : public runtime::IFakeDeviceSink {
public:
  void write(const runtime::FakeDeviceEvent& event) override {
    ++count_;
    add_event(hash_, event);
  }

  std::uint64_t count() const noexcept { return count_; }
  std::uint64_t digest() const noexcept { return hash_.value(); }

private:
  SemanticHash hash_;
  std::uint64_t count_{};
};

contracts::PlayerCommand make_command(const std::uint64_t id, contracts::Command payload,
                                      const std::optional<std::uint64_t> expected = std::nullopt) {
  return {contracts::kSchemaVersion, id, expected, std::move(payload)};
}

std::uint32_t checksum(const std::array<std::uint8_t, kSyntheticReadSize>& bytes) noexcept {
  std::uint32_t value = 2'166'136'261U;
  for (const auto byte : bytes) {
    value ^= byte;
    value *= 16'777'619U;
  }
  return value;
}

bool check_storage(IProbeBlobReader& reader, ProbeRunResult& result) noexcept {
  result.blob_size = reader.size();
  const std::array<std::uint32_t, kStorageCheckCount> offsets{
      0U, kSyntheticBlobSize / 2U, kSyntheticBlobSize - kSyntheticReadSize,
      kSyntheticBlobSize - (kSyntheticReadSize / 2U)};
  bool passed = result.blob_size == kSyntheticBlobSize;

  for (std::size_t index = 0; index < offsets.size(); ++index) {
    auto& check = result.storage_checks[index];
    check.offset = offsets[index];
    check.length = kSyntheticReadSize;
    check.expected_success = index + 1U < offsets.size();

    std::array<std::uint8_t, kSyntheticReadSize> bytes{};
    check.read_succeeded = reader.read(check.offset, bytes.data(), check.length);
    check.checksum = check.read_succeeded ? checksum(bytes) : 0U;
    check.content_matches = !check.expected_success && !check.read_succeeded;
    if (check.expected_success && check.read_succeeded) {
      check.content_matches = true;
      for (std::uint32_t byte = 0; byte < check.length; ++byte) {
        check.content_matches =
            check.content_matches && bytes[byte] == synthetic_byte(check.offset + byte);
      }
    }
    passed = passed && check.read_succeeded == check.expected_success && check.content_matches;
  }
  return passed;
}

bool advance(runtime::MockCore& core, const std::uint64_t tick) {
  return core.advance_to(tick) == runtime::AdvanceResult::Ok;
}

bool storage_equal(const StorageCheck& left, const StorageCheck& right) noexcept {
  return left.offset == right.offset && left.length == right.length &&
         left.checksum == right.checksum && left.expected_success == right.expected_success &&
         left.read_succeeded == right.read_succeeded &&
         left.content_matches == right.content_matches;
}

constexpr std::array<ProbeAction, 4> kInteractiveActions{
    ProbeAction::Play, ProbeAction::TogglePause, ProbeAction::TogglePause, ProbeAction::Stop};
constexpr std::array<contracts::TransportState, 4> kInteractiveTransports{
    contracts::TransportState::Playing, contracts::TransportState::Paused,
    contracts::TransportState::Playing, contracts::TransportState::Stopped};

contracts::Command action_command(const ProbeAction action) {
  switch (action) {
  case ProbeAction::Play:
    return contracts::Play{};
  case ProbeAction::TogglePause:
    return contracts::TogglePause{};
  case ProbeAction::Stop:
    return contracts::Stop{};
  }
  return contracts::Stop{};
}

} // namespace

std::uint32_t ReadLatencyStats::average_us() const noexcept {
  return iterations == 0U ? 0U : static_cast<std::uint32_t>(total_us / iterations);
}

bool ReadLatencyStats::passed() const noexcept {
  return iterations == kLatencyReadIterations && successful_reads == iterations &&
         content_matches && minimum_us <= maximum_us;
}

std::uint32_t TargetReadProfile::average_us() const noexcept {
  return iterations == 0U ? 0U : static_cast<std::uint32_t>(total_us / iterations);
}

bool TargetReadProfile::passed() const noexcept {
  return iterations > 0U && iterations <= kTargetProfileMaxSamples && length > 0U &&
         length <= kSyntheticBlobSize && successful_reads == iterations && content_matches &&
         minimum_us <= percentile_50_us && percentile_50_us <= percentile_90_us &&
         percentile_90_us <= percentile_95_us && percentile_95_us <= percentile_99_us &&
         percentile_99_us <= maximum_us && at_or_above_2ms <= at_or_above_1ms;
}

bool BoundedDeviceQueue::push(const ProbeDeviceWrite& write) noexcept {
  if (size_ == writes_.size()) {
    ++overflow_rejections_;
    return false;
  }
  writes_[tail_] = write;
  tail_ = (tail_ + 1U) % writes_.size();
  ++size_;
  return true;
}

bool BoundedDeviceQueue::pop(ProbeDeviceWrite& write) noexcept {
  if (size_ == 0U) {
    return false;
  }
  write = writes_[head_];
  head_ = (head_ + 1U) % writes_.size();
  --size_;
  return true;
}

std::size_t BoundedDeviceQueue::size() const noexcept { return size_; }

std::size_t BoundedDeviceQueue::capacity() const noexcept { return writes_.size(); }

std::uint32_t BoundedDeviceQueue::overflow_rejections() const noexcept {
  return overflow_rejections_;
}

bool DeviceQueueProbeResult::passed() const noexcept {
  return accepted_writes == kDeviceQueueCapacity && drained_writes == accepted_writes &&
         overflow_rejections == 1U && fifo_ordered;
}

InteractiveCommandProbe::InteractiveCommandProbe() {
  const auto open = core_.submit(make_command(
      next_command_id_++, contracts::OpenLibrary{"m0:library"}, core_.latest().sequence));
  const auto load = core_.submit(make_command(
      next_command_id_++, contracts::LoadTrack{contracts::TrackId{1}}, core_.latest().sequence));
  clock_tick_ = runtime::kSnapshotCadenceTicks;
  const auto advanced = core_.advance_to(clock_tick_);
  const auto snapshot = core_.latest();
  ready_ = open.outcome == contracts::CommandOutcome::Accepted &&
           load.outcome == contracts::CommandOutcome::Accepted &&
           advanced == runtime::AdvanceResult::Ok &&
           snapshot.transport == contracts::TransportState::Stopped;
}

bool InteractiveCommandProbe::ready() const noexcept { return ready_; }

bool InteractiveCommandProbe::completed() const noexcept {
  return ready_ && !failed_ && completed_steps_ == kInteractiveActions.size();
}

bool InteractiveCommandProbe::failed() const noexcept { return failed_; }

std::size_t InteractiveCommandProbe::completed_steps() const noexcept { return completed_steps_; }

std::optional<ProbeAction> InteractiveCommandProbe::expected_action() const noexcept {
  if (!ready_ || failed_ || completed_steps_ >= kInteractiveActions.size()) {
    return std::nullopt;
  }
  return kInteractiveActions[completed_steps_];
}

contracts::PlayerSnapshot InteractiveCommandProbe::latest() const { return core_.latest(); }

InteractiveStepResult InteractiveCommandProbe::apply(const ProbeAction action) {
  InteractiveStepResult result;
  result.action = action;
  result.snapshot = core_.latest();
  const auto expected = expected_action();
  if (!expected.has_value()) {
    return result;
  }
  result.expected_action = *expected;
  result.expected = action == *expected;
  if (!result.expected) {
    return result;
  }

  result.command_result = core_.submit(
      make_command(next_command_id_++, action_command(action), result.snapshot.sequence));
  clock_tick_ += runtime::kSnapshotCadenceTicks;
  result.advance_result = core_.advance_to(clock_tick_);
  result.snapshot = core_.latest();
  result.passed = result.command_result.outcome == contracts::CommandOutcome::Accepted &&
                  result.command_result.reason == contracts::CommandReason::None &&
                  result.advance_result == runtime::AdvanceResult::Ok &&
                  result.snapshot.transport == kInteractiveTransports[completed_steps_];
  if (result.passed) {
    ++completed_steps_;
  } else {
    failed_ = true;
  }
  return result;
}

bool ProbeRunResult::passed() const noexcept {
  if (!execution_ok || schema_version != contracts::kSchemaVersion ||
      blob_size != kSyntheticBlobSize || snapshot_count < kProbeMinimumSnapshots ||
      event_count == 0U || final_transport != contracts::TransportState::Stopped ||
      final_position_ticks != 0U || duplicate_outcome != contracts::CommandOutcome::Duplicate ||
      duplicate_reason != contracts::CommandReason::DuplicateCommandId ||
      stale_reason != contracts::CommandReason::StaleCommandId ||
      overflow_reason != contracts::CommandReason::QueueFull) {
    return false;
  }
  for (const auto& check : storage_checks) {
    if (check.read_succeeded != check.expected_success || !check.content_matches) {
      return false;
    }
  }
  return true;
}

ProbeRunResult run_comparison_probe(IProbeBlobReader& blob_reader, IProbeRenderer* renderer) {
  ProbeRunResult result;
  result.renderer_enabled = renderer != nullptr;
  result.execution_ok = check_storage(blob_reader, result);

  EventRecorder events;
  SnapshotRecorder snapshots(renderer);
  runtime::MockCore core(&events, &snapshots);
  SemanticHash commands;

  const auto submit = [&core, &commands, &result](const contracts::PlayerCommand& command) {
    const auto command_result = core.submit(command);
    ++result.command_count;
    add_command_result(commands, command_result);
    result.execution_ok =
        result.execution_ok && command_result.outcome == contracts::CommandOutcome::Accepted;
  };

  submit(make_command(1, contracts::OpenLibrary{"m0:library"}, 0));
  submit(make_command(2, contracts::LoadTrack{contracts::TrackId{1}}, 0));
  result.execution_ok = result.execution_ok && advance(core, 1'000);
  submit(make_command(3, contracts::Play{}, 1));
  result.execution_ok = result.execution_ok && advance(core, 60'000);
  submit(make_command(4, contracts::Pause{}, 60));
  result.execution_ok = result.execution_ok && advance(core, 90'000);
  submit(make_command(5, contracts::Resume{}, 90));
  result.execution_ok = result.execution_ok && advance(core, 150'000);
  submit(make_command(6, contracts::SetChannelMute{contracts::ChannelId{0}, true}, 150));
  submit(make_command(7, contracts::SetChannelSolo{contracts::ChannelId{1}, true}, 150));
  submit(make_command(8, contracts::ClearChannelOverrides{}, 150));
  const auto stop_command = make_command(9, contracts::Stop{}, 150);
  submit(stop_command);

  const auto duplicate = core.submit(stop_command);
  result.duplicate_outcome = duplicate.outcome;
  result.duplicate_reason = duplicate.reason;
  result.execution_ok = result.execution_ok && advance(core, 151'000);
  result.command_digest = commands.value();
  result.snapshot_count = snapshots.count();
  result.snapshot_digest = snapshots.digest();
  result.event_count = events.count();
  result.event_digest = events.digest();

  const auto latest = core.latest();
  result.final_snapshot_sequence = latest.sequence;
  result.final_position_ticks = latest.position.position_ticks;
  result.final_transport = latest.transport;

  runtime::MockCore stale_fixture;
  for (std::uint64_t id = 1; id <= 70; ++id) {
    static_cast<void>(stale_fixture.submit(make_command(id, contracts::Stop{})));
  }
  result.stale_reason = stale_fixture.submit(make_command(1, contracts::Stop{})).reason;

  runtime::MockCore queue_fixture;
  static_cast<void>(queue_fixture.submit(make_command(1, contracts::OpenLibrary{"m0:library"})));
  static_cast<void>(
      queue_fixture.submit(make_command(2, contracts::LoadTrack{contracts::TrackId{1}})));
  result.execution_ok = result.execution_ok && advance(queue_fixture, 1'000);
  for (std::uint64_t index = 0; index < runtime::kCommandQueueCapacity; ++index) {
    static_cast<void>(queue_fixture.submit(make_command(
        3U + index, contracts::SetChannelMute{contracts::ChannelId{0}, index % 2U == 0U})));
  }
  result.overflow_reason =
      queue_fixture
          .submit(make_command(3U + runtime::kCommandQueueCapacity,
                               contracts::SetChannelMute{contracts::ChannelId{0}, false}))
          .reason;

  return result;
}

ReadLatencyStats measure_read_latency(IProbeBlobReader& blob_reader,
                                      IProbeMonotonicClock& clock) noexcept {
  ReadLatencyStats result;
  result.iterations = kLatencyReadIterations;
  result.minimum_us = std::numeric_limits<std::uint32_t>::max();
  result.content_matches = blob_reader.size() == kSyntheticBlobSize;
  constexpr auto kOffsetRange = kSyntheticBlobSize - kLatencyReadSize + 1U;

  for (std::uint32_t iteration = 0; iteration < result.iterations; ++iteration) {
    const auto offset = (iteration * 127U) % kOffsetRange;
    std::array<std::uint8_t, kLatencyReadSize> bytes{};
    const auto started = clock.now_us();
    const auto read = blob_reader.read(offset, bytes.data(), kLatencyReadSize);
    const auto elapsed = clock.now_us() - started;
    result.total_us += elapsed;
    result.minimum_us = std::min(result.minimum_us, elapsed);
    result.maximum_us = std::max(result.maximum_us, elapsed);
    if (read) {
      ++result.successful_reads;
      for (std::uint32_t index = 0; index < kLatencyReadSize; ++index) {
        result.content_matches =
            result.content_matches && bytes[index] == synthetic_byte(offset + index);
      }
    } else {
      result.content_matches = false;
    }
  }
  if (result.iterations == 0U) {
    result.minimum_us = 0U;
  }
  return result;
}

TargetReadProfile measure_target_read_profile(IProbeTimedBlobReader& blob_reader,
                                              const std::uint32_t iterations,
                                              const std::uint32_t length,
                                              const ReadOffsetPattern pattern) noexcept {
  TargetReadProfile result;
  result.iterations = iterations;
  result.length = length;
  result.pattern = pattern;
  if (iterations == 0U || iterations > kTargetProfileMaxSamples || length == 0U ||
      length > kSyntheticBlobSize) {
    return result;
  }

  result.minimum_us = std::numeric_limits<std::uint32_t>::max();
  result.content_matches = true;
  const auto offset_range = kSyntheticBlobSize - length + 1U;
  std::array<std::uint32_t, kTargetProfileMaxSamples> samples{};
  std::array<std::uint8_t, kSyntheticBlobSize> bytes{};

  for (std::uint32_t iteration = 0; iteration < result.iterations; ++iteration) {
    const auto offset =
        pattern == ReadOffsetPattern::Fixed ? 0U : (iteration * 127U) % offset_range;
    std::uint32_t elapsed_us{};
    const auto read = blob_reader.read_timed(offset, bytes.data(), length, elapsed_us);
    samples[iteration] = elapsed_us;
    result.total_us += elapsed_us;
    result.minimum_us = std::min(result.minimum_us, elapsed_us);
    result.maximum_us = std::max(result.maximum_us, elapsed_us);
    result.at_or_above_1ms += elapsed_us >= 1'000U ? 1U : 0U;
    result.at_or_above_2ms += elapsed_us >= 2'000U ? 1U : 0U;
    if (read) {
      ++result.successful_reads;
      for (std::uint32_t index = 0; index < length; ++index) {
        result.content_matches =
            result.content_matches && bytes[index] == synthetic_byte(offset + index);
      }
    } else {
      result.content_matches = false;
    }
  }

  std::sort(samples.begin(), samples.begin() + iterations);
  const auto percentile = [&samples, iterations](const std::uint32_t percent) {
    const auto rank = (percent * iterations + 99U) / 100U;
    return samples[rank - 1U];
  };
  result.percentile_50_us = percentile(50U);
  result.percentile_90_us = percentile(90U);
  result.percentile_95_us = percentile(95U);
  result.percentile_99_us = percentile(99U);
  return result;
}

DeviceQueueProbeResult run_device_queue_probe(IProbeDeviceObserver* const observer) noexcept {
  DeviceQueueProbeResult result;
  result.observer_enabled = observer != nullptr;
  result.fifo_ordered = true;
  BoundedDeviceQueue queue;

  for (std::size_t index = 0; index < queue.capacity(); ++index) {
    const ProbeDeviceWrite write{1'000U * index, 1U, static_cast<std::uint8_t>(index),
                                 static_cast<std::uint8_t>(0x80U + index)};
    if (queue.push(write)) {
      ++result.accepted_writes;
    }
  }
  const ProbeDeviceWrite overflow{9'000U, 1U, 0x20U, 0xFFU};
  static_cast<void>(queue.push(overflow));
  result.overflow_rejections = queue.overflow_rejections();

  SemanticHash digest;
  ProbeDeviceWrite write;
  while (queue.pop(write)) {
    const auto index = result.drained_writes;
    const ProbeDeviceWrite expected{1'000ULL * index, 1U, static_cast<std::uint8_t>(index),
                                    static_cast<std::uint8_t>(0x80U + index)};
    result.fifo_ordered = result.fifo_ordered && write == expected;
    digest.add_integer(write.at_tick);
    digest.add_integer(write.device_id);
    digest.add_integer(write.address);
    digest.add_integer(write.value);
    if (observer != nullptr) {
      observer->observe(write);
    }
    ++result.drained_writes;
  }
  result.write_digest = digest.value();
  return result;
}

bool equivalent_device_queue_semantics(const DeviceQueueProbeResult& left,
                                       const DeviceQueueProbeResult& right) noexcept {
  return left.accepted_writes == right.accepted_writes &&
         left.drained_writes == right.drained_writes &&
         left.overflow_rejections == right.overflow_rejections &&
         left.write_digest == right.write_digest && left.fifo_ordered == right.fifo_ordered;
}

bool equivalent_semantics(const ProbeRunResult& left, const ProbeRunResult& right) noexcept {
  if (left.schema_version != right.schema_version || left.blob_size != right.blob_size ||
      left.command_count != right.command_count || left.command_digest != right.command_digest ||
      left.snapshot_count != right.snapshot_count ||
      left.snapshot_digest != right.snapshot_digest || left.event_count != right.event_count ||
      left.event_digest != right.event_digest ||
      left.final_snapshot_sequence != right.final_snapshot_sequence ||
      left.final_position_ticks != right.final_position_ticks ||
      left.final_transport != right.final_transport ||
      left.duplicate_outcome != right.duplicate_outcome ||
      left.duplicate_reason != right.duplicate_reason || left.stale_reason != right.stale_reason ||
      left.overflow_reason != right.overflow_reason || left.execution_ok != right.execution_ok) {
    return false;
  }
  for (std::size_t index = 0; index < left.storage_checks.size(); ++index) {
    if (!storage_equal(left.storage_checks[index], right.storage_checks[index])) {
      return false;
    }
  }
  return true;
}

bool matches_golden(const ProbeRunResult& result) noexcept {
  return result.passed() && result.command_count == kGoldenCommandCount &&
         result.command_digest == kGoldenCommandDigest &&
         result.snapshot_count == kGoldenSnapshotCount &&
         result.snapshot_digest == kGoldenSnapshotDigest &&
         result.event_count == kGoldenEventCount && result.event_digest == kGoldenEventDigest &&
         result.final_snapshot_sequence == kGoldenFinalSnapshotSequence;
}

} // namespace rpcmp::spike
