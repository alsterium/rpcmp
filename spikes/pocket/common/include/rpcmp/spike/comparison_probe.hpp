#ifndef RPCMP_SPIKE_COMPARISON_PROBE_HPP
#define RPCMP_SPIKE_COMPARISON_PROBE_HPP

#include "rpcmp/runtime/mock_core.hpp"

#include <array>
#include <cstdint>

namespace rpcmp::spike {

inline constexpr std::uint32_t kSyntheticBlobSize = 4'096;
inline constexpr std::uint32_t kSyntheticReadSize = 16;
inline constexpr std::size_t kStorageCheckCount = 4;
inline constexpr std::uint64_t kProbeMinimumSnapshots = 120;

constexpr std::uint8_t synthetic_byte(const std::uint32_t offset) noexcept {
  return static_cast<std::uint8_t>((offset * 37U + 11U) & 0xFFU);
}

class IProbeBlobReader {
public:
  virtual ~IProbeBlobReader() = default;
  virtual std::uint32_t size() const noexcept = 0;
  virtual bool read(std::uint32_t offset, std::uint8_t* destination,
                    std::uint32_t length) noexcept = 0;
};

class IProbeRenderer {
public:
  virtual ~IProbeRenderer() = default;
  virtual void render(const contracts::PlayerSnapshot& snapshot) = 0;
};

struct StorageCheck {
  std::uint32_t offset{};
  std::uint32_t length{};
  std::uint32_t checksum{};
  bool expected_success{};
  bool read_succeeded{};
  bool content_matches{};
};

struct ProbeRunResult {
  std::uint16_t schema_version{contracts::kSchemaVersion};
  std::uint32_t blob_size{};
  std::array<StorageCheck, kStorageCheckCount> storage_checks{};
  std::uint64_t command_count{};
  std::uint64_t command_digest{};
  std::uint64_t snapshot_count{};
  std::uint64_t snapshot_digest{};
  std::uint64_t event_count{};
  std::uint64_t event_digest{};
  std::uint64_t final_snapshot_sequence{};
  std::uint64_t final_position_ticks{};
  contracts::TransportState final_transport{contracts::TransportState::Empty};
  contracts::CommandOutcome duplicate_outcome{contracts::CommandOutcome::Rejected};
  contracts::CommandReason duplicate_reason{contracts::CommandReason::None};
  contracts::CommandReason stale_reason{contracts::CommandReason::None};
  contracts::CommandReason overflow_reason{contracts::CommandReason::None};
  bool renderer_enabled{};
  bool execution_ok{};

  bool passed() const noexcept;
};

ProbeRunResult run_comparison_probe(IProbeBlobReader& blob_reader,
                                    IProbeRenderer* renderer = nullptr);
bool equivalent_semantics(const ProbeRunResult& left, const ProbeRunResult& right) noexcept;

} // namespace rpcmp::spike

#endif // RPCMP_SPIKE_COMPARISON_PROBE_HPP
