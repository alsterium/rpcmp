#include "rpcmp/spike/comparison_probe.hpp"

#include <cstdint>
#include <cstdio>

extern "C" {
int rpcmp_pocket_slot_size(std::uint32_t slot_id, std::uint32_t* size);
int rpcmp_pocket_slot_read(std::uint32_t slot_id, std::uint32_t offset, void* destination,
                           std::uint32_t length);
std::uint32_t rpcmp_pocket_time_us();
std::uint32_t rpcmp_pocket_poll_actions();
void rpcmp_pocket_wait_vblank();
void rpcmp_pocket_terminal_init();
[[noreturn]] void rpcmp_pocket_hold_result();
}

namespace {

constexpr std::uint32_t kSyntheticSlot = 4;
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

class PocketClock final : public rpcmp::spike::IProbeMonotonicClock {
public:
  std::uint32_t now_us() noexcept override { return rpcmp_pocket_time_us(); }
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

void print_interactive(const rpcmp::spike::ReadLatencyStats& latency,
                       const rpcmp::spike::InteractiveCommandProbe& probe) {
  std::printf("\033[2J\033[H");
  std::printf("RPCMP Pocket input probe\n\n");
  std::printf("AUTO: PASS\n");
  std::printf("read16 x%lu us\n", static_cast<unsigned long>(latency.iterations));
  std::printf("min=%lu avg=%lu max=%lu\n\n", static_cast<unsigned long>(latency.minimum_us),
              static_cast<unsigned long>(latency.average_us()),
              static_cast<unsigned long>(latency.maximum_us));
  std::printf("INPUT: %u/4\n", static_cast<unsigned>(probe.completed_steps()));
  std::printf("state=%s seq=%llu\n\n", transport_name(probe.latest().transport),
              static_cast<unsigned long long>(probe.latest().sequence));
  if (probe.completed()) {
    std::printf("INPUT: PASS\nOVERALL: PASS\n");
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
  print_result(observed, passed && latency.passed());
  if (!passed || !latency.passed()) {
    rpcmp_pocket_hold_result();
  }

  rpcmp::spike::InteractiveCommandProbe input_probe;
  if (!input_probe.ready()) {
    std::printf("INPUT INIT: FAIL\n");
    rpcmp_pocket_hold_result();
  }
  print_interactive(latency, input_probe);
  while (!input_probe.completed() && !input_probe.failed()) {
    rpcmp_pocket_wait_vblank();
    const auto action = pressed_action(rpcmp_pocket_poll_actions());
    if (action.has_value()) {
      static_cast<void>(input_probe.apply(*action));
      print_interactive(latency, input_probe);
    }
  }
  rpcmp_pocket_hold_result();
}
