#include "rpcmp/spike/comparison_probe.hpp"
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
