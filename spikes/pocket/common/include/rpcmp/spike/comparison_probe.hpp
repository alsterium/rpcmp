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
inline constexpr std::uint64_t kGoldenCommandCount = 9;
inline constexpr std::uint64_t kGoldenCommandDigest = 2'446'879'228'733'133'299ULL;
inline constexpr std::uint64_t kGoldenSnapshotCount = 151;
inline constexpr std::uint64_t kGoldenSnapshotDigest = 6'832'192'089'657'554'689ULL;
inline constexpr std::uint64_t kGoldenEventCount = 154;
inline constexpr std::uint64_t kGoldenEventDigest = 16'311'210'033'269'188'847ULL;
inline constexpr std::uint64_t kGoldenFinalSnapshotSequence = 151;
inline constexpr std::uint32_t kLatencyReadIterations = 32;
inline constexpr std::uint32_t kLatencyReadSize = 16;

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

class IProbeMonotonicClock {
public:
  virtual ~IProbeMonotonicClock() = default;
  virtual std::uint32_t now_us() noexcept = 0;
};

struct ReadLatencyStats {
  std::uint32_t iterations{};
  std::uint32_t successful_reads{};
  std::uint32_t minimum_us{};
  std::uint32_t maximum_us{};
  std::uint64_t total_us{};
  bool content_matches{};

  std::uint32_t average_us() const noexcept;
  bool passed() const noexcept;
};

enum class ProbeAction : std::uint8_t { Play, TogglePause, Stop };

struct InteractiveStepResult {
  ProbeAction action{ProbeAction::Stop};
  ProbeAction expected_action{ProbeAction::Play};
  contracts::CommandResult command_result{};
  runtime::AdvanceResult advance_result{runtime::AdvanceResult::Ok};
  contracts::PlayerSnapshot snapshot{};
  bool expected{};
  bool passed{};
};

class InteractiveCommandProbe final {
public:
  InteractiveCommandProbe();

  bool ready() const noexcept;
  bool completed() const noexcept;
  bool failed() const noexcept;
  std::size_t completed_steps() const noexcept;
  std::optional<ProbeAction> expected_action() const noexcept;
  contracts::PlayerSnapshot latest() const;
  InteractiveStepResult apply(ProbeAction action);

private:
  runtime::MockCore core_;
  std::uint64_t clock_tick_{};
  std::uint64_t next_command_id_{1};
  std::size_t completed_steps_{};
  bool ready_{};
  bool failed_{};
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
ReadLatencyStats measure_read_latency(IProbeBlobReader& blob_reader,
                                      IProbeMonotonicClock& clock) noexcept;
bool equivalent_semantics(const ProbeRunResult& left, const ProbeRunResult& right) noexcept;
bool matches_golden(const ProbeRunResult& result) noexcept;

} // namespace rpcmp::spike

#endif // RPCMP_SPIKE_COMPARISON_PROBE_HPP
