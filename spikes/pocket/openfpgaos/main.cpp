#include "rpcmp/library/logical_library.hpp"
#include "rpcmp/spike/comparison_probe.hpp"
#include "rpcmp/spike/mdx_hardware_probe.hpp"

#include <cstdint>
#include <cstdio>

extern "C" {
int rpcmp_pocket_slot_size(std::uint32_t slot_id, std::uint32_t* size);
int rpcmp_pocket_slot_read(std::uint32_t slot_id, std::uint32_t offset, void* destination,
                           std::uint32_t length);
int rpcmp_pocket_target_read_prepare(std::uint32_t length);
int rpcmp_pocket_target_read(std::uint32_t slot_id, std::uint32_t offset, void* destination,
                             std::uint32_t length, std::uint32_t* elapsed_us);
std::uint32_t rpcmp_pocket_time_us();
std::uint32_t rpcmp_pocket_poll_actions();
void rpcmp_pocket_wait_vblank();
void rpcmp_pocket_terminal_init();
[[noreturn]] void rpcmp_pocket_hold_result();
}

namespace {

constexpr std::uint32_t kSyntheticSlot = 4;
constexpr std::uint32_t kLibrarySlot = 5;
constexpr std::size_t kPocketLibraryBytes = 1024;
constexpr std::uint32_t kActionPlay = 1U << 0;
constexpr std::uint32_t kActionTogglePause = 1U << 1;
constexpr std::uint32_t kActionStop = 1U << 2;

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

bool verify_library_blob() {
  std::uint32_t size{};
  if (rpcmp_pocket_slot_size(kLibrarySlot, &size) != 0 || size < rpcmp::library::kHeaderSize ||
      size > kPocketLibraryBytes) {
    return false;
  }
  static std::uint8_t bytes[kPocketLibraryBytes]{};
  if (rpcmp_pocket_slot_read(kLibrarySlot, 0, bytes, size) != 0) {
    return false;
  }
  rpcmp::library::ValidationLimits limits;
  limits.max_sections = 6;
  limits.max_file_size = kPocketLibraryBytes;
  limits.max_records_per_section = 16;
  limits.max_string_bytes = 64;
  limits.max_dependencies_per_track = 4;
  limits.max_total_decoded_bytes = 256;
  rpcmp::library::LogicalLibrary library;
  if (rpcmp::library::LogicalLibrary::open({bytes, size}, library, limits) !=
      rpcmp::library::LibraryError::None) {
    return false;
  }
  rpcmp::library::BlobView blob;
  constexpr rpcmp::contracts::BlobId kExpectedBlob{0xA75C21A0C642594AULL};
  if (!library.find_blob(kExpectedBlob, blob) || blob.bytes.size != 3 || blob.bytes.data[0] != 1 ||
      blob.bytes.data[1] != 2 || blob.bytes.data[2] != 3) {
    return false;
  }
  return !library.find_blob(rpcmp::contracts::BlobId{1}, blob);
}

class PocketClock final : public rpcmp::spike::IProbeMonotonicClock {
public:
  std::uint32_t now_us() noexcept override { return rpcmp_pocket_time_us(); }
};

class TargetDataSlotBlob final : public rpcmp::spike::IProbeTimedBlobReader {
public:
  TargetDataSlotBlob() {
    valid_ = rpcmp_pocket_target_read_prepare(rpcmp::spike::kSyntheticBlobSize) == 0;
  }

  bool read_timed(const std::uint32_t offset, std::uint8_t* const destination,
                  const std::uint32_t length, std::uint32_t& elapsed_us) noexcept override {
    return valid_ &&
           rpcmp_pocket_target_read(kSyntheticSlot, offset, destination, length, &elapsed_us) == 0;
  }

private:
  bool valid_{};
};

class DeviceObserver final : public rpcmp::spike::IProbeDeviceObserver {
public:
  void observe(const rpcmp::spike::ProbeDeviceWrite&) override { ++count_; }
  std::uint32_t count() const noexcept { return count_; }

private:
  std::uint32_t count_{};
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

void print_result(const rpcmp::spike::ProbeRunResult& result,
                  const rpcmp::spike::MdxHardwareProbeResult& mdx, const bool library_passed,
                  const bool passed) {
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
  std::printf("LIBRARY: %s\n", library_passed ? "PASS" : "FAIL");
  std::printf("MDX: %s W=%lu T=%llu\n", mdx.passed() ? "PASS" : "FAIL",
              static_cast<unsigned long>(mdx.writes),
              static_cast<unsigned long long>(mdx.end_tick));
  std::printf("MDX digest=%016llx\n", static_cast<unsigned long long>(mdx.write_digest));
  std::printf("\nRESULT: %s\n", passed ? "PASS" : "FAIL");
  std::printf("Open the Pocket menu to exit.\n");
}

const char* transport_name(const rpcmp::contracts::TransportState transport) noexcept {
  switch (transport) {
  case rpcmp::contracts::TransportState::Stopped:
    return "stopped";
  case rpcmp::contracts::TransportState::Playing:
    return "playing";
  case rpcmp::contracts::TransportState::Paused:
    return "paused";
  default:
    return "other";
  }
}

const char* action_prompt(const rpcmp::spike::ProbeAction action) noexcept {
  switch (action) {
  case rpcmp::spike::ProbeAction::Play:
    return "Press A: Play";
  case rpcmp::spike::ProbeAction::TogglePause:
    return "Press B: Pause/Resume";
  case rpcmp::spike::ProbeAction::Stop:
    return "Press START: Stop";
  }
  return "";
}

void print_profile(const char* const label, const rpcmp::spike::TargetReadProfile& profile) {
  std::printf("%s a/50/90=%lu/%lu/%lu\n", label, static_cast<unsigned long>(profile.average_us()),
              static_cast<unsigned long>(profile.percentile_50_us),
              static_cast<unsigned long>(profile.percentile_90_us));
  std::printf("95/99/M/1k/2k=%lu/%lu/%lu/%lu/%lu\n",
              static_cast<unsigned long>(profile.percentile_95_us),
              static_cast<unsigned long>(profile.percentile_99_us),
              static_cast<unsigned long>(profile.maximum_us),
              static_cast<unsigned long>(profile.at_or_above_1ms),
              static_cast<unsigned long>(profile.at_or_above_2ms));
}

void print_workload_profile(const char* const label,
                            const rpcmp::spike::RuntimeWorkloadProfile& profile) {
  std::printf("%s a/50/90=%lu/%lu/%lu\n", label, static_cast<unsigned long>(profile.average_us()),
              static_cast<unsigned long>(profile.percentile_50_us),
              static_cast<unsigned long>(profile.percentile_90_us));
  std::printf("95/99/M=%lu/%lu/%lu\n", static_cast<unsigned long>(profile.percentile_95_us),
              static_cast<unsigned long>(profile.percentile_99_us),
              static_cast<unsigned long>(profile.maximum_us));
}

void print_interactive(const rpcmp::spike::TargetReadProfile& fixed_16,
                       const rpcmp::spike::TargetReadProfile& rotating_16,
                       const rpcmp::spike::TargetReadProfile& fixed_256,
                       const rpcmp::spike::TargetReadProfile& fixed_4096,
                       const rpcmp::spike::RuntimeWorkloadProfile& headless_workload,
                       const rpcmp::spike::RuntimeWorkloadProfile& observed_workload,
                       const rpcmp::spike::MdxHardwareProbeResult& mdx,
                       const rpcmp::spike::InteractiveCommandProbe& probe) {
  std::printf("\033[2J\033[H");
  std::printf("RPCMP runtime workload probe\n");
  std::printf("AUTO/QUEUE: PASS\n");
  std::printf("MDX: PASS W=%lu T=%llu\n", static_cast<unsigned long>(mdx.writes),
              static_cast<unsigned long long>(mdx.end_tick));
  std::printf("MDX D=%016llx\n", static_cast<unsigned long long>(mdx.write_digest));
  print_profile("F16", fixed_16);
  print_profile("R16", rotating_16);
  print_profile("F256", fixed_256);
  print_profile("F4096", fixed_4096);
  print_workload_profile("WH", headless_workload);
  print_workload_profile("WO", observed_workload);
  std::printf("WD s=%016llx\n",
              static_cast<unsigned long long>(headless_workload.reference.snapshot_digest));
  std::printf("WD e=%016llx\n",
              static_cast<unsigned long long>(headless_workload.reference.event_digest));
  std::printf("WD w=%016llx\n",
              static_cast<unsigned long long>(headless_workload.reference.write_digest));
  std::printf("\n");
  std::printf("INPUT: %u/4\n", static_cast<unsigned>(probe.completed_steps()));
  std::printf("state=%s seq=%llu\n\n", transport_name(probe.latest().transport),
              static_cast<unsigned long long>(probe.latest().sequence));
  if (probe.completed()) {
    std::printf("MDX: PASS\nINPUT: PASS\nOVERALL: PASS\n");
    std::printf("Open Pocket menu to exit.\n");
    return;
  }
  if (probe.failed()) {
    std::printf("INPUT: FAIL\nOVERALL: FAIL\n");
    return;
  }
  const auto expected = probe.expected_action();
  if (expected.has_value()) {
    std::printf("%s\n", action_prompt(*expected));
  }
}

std::optional<rpcmp::spike::ProbeAction> pressed_action(const std::uint32_t actions) noexcept {
  if ((actions & kActionPlay) != 0U) {
    return rpcmp::spike::ProbeAction::Play;
  }
  if ((actions & kActionTogglePause) != 0U) {
    return rpcmp::spike::ProbeAction::TogglePause;
  }
  if ((actions & kActionStop) != 0U) {
    return rpcmp::spike::ProbeAction::Stop;
  }
  return std::nullopt;
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

  PocketClock clock;
  const auto latency = rpcmp::spike::measure_read_latency(blob, clock);
  TargetDataSlotBlob target_blob;
  const auto fixed_16 = rpcmp::spike::measure_target_read_profile(
      target_blob, rpcmp::spike::kTargetProfileFullSamples, rpcmp::spike::kLatencyReadSize,
      rpcmp::spike::ReadOffsetPattern::Fixed);
  const auto rotating_16 = rpcmp::spike::measure_target_read_profile(
      target_blob, rpcmp::spike::kTargetProfileFullSamples, rpcmp::spike::kLatencyReadSize,
      rpcmp::spike::ReadOffsetPattern::Rotating);
  const auto fixed_256 = rpcmp::spike::measure_target_read_profile(
      target_blob, rpcmp::spike::kTargetProfileScaleSamples, 256U,
      rpcmp::spike::ReadOffsetPattern::Fixed);
  const auto fixed_4096 = rpcmp::spike::measure_target_read_profile(
      target_blob, rpcmp::spike::kTargetProfileScaleSamples, rpcmp::spike::kSyntheticBlobSize,
      rpcmp::spike::ReadOffsetPattern::Fixed);
  const bool profiles_passed =
      fixed_16.passed() && rotating_16.passed() && fixed_256.passed() && fixed_4096.passed();
  DeviceObserver device_observer;
  const auto observed_queue = rpcmp::spike::run_device_queue_probe(&device_observer);
  const auto headless_queue = rpcmp::spike::run_device_queue_probe();
  const bool queue_passed =
      observed_queue.passed() && headless_queue.passed() &&
      device_observer.count() == rpcmp::spike::kDeviceQueueCapacity &&
      rpcmp::spike::equivalent_device_queue_semantics(observed_queue, headless_queue);
  const auto headless_workload =
      rpcmp::spike::measure_runtime_workload_profile(clock, rpcmp::spike::kRuntimeWorkloadSamples);
  const auto observed_workload = rpcmp::spike::measure_runtime_workload_profile(
      clock, rpcmp::spike::kRuntimeWorkloadSamples, &observer);
  const bool workloads_passed = headless_workload.passed() && observed_workload.passed() &&
                                rpcmp::spike::equivalent_runtime_workload(
                                    headless_workload.reference, observed_workload.reference);
  const bool library_passed = verify_library_blob();
  const auto mdx = rpcmp::spike::run_mdx_hardware_probe();
  print_result(observed, mdx, library_passed,
               passed && latency.passed() && profiles_passed && queue_passed && workloads_passed &&
                   library_passed && mdx.passed());
  if (!passed || !latency.passed() || !profiles_passed || !queue_passed || !workloads_passed ||
      !library_passed || !mdx.passed()) {
    rpcmp_pocket_hold_result();
  }

  rpcmp::spike::InteractiveCommandProbe input_probe;
  if (!input_probe.ready()) {
    std::printf("INPUT INIT: FAIL\n");
    rpcmp_pocket_hold_result();
  }
  print_interactive(fixed_16, rotating_16, fixed_256, fixed_4096, headless_workload,
                    observed_workload, mdx, input_probe);
  while (!input_probe.completed() && !input_probe.failed()) {
    rpcmp_pocket_wait_vblank();
    const auto action = pressed_action(rpcmp_pocket_poll_actions());
    if (action.has_value()) {
      static_cast<void>(input_probe.apply(*action));
      print_interactive(fixed_16, rotating_16, fixed_256, fixed_4096, headless_workload,
                        observed_workload, mdx, input_probe);
    }
  }
  rpcmp_pocket_hold_result();
}
