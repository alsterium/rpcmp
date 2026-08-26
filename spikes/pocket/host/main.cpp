#include "rpcmp/spike/comparison_probe.hpp"

#include <cstdint>
#include <iostream>

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

class CountingRenderer final : public rpcmp::spike::IProbeRenderer {
public:
  void render(const rpcmp::contracts::PlayerSnapshot& snapshot) override {
    ++count_;
    work_ ^= snapshot.sequence + snapshot.position.position_ticks;
    for (const auto& channel : snapshot.channels) {
      work_ ^= static_cast<std::uint64_t>(channel.activity) << (channel.channel_id.value % 8U);
    }
  }

  std::uint64_t count() const noexcept { return count_; }
  std::uint64_t work() const noexcept { return work_; }

private:
  std::uint64_t count_{};
  std::uint64_t work_{};
};

} // namespace

int main() {
  SyntheticBlobReader blob;
  CountingRenderer renderer;
  const auto observed = rpcmp::spike::run_comparison_probe(blob, &renderer);
  const auto headless = rpcmp::spike::run_comparison_probe(blob);
  const auto passed = observed.passed() && headless.passed() &&
                      rpcmp::spike::equivalent_semantics(observed, headless) &&
                      renderer.count() == observed.snapshot_count;

  std::cout << "schema=" << observed.schema_version << " snapshots=" << observed.snapshot_count
            << " snapshot_digest=" << observed.snapshot_digest << " events=" << observed.event_count
            << " event_digest=" << observed.event_digest << " commands=" << observed.command_count
            << " command_digest=" << observed.command_digest << " renderer_work=" << renderer.work()
            << '\n';
  for (const auto& check : observed.storage_checks) {
    std::cout << "storage offset=" << check.offset << " length=" << check.length
              << " expected=" << check.expected_success << " read=" << check.read_succeeded
              << " content=" << check.content_matches << " checksum=" << check.checksum << '\n';
  }
  std::cout << "final_sequence=" << observed.final_snapshot_sequence
            << " final_position=" << observed.final_position_ticks
            << " duplicate_reason=" << static_cast<unsigned>(observed.duplicate_reason)
            << " stale_reason=" << static_cast<unsigned>(observed.stale_reason)
            << " overflow_reason=" << static_cast<unsigned>(observed.overflow_reason) << '\n';
  std::cout << "pocket comparison probe: " << (passed ? "PASS" : "FAIL") << '\n';
  return passed ? 0 : 1;
}
