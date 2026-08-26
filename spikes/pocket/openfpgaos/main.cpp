#include "rpcmp/spike/comparison_probe.hpp"

#include <array>
#include <cstdint>

namespace {

class SyntheticBlob final : public rpcmp::spike::IProbeBlobReader {
public:
  SyntheticBlob() {
    for (std::uint32_t offset = 0; offset < bytes_.size(); ++offset) {
      bytes_[offset] = rpcmp::spike::synthetic_byte(offset);
    }
  }

  std::uint32_t size() const noexcept override { return bytes_.size(); }

  bool read(const std::uint32_t offset, std::uint8_t* const destination,
            const std::uint32_t length) noexcept override {
    if (destination == nullptr || offset > bytes_.size() || length > bytes_.size() - offset) {
      return false;
    }
    for (std::uint32_t index = 0; index < length; ++index) {
      destination[index] = bytes_[offset + index];
    }
    return true;
  }

private:
  std::array<std::uint8_t, rpcmp::spike::kSyntheticBlobSize> bytes_{};
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
  SyntheticBlob blob;
  SnapshotObserver observer;
  const auto observed = rpcmp::spike::run_comparison_probe(blob, &observer);
  const auto headless = rpcmp::spike::run_comparison_probe(blob);
  const bool passed = observed.passed() && headless.passed() &&
                      rpcmp::spike::equivalent_semantics(observed, headless) &&
                      observer.last_sequence() == observed.final_snapshot_sequence;
  return passed ? 0 : 1;
}
