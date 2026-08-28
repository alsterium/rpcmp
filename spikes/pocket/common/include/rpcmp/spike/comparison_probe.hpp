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
inline constexpr std::size_t kDeviceQueueCapacity = 8;
inline constexpr std::uint32_t kTargetProfileMaxSamples = 256;
inline constexpr std::uint32_t kTargetProfileFullSamples = 256;
inline constexpr std::uint32_t kTargetProfileScaleSamples = 64;
inline constexpr std::uint32_t kRuntimeWorkloadMaxSamples = 64;
inline constexpr std::uint32_t kRuntimeWorkloadSamples = 32;
inline constexpr std::uint32_t kRuntimeWorkloadSnapshots = 120;
inline constexpr std::uint32_t kRuntimeWorkloadWritesPerSnapshot = 8;

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

class IProbeTimedBlobReader {
public:
  virtual ~IProbeTimedBlobReader() = default;
  virtual bool read_timed(std::uint32_t offset, std::uint8_t* destination, std::uint32_t length,
                          std::uint32_t& elapsed_us) noexcept = 0;
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

enum class ReadOffsetPattern : std::uint8_t { Fixed, Rotating };

struct TargetReadProfile {
  std::uint32_t iterations{};
  std::uint32_t length{};
  ReadOffsetPattern pattern{ReadOffsetPattern::Fixed};
  std::uint32_t successful_reads{};
  std::uint32_t minimum_us{};
  std::uint32_t percentile_50_us{};
  std::uint32_t percentile_90_us{};
  std::uint32_t percentile_95_us{};
  std::uint32_t percentile_99_us{};
  std::uint32_t maximum_us{};
  std::uint64_t total_us{};
  std::uint32_t at_or_above_1ms{};
  std::uint32_t at_or_above_2ms{};
  bool content_matches{};

  std::uint32_t average_us() const noexcept;
  bool passed() const noexcept;
};

struct ProbeDeviceWrite {
  std::uint64_t at_tick{};
  std::uint16_t device_id{};
  std::uint8_t address{};
  std::uint8_t value{};

  friend bool operator==(const ProbeDeviceWrite& left, const ProbeDeviceWrite& right) noexcept {
    return left.at_tick == right.at_tick && left.device_id == right.device_id &&
           left.address == right.address && left.value == right.value;
  }
};

class BoundedDeviceQueue final {
public:
  bool push(const ProbeDeviceWrite& write) noexcept;
  bool pop(ProbeDeviceWrite& write) noexcept;
  std::size_t size() const noexcept;
  std::size_t capacity() const noexcept;
  std::uint32_t overflow_rejections() const noexcept;

private:
  std::array<ProbeDeviceWrite, kDeviceQueueCapacity> writes_{};
  std::size_t head_{};
  std::size_t tail_{};
  std::size_t size_{};
  std::uint32_t overflow_rejections_{};
};

class IProbeDeviceObserver {
public:
  virtual ~IProbeDeviceObserver() = default;
  virtual void observe(const ProbeDeviceWrite& write) = 0;
};

struct DeviceQueueProbeResult {
  std::uint32_t accepted_writes{};
  std::uint32_t drained_writes{};
  std::uint32_t overflow_rejections{};
  std::uint64_t write_digest{};
  bool observer_enabled{};
  bool fifo_ordered{};

  bool passed() const noexcept;
};

struct RuntimeWorkloadResult {
  std::uint32_t snapshot_count{};
  std::uint32_t event_count{};
  std::uint32_t device_writes{};
  std::uint64_t snapshot_digest{};
  std::uint64_t event_digest{};
  std::uint64_t write_digest{};
  std::uint64_t final_snapshot_sequence{};
  std::uint64_t final_position_ticks{};
  contracts::TransportState final_transport{contracts::TransportState::Empty};
  bool renderer_enabled{};
  bool execution_ok{};

  bool passed() const noexcept;
};

struct RuntimeWorkloadProfile {
  std::uint32_t samples{};
  std::uint32_t successful_samples{};
  std::uint32_t minimum_us{};
  std::uint32_t percentile_50_us{};
  std::uint32_t percentile_90_us{};
  std::uint32_t percentile_95_us{};
  std::uint32_t percentile_99_us{};
  std::uint32_t maximum_us{};
  std::uint64_t total_us{};
  RuntimeWorkloadResult reference{};
  bool deterministic{};

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
TargetReadProfile measure_target_read_profile(IProbeTimedBlobReader& blob_reader,
                                              std::uint32_t iterations, std::uint32_t length,
                                              ReadOffsetPattern pattern) noexcept;
DeviceQueueProbeResult run_device_queue_probe(IProbeDeviceObserver* observer = nullptr) noexcept;
RuntimeWorkloadResult run_runtime_workload(IProbeRenderer* renderer = nullptr);
RuntimeWorkloadProfile measure_runtime_workload_profile(IProbeMonotonicClock& clock,
                                                        std::uint32_t samples,
                                                        IProbeRenderer* renderer = nullptr);
bool equivalent_device_queue_semantics(const DeviceQueueProbeResult& left,
                                       const DeviceQueueProbeResult& right) noexcept;
bool equivalent_runtime_workload(const RuntimeWorkloadResult& left,
                                 const RuntimeWorkloadResult& right) noexcept;
bool equivalent_semantics(const ProbeRunResult& left, const ProbeRunResult& right) noexcept;
bool matches_golden(const ProbeRunResult& result) noexcept;

} // namespace rpcmp::spike

#endif // RPCMP_SPIKE_COMPARISON_PROBE_HPP
