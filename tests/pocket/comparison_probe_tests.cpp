#include "rpcmp/spike/comparison_probe.hpp"
#include "rpcmp/spike/mdx_hardware_probe.hpp"
#include "test_support.hpp"

#include <cstdint>

namespace {

class SyntheticBlobReader final : public rpcmp::spike::IProbeBlobReader {
public:
  std::uint32_t size() const noexcept override { return rpcmp::spike::kSyntheticBlobSize; }

  bool read(const std::uint32_t offset, std::uint8_t* destination,
            const std::uint32_t length) noexcept override {
    if (destination == nullptr || offset > size() || length > size() - offset) {
      return false;
    }
    for (std::uint32_t index = 0; index < length; ++index) {
      destination[index] = rpcmp::spike::synthetic_byte(offset + index);
    }
    return true;
  }
};

class ExercisingRenderer final : public rpcmp::spike::IProbeRenderer {
public:
  void render(const rpcmp::contracts::PlayerSnapshot& snapshot) override {
    ++count_;
    accumulator_ ^= snapshot.sequence;
    for (const auto& channel : snapshot.channels) {
      accumulator_ += channel.activity;
    }
  }

  std::uint64_t count() const noexcept { return count_; }
  std::uint64_t accumulator() const noexcept { return accumulator_; }

private:
  std::uint64_t count_{};
  std::uint64_t accumulator_{};
};

class CorruptBlobReader final : public rpcmp::spike::IProbeBlobReader {
public:
  std::uint32_t size() const noexcept override { return rpcmp::spike::kSyntheticBlobSize; }

  bool read(const std::uint32_t offset, std::uint8_t* destination,
            const std::uint32_t length) noexcept override {
    if (destination == nullptr || offset > size() || length > size() - offset) {
      return false;
    }
    for (std::uint32_t index = 0; index < length; ++index) {
      destination[index] = 0U;
    }
    return true;
  }
};

class FakeClock final : public rpcmp::spike::IProbeMonotonicClock {
public:
  std::uint32_t now_us() noexcept override { return now_us_; }
  void advance(const std::uint32_t duration_us) noexcept { now_us_ += duration_us; }

private:
  std::uint32_t now_us_{0xFFFFFFF0U};
};

class SteppingClock final : public rpcmp::spike::IProbeMonotonicClock {
public:
  std::uint32_t now_us() noexcept override {
    const auto value = now_us_;
    now_us_ += 100U;
    return value;
  }

private:
  std::uint32_t now_us_{0xFFFFFF00U};
};

class TimedBlobReader final : public rpcmp::spike::IProbeBlobReader {
public:
  explicit TimedBlobReader(FakeClock& clock) : clock_(clock) {}

  std::uint32_t size() const noexcept override { return rpcmp::spike::kSyntheticBlobSize; }

  bool read(const std::uint32_t offset, std::uint8_t* destination,
            const std::uint32_t length) noexcept override {
    if (destination == nullptr || offset > size() || length > size() - offset) {
      return false;
    }
    for (std::uint32_t index = 0; index < length; ++index) {
      destination[index] = rpcmp::spike::synthetic_byte(offset + index);
    }
    clock_.advance(10U + calls_++);
    return true;
  }

private:
  FakeClock& clock_;
  std::uint32_t calls_{};
};

class TargetTimedBlobReader final : public rpcmp::spike::IProbeTimedBlobReader {
public:
  bool read_timed(const std::uint32_t offset, std::uint8_t* const destination,
                  const std::uint32_t length, std::uint32_t& elapsed_us) noexcept override {
    if (destination == nullptr || offset > rpcmp::spike::kSyntheticBlobSize ||
        length > rpcmp::spike::kSyntheticBlobSize - offset) {
      return false;
    }
    for (std::uint32_t index = 0; index < length; ++index) {
      destination[index] = rpcmp::spike::synthetic_byte(offset + index);
    }
    elapsed_us = 20U * ++calls_;
    return true;
  }

private:
  std::uint32_t calls_{};
};

class DeviceObserver final : public rpcmp::spike::IProbeDeviceObserver {
public:
  void observe(const rpcmp::spike::ProbeDeviceWrite&) override { ++count_; }
  std::uint32_t count() const noexcept { return count_; }

private:
  std::uint32_t count_{};
};

} // namespace

int main() {
  rpcmp::test::Suite suite;
  SyntheticBlobReader blob;
  ExercisingRenderer renderer;

  const auto observed = rpcmp::spike::run_comparison_probe(blob, &renderer);
  const auto headless = rpcmp::spike::run_comparison_probe(blob);
  RPCMP_CHECK(suite, observed.passed());
  RPCMP_CHECK(suite, headless.passed());
  RPCMP_CHECK(suite, rpcmp::spike::matches_golden(observed));
  RPCMP_CHECK(suite, rpcmp::spike::matches_golden(headless));
  RPCMP_CHECK(suite, rpcmp::spike::equivalent_semantics(observed, headless));
  RPCMP_CHECK(suite, observed.renderer_enabled);
  RPCMP_CHECK(suite, !headless.renderer_enabled);
  RPCMP_CHECK(suite, renderer.count() == observed.snapshot_count);
  RPCMP_CHECK(suite, renderer.accumulator() != 0U);
  RPCMP_CHECK(suite, observed.snapshot_count >= rpcmp::spike::kProbeMinimumSnapshots);
  RPCMP_CHECK(suite, observed.event_count != 0U);
  RPCMP_CHECK(suite, observed.snapshot_count == rpcmp::spike::kGoldenSnapshotCount);
  RPCMP_CHECK(suite, observed.snapshot_digest == rpcmp::spike::kGoldenSnapshotDigest);
  RPCMP_CHECK(suite, observed.event_count == rpcmp::spike::kGoldenEventCount);
  RPCMP_CHECK(suite, observed.event_digest == rpcmp::spike::kGoldenEventDigest);
  RPCMP_CHECK(suite, observed.command_count == rpcmp::spike::kGoldenCommandCount);
  RPCMP_CHECK(suite, observed.command_digest == rpcmp::spike::kGoldenCommandDigest);
  RPCMP_CHECK(suite,
              observed.final_snapshot_sequence == rpcmp::spike::kGoldenFinalSnapshotSequence);
  RPCMP_CHECK(suite,
              observed.duplicate_reason == rpcmp::contracts::CommandReason::DuplicateCommandId);
  RPCMP_CHECK(suite, observed.stale_reason == rpcmp::contracts::CommandReason::StaleCommandId);
  RPCMP_CHECK(suite, observed.overflow_reason == rpcmp::contracts::CommandReason::QueueFull);

  CorruptBlobReader corrupt;
  const auto corrupt_result = rpcmp::spike::run_comparison_probe(corrupt);
  RPCMP_CHECK(suite, !corrupt_result.passed());
  RPCMP_CHECK(suite, !rpcmp::spike::matches_golden(corrupt_result));
  RPCMP_CHECK(suite, !corrupt_result.execution_ok);

  FakeClock clock;
  TimedBlobReader timed_blob(clock);
  const auto latency = rpcmp::spike::measure_read_latency(timed_blob, clock);
  RPCMP_CHECK(suite, latency.passed());
  RPCMP_CHECK(suite, latency.iterations == rpcmp::spike::kLatencyReadIterations);
  RPCMP_CHECK(suite, latency.successful_reads == latency.iterations);
  RPCMP_CHECK(suite, latency.minimum_us == 10U);
  RPCMP_CHECK(suite, latency.maximum_us == 41U);
  RPCMP_CHECK(suite, latency.total_us == 816U);
  RPCMP_CHECK(suite, latency.average_us() == 25U);

  TargetTimedBlobReader target_blob;
  const auto target_profile = rpcmp::spike::measure_target_read_profile(
      target_blob, rpcmp::spike::kTargetProfileFullSamples, rpcmp::spike::kLatencyReadSize,
      rpcmp::spike::ReadOffsetPattern::Rotating);
  RPCMP_CHECK(suite, target_profile.passed());
  RPCMP_CHECK(suite, target_profile.minimum_us == 20U);
  RPCMP_CHECK(suite, target_profile.percentile_50_us == 2'560U);
  RPCMP_CHECK(suite, target_profile.percentile_90_us == 4'620U);
  RPCMP_CHECK(suite, target_profile.percentile_95_us == 4'880U);
  RPCMP_CHECK(suite, target_profile.percentile_99_us == 5'080U);
  RPCMP_CHECK(suite, target_profile.maximum_us == 5'120U);
  RPCMP_CHECK(suite, target_profile.total_us == 657'920U);
  RPCMP_CHECK(suite, target_profile.average_us() == 2'570U);
  RPCMP_CHECK(suite, target_profile.at_or_above_1ms == 207U);
  RPCMP_CHECK(suite, target_profile.at_or_above_2ms == 157U);
  const auto invalid_target_profile = rpcmp::spike::measure_target_read_profile(
      target_blob, rpcmp::spike::kTargetProfileMaxSamples + 1U, rpcmp::spike::kLatencyReadSize,
      rpcmp::spike::ReadOffsetPattern::Fixed);
  RPCMP_CHECK(suite, !invalid_target_profile.passed());

  DeviceObserver device_observer;
  const auto observed_queue = rpcmp::spike::run_device_queue_probe(&device_observer);
  const auto headless_queue = rpcmp::spike::run_device_queue_probe();
  RPCMP_CHECK(suite, observed_queue.passed());
  RPCMP_CHECK(suite, headless_queue.passed());
  RPCMP_CHECK(suite, device_observer.count() == rpcmp::spike::kDeviceQueueCapacity);
  RPCMP_CHECK(suite,
              rpcmp::spike::equivalent_device_queue_semantics(observed_queue, headless_queue));

  rpcmp::spike::BoundedDeviceQueue wrapped_queue;
  for (std::size_t index = 0; index < wrapped_queue.capacity(); ++index) {
    RPCMP_CHECK(suite, wrapped_queue.push({index, 1U, static_cast<std::uint8_t>(index), 0U}));
  }
  rpcmp::spike::ProbeDeviceWrite wrapped_write;
  for (std::size_t index = 0; index < wrapped_queue.capacity() / 2U; ++index) {
    RPCMP_CHECK(suite, wrapped_queue.pop(wrapped_write));
    RPCMP_CHECK(suite, wrapped_write.at_tick == index);
    RPCMP_CHECK(suite, wrapped_queue.push(
                           {wrapped_queue.capacity() + index, 1U,
                            static_cast<std::uint8_t>(wrapped_queue.capacity() + index), 0U}));
  }
  for (std::size_t index = wrapped_queue.capacity() / 2U;
       index < wrapped_queue.capacity() * 3U / 2U; ++index) {
    RPCMP_CHECK(suite, wrapped_queue.pop(wrapped_write));
    RPCMP_CHECK(suite, wrapped_write.at_tick == index);
  }
  RPCMP_CHECK(suite, wrapped_queue.size() == 0U);

  const auto headless_workload = rpcmp::spike::run_runtime_workload();
  ExercisingRenderer workload_renderer;
  const auto observed_workload = rpcmp::spike::run_runtime_workload(&workload_renderer);
  RPCMP_CHECK(suite, headless_workload.passed());
  RPCMP_CHECK(suite, observed_workload.passed());
  RPCMP_CHECK(suite,
              rpcmp::spike::equivalent_runtime_workload(headless_workload, observed_workload));
  RPCMP_CHECK(suite, workload_renderer.count() == rpcmp::spike::kRuntimeWorkloadSnapshots);
  SteppingClock workload_clock;
  const auto workload_profile = rpcmp::spike::measure_runtime_workload_profile(
      workload_clock, rpcmp::spike::kRuntimeWorkloadSamples);
  RPCMP_CHECK(suite, workload_profile.passed());
  RPCMP_CHECK(suite, workload_profile.successful_samples == rpcmp::spike::kRuntimeWorkloadSamples);
  RPCMP_CHECK(suite, workload_profile.minimum_us == 100U);
  RPCMP_CHECK(suite, workload_profile.average_us() == 100U);
  RPCMP_CHECK(suite, workload_profile.maximum_us == 100U);
  RPCMP_CHECK(suite, rpcmp::spike::equivalent_runtime_workload(workload_profile.reference,
                                                               headless_workload));
  const auto invalid_workload_profile = rpcmp::spike::measure_runtime_workload_profile(
      workload_clock, rpcmp::spike::kRuntimeWorkloadMaxSamples + 1U);
  RPCMP_CHECK(suite, !invalid_workload_profile.passed());

  const auto mdx = rpcmp::spike::run_mdx_hardware_probe(nullptr, 0);
  RPCMP_CHECK(suite, mdx.passed());
  RPCMP_CHECK(suite, mdx.driver_ticks == rpcmp::spike::kMdxHardwareProbeDriverTicks);
  RPCMP_CHECK(suite, mdx.writes == rpcmp::spike::kMdxHardwareProbeExpectedWrites);
  RPCMP_CHECK(suite, mdx.end_tick == rpcmp::spike::kMdxHardwareProbeExpectedEndTick);
  RPCMP_CHECK(suite, mdx.write_digest == rpcmp::spike::kMdxHardwareProbeExpectedDigest);

  rpcmp::spike::InteractiveCommandProbe input_probe;
  RPCMP_CHECK(suite, input_probe.ready());
  RPCMP_CHECK(suite, !input_probe.completed());
  RPCMP_CHECK(suite, input_probe.expected_action() == rpcmp::spike::ProbeAction::Play);
  const auto unexpected = input_probe.apply(rpcmp::spike::ProbeAction::Stop);
  RPCMP_CHECK(suite, !unexpected.expected);
  RPCMP_CHECK(suite, !unexpected.passed);
  RPCMP_CHECK(suite, input_probe.completed_steps() == 0U);

  const auto play = input_probe.apply(rpcmp::spike::ProbeAction::Play);
  RPCMP_CHECK(suite, play.passed);
  RPCMP_CHECK(suite, play.snapshot.transport == rpcmp::contracts::TransportState::Playing);
  const auto pause = input_probe.apply(rpcmp::spike::ProbeAction::TogglePause);
  RPCMP_CHECK(suite, pause.passed);
  RPCMP_CHECK(suite, pause.snapshot.transport == rpcmp::contracts::TransportState::Paused);
  const auto resume = input_probe.apply(rpcmp::spike::ProbeAction::TogglePause);
  RPCMP_CHECK(suite, resume.passed);
  RPCMP_CHECK(suite, resume.snapshot.transport == rpcmp::contracts::TransportState::Playing);
  const auto stop = input_probe.apply(rpcmp::spike::ProbeAction::Stop);
  RPCMP_CHECK(suite, stop.passed);
  RPCMP_CHECK(suite, stop.snapshot.transport == rpcmp::contracts::TransportState::Stopped);
  RPCMP_CHECK(suite, input_probe.completed());
  RPCMP_CHECK(suite, !input_probe.failed());
  RPCMP_CHECK(suite, !input_probe.expected_action().has_value());

  return suite.finish("pocket comparison probe");
}
