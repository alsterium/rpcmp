#include "rpcmp/spike/comparison_probe.hpp"
#include "rpcmp/spike/mdx_hardware_probe.hpp"

int of_file_read(std::uint32_t slot_id, std::uint32_t offset, void* destination,
                 std::uint32_t length);
long of_file_size(std::uint32_t slot_id);

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

constexpr std::uint32_t kSyntheticSlot = 4;
bool write_synthetic_slot() {
  const char* const data_directory = std::getenv("OF_DATA_DIR");
  if (data_directory == nullptr) {
    return false;
  }

  char path[512]{};
  const int path_length =
      std::snprintf(path, sizeof(path), "%s/%u.bin", data_directory, kSyntheticSlot);
  if (path_length < 0 || static_cast<std::size_t>(path_length) >= sizeof(path)) {
    return false;
  }

  std::FILE* const file = std::fopen(path, "wb");
  if (file == nullptr) {
    return false;
  }
  bool written = true;
  for (std::uint32_t offset = 0; offset < rpcmp::spike::kSyntheticBlobSize; ++offset) {
    const auto byte = rpcmp::spike::synthetic_byte(offset);
    written = written && std::fwrite(&byte, sizeof(byte), 1, file) == 1;
  }
  return std::fclose(file) == 0 && written;
}

class DataSlotBlob final : public rpcmp::spike::IProbeBlobReader {
public:
  DataSlotBlob() {
    const long measured = of_file_size(kSyntheticSlot);
    if (measured >= 0 &&
        static_cast<unsigned long>(measured) <= std::numeric_limits<std::uint32_t>::max()) {
      size_ = static_cast<std::uint32_t>(measured);
      valid_ = true;
    }
  }

  std::uint32_t size() const noexcept override { return size_; }

  bool read(const std::uint32_t offset, std::uint8_t* const destination,
            const std::uint32_t length) noexcept override {
    if (!valid_ || destination == nullptr || offset > size_ || length > size_ - offset) {
      return false;
    }
    return of_file_read(kSyntheticSlot, offset, destination, length) == 0;
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

} // namespace

int main() {
  if (!write_synthetic_slot()) {
    std::fprintf(stderr, "openfpgaOS desktop shim setup failed (errno=%d)\n", errno);
    return 1;
  }

  DataSlotBlob blob;
  SnapshotObserver observer;
  const auto observed = rpcmp::spike::run_comparison_probe(blob, &observer);
  const auto headless = rpcmp::spike::run_comparison_probe(blob);
  const auto workload = rpcmp::spike::run_runtime_workload();
  const auto mdx = rpcmp::spike::run_mdx_hardware_probe(nullptr, 0);
  const bool passed = rpcmp::spike::matches_golden(observed) &&
                      rpcmp::spike::matches_golden(headless) &&
                      rpcmp::spike::equivalent_semantics(observed, headless) &&
                      observer.last_sequence() == observed.final_snapshot_sequence &&
                      workload.passed() && mdx.passed();

  std::printf("snapshots=%llu\n", static_cast<unsigned long long>(observed.snapshot_count));
  std::printf("snapshot_digest=%llu\n", static_cast<unsigned long long>(observed.snapshot_digest));
  std::printf("events=%llu\n", static_cast<unsigned long long>(observed.event_count));
  std::printf("event_digest=%llu\n", static_cast<unsigned long long>(observed.event_digest));
  std::printf("commands=%llu\n", static_cast<unsigned long long>(observed.command_count));
  std::printf("command_digest=%llu\n", static_cast<unsigned long long>(observed.command_digest));
  std::printf("workload_snapshot_digest=%llu\n",
              static_cast<unsigned long long>(workload.snapshot_digest));
  std::printf("workload_event_digest=%llu\n",
              static_cast<unsigned long long>(workload.event_digest));
  std::printf("workload_write_digest=%llu\n",
              static_cast<unsigned long long>(workload.write_digest));
  std::printf("mdx_ticks=%lu\n", static_cast<unsigned long>(mdx.driver_ticks));
  std::printf("mdx_writes=%lu\n", static_cast<unsigned long>(mdx.writes));
  std::printf("mdx_end_tick=%llu\n", static_cast<unsigned long long>(mdx.end_tick));
  std::printf("mdx_digest=%llu\n", static_cast<unsigned long long>(mdx.write_digest));
  std::printf("result=%s\n", passed ? "PASS" : "FAIL");
  return passed ? 0 : 1;
}
