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

  return suite.finish("pocket comparison probe");
}
