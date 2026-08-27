#include "rpcmp/spike/comparison_probe.hpp"

#include <cstdint>
#include <cstdio>

extern "C" {
int rpcmp_pocket_slot_size(std::uint32_t slot_id, std::uint32_t* size);
int rpcmp_pocket_slot_read(std::uint32_t slot_id, std::uint32_t offset, void* destination,
                           std::uint32_t length);
void rpcmp_pocket_terminal_init();
[[noreturn]] void rpcmp_pocket_hold_result();
}

namespace {

constexpr std::uint32_t kSyntheticSlot = 4;

class DataSlotBlob final : public rpcmp::spike::IProbeBlobReader {
public:
  DataSlotBlob() { valid_ = rpcmp_pocket_slot_size(kSyntheticSlot, &size_) == 0; }

  std::uint32_t size() const noexcept override { return size_; }

  bool read(const std::uint32_t offset, std::uint8_t* const destination,
            const std::uint32_t length) noexcept override {
    if (!valid_ || destination == nullptr || offset > size_ || length > size_ - offset) {
      return false;
    }
    return rpcmp_pocket_slot_read(kSyntheticSlot, offset, destination, length) == 0;
  }

private:
  std::uint32_t size_{};
  bool valid_{};
};

class SnapshotObserver final : public rpcmp::spike::IProbeRenderer {
public:
  void render(const rpcmp::contracts::PlayerSnapshot& snapshot) override {
    last_sequence_ = snapshot.sequence;
  }

  std::uint64_t last_sequence() const noexcept { return last_sequence_; }

private:
  std::uint64_t last_sequence_{};
};

void print_result(const rpcmp::spike::ProbeRunResult& result, const bool passed) {
  std::printf("RPCMP openfpgaOS probe\n\n");
  std::printf("slot4_bytes=%lu\n", static_cast<unsigned long>(result.blob_size));
  for (std::size_t index = 0; index < result.storage_checks.size(); ++index) {
    const auto& check = result.storage_checks[index];
    std::printf("read%u off=%lu %s sum=%lu\n", static_cast<unsigned>(index),
                static_cast<unsigned long>(check.offset),
                check.read_succeeded == check.expected_success && check.content_matches ? "OK"
                                                                                        : "FAIL",
                static_cast<unsigned long>(check.checksum));
  }
  std::printf("commands=%llu digest=%llu\n", static_cast<unsigned long long>(result.command_count),
              static_cast<unsigned long long>(result.command_digest));
  std::printf("snapshots=%llu digest=%llu\n",
              static_cast<unsigned long long>(result.snapshot_count),
              static_cast<unsigned long long>(result.snapshot_digest));
  std::printf("events=%llu digest=%llu\n", static_cast<unsigned long long>(result.event_count),
              static_cast<unsigned long long>(result.event_digest));
  std::printf("final_sequence=%llu\n",
              static_cast<unsigned long long>(result.final_snapshot_sequence));
  std::printf("\nRESULT: %s\n", passed ? "PASS" : "FAIL");
  std::printf("Open the Pocket menu to exit.\n");
}

} // namespace

int main() {
  rpcmp_pocket_terminal_init();

  DataSlotBlob blob;
  SnapshotObserver observer;
  const auto observed = rpcmp::spike::run_comparison_probe(blob, &observer);
  const auto headless = rpcmp::spike::run_comparison_probe(blob);
  const bool passed = rpcmp::spike::matches_golden(observed) &&
                      rpcmp::spike::matches_golden(headless) &&
                      rpcmp::spike::equivalent_semantics(observed, headless) &&
                      observer.last_sequence() == observed.final_snapshot_sequence;

  print_result(observed, passed);
  rpcmp_pocket_hold_result();
}
