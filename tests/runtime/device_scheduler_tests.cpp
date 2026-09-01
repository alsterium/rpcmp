#include "rpcmp/runtime/device_scheduler.hpp"
#include "rpcmp/runtime/fixed_ym2151_sequence.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <limits>
#include <vector>

namespace {

class RecordingPort final : public rpcmp::runtime::IDevicePort {
public:
  rpcmp::contracts::DeviceId device_id() const noexcept override { return id; }
  rpcmp::contracts::DeviceType device_type() const noexcept override { return type; }

  rpcmp::runtime::DevicePortResult
  accept(const rpcmp::contracts::DeviceOperation& operation) override {
    if (fault_accept) {
      return rpcmp::runtime::DevicePortResult::DeviceFault;
    }
    if (backpressure) {
      return rpcmp::runtime::DevicePortResult::Backpressure;
    }
    accepted.push_back(operation);
    return rpcmp::runtime::DevicePortResult::Accepted;
  }

  rpcmp::runtime::DevicePortResult reset() override {
    ++reset_count;
    if (fault_reset) {
      return rpcmp::runtime::DevicePortResult::DeviceFault;
    }
    return backpressure_reset ? rpcmp::runtime::DevicePortResult::Backpressure
                              : rpcmp::runtime::DevicePortResult::Accepted;
  }

  rpcmp::contracts::DeviceId id{1};
  rpcmp::contracts::DeviceType type{rpcmp::contracts::DeviceType::Ym2151};
  bool backpressure{};
  bool fault_accept{};
  bool fault_reset{};
  bool backpressure_reset{};
  std::size_t reset_count{};
  std::vector<rpcmp::contracts::DeviceOperation> accepted;
};

rpcmp::contracts::DeviceOpStream small_stream(const std::uint64_t first_tick = 10,
                                              const std::uint64_t second_tick = 10) {
  rpcmp::contracts::DeviceOpStream stream;
  stream.tick_rate = 1'000;
  stream.operations = {
      {first_tick, {1}, rpcmp::contracts::WriteRegister{0x20, 0x01}},
      {second_tick, {1}, rpcmp::contracts::WriteRegister{0x21, 0x02}},
  };
  return stream;
}

void test_fixed_sequence_order(rpcmp::test::Suite& suite) {
  RecordingPort port;
  rpcmp::runtime::DeviceScheduler scheduler(port, 1'000, 32, 3'500);
  const auto sequence = rpcmp::runtime::make_fixed_ym2151_sequence();

  RPCMP_CHECK(suite, scheduler.submit(sequence) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, scheduler.advance_to(0) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, port.accepted.size() == 13);
  RPCMP_CHECK(suite, scheduler.advance_to(999) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, port.accepted.size() == 13);
  RPCMP_CHECK(suite, scheduler.advance_to(1'000) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, scheduler.advance_to(3'500) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, port.accepted.size() == sequence.operations.size());
  RPCMP_CHECK(suite, scheduler.pending_count() == 0);

  for (std::size_t index = 0; index < sequence.operations.size(); ++index) {
    RPCMP_CHECK(suite, port.accepted[index] == sequence.operations[index].operation);
  }
}

void test_backpressure_and_equal_time_order(rpcmp::test::Suite& suite) {
  RecordingPort port;
  rpcmp::runtime::DeviceScheduler scheduler(port, 1'000, 4, 100);
  const auto stream = small_stream();
  RPCMP_CHECK(suite, scheduler.submit(stream) == rpcmp::runtime::SchedulerResult::Accepted);

  port.backpressure = true;
  RPCMP_CHECK(suite, scheduler.advance_to(10) == rpcmp::runtime::SchedulerResult::Backpressure);
  RPCMP_CHECK(suite, scheduler.pending_count() == 2);
  RPCMP_CHECK(suite, port.accepted.empty());

  port.backpressure = false;
  RPCMP_CHECK(suite, scheduler.advance_to(10) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, scheduler.pending_count() == 0);
  RPCMP_CHECK(suite, port.accepted.size() == 2);
  RPCMP_CHECK(suite, port.accepted[0] == stream.operations[0].operation);
  RPCMP_CHECK(suite, port.accepted[1] == stream.operations[1].operation);
}

void test_admission_validation(rpcmp::test::Suite& suite) {
  RecordingPort port;
  rpcmp::runtime::DeviceScheduler scheduler(port, 1'000, 3, 100);
  auto stream = small_stream();
  RPCMP_CHECK(suite, scheduler.submit(stream) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, scheduler.submit(stream) == rpcmp::runtime::SchedulerResult::QueueFull);
  RPCMP_CHECK(suite, scheduler.pending_count() == 2);

  auto earlier_stream = small_stream(9, 9);
  earlier_stream.operations.resize(1);
  RPCMP_CHECK(suite, scheduler.submit(earlier_stream) ==
                         rpcmp::runtime::SchedulerResult::InvalidOperation);
  RPCMP_CHECK(suite, scheduler.pending_count() == 2);

  stream.operations.front().device_id = {2};
  RPCMP_CHECK(suite, scheduler.submit(stream) == rpcmp::runtime::SchedulerResult::InvalidDevice);
  stream = small_stream(11, 10);
  RPCMP_CHECK(suite, scheduler.submit(stream) == rpcmp::runtime::SchedulerResult::InvalidOperation);
  stream = small_stream();
  stream.version = 2;
  RPCMP_CHECK(suite, scheduler.submit(stream) == rpcmp::runtime::SchedulerResult::InvalidOperation);
  stream.version = rpcmp::contracts::kDeviceOpStreamVersion;
  stream.tick_rate = 999;
  RPCMP_CHECK(suite, scheduler.submit(stream) == rpcmp::runtime::SchedulerResult::InvalidOperation);
  stream = small_stream(10, 101);
  RPCMP_CHECK(suite, scheduler.submit(stream) == rpcmp::runtime::SchedulerResult::TimeOverflow);

  RecordingPort wrong_type;
  wrong_type.type = rpcmp::contracts::DeviceType::Other;
  rpcmp::runtime::DeviceScheduler wrong_scheduler(wrong_type, 1'000, 3, 100);
  RPCMP_CHECK(suite, wrong_scheduler.submit(small_stream()) ==
                         rpcmp::runtime::SchedulerResult::InvalidDevice);

  rpcmp::runtime::DeviceScheduler invalid_scheduler(port, 0, 0, 100);
  RPCMP_CHECK(suite, invalid_scheduler.submit(small_stream()) ==
                         rpcmp::runtime::SchedulerResult::InvalidOperation);
  rpcmp::runtime::DeviceScheduler oversized_scheduler(
      port, 1'000, rpcmp::runtime::kMaxScheduledDeviceOps + 1, 100);
  RPCMP_CHECK(suite, oversized_scheduler.submit(small_stream()) ==
                         rpcmp::runtime::SchedulerResult::InvalidOperation);
}

void test_time_and_reset(rpcmp::test::Suite& suite) {
  RecordingPort port;
  rpcmp::runtime::DeviceScheduler scheduler(port, 1'000, 4,
                                            std::numeric_limits<std::uint64_t>::max());
  RPCMP_CHECK(suite,
              scheduler.submit(small_stream(20, 30)) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, scheduler.advance_to(15) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, scheduler.advance_to(14) == rpcmp::runtime::SchedulerResult::TimeReversed);
  RPCMP_CHECK(suite, scheduler.advance_to(std::numeric_limits<std::uint64_t>::max() - 1) ==
                         rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, scheduler.advance_by(2) == rpcmp::runtime::SchedulerResult::TimeOverflow);
  RPCMP_CHECK(suite, scheduler.reset() == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, scheduler.media_tick() == 0);
  RPCMP_CHECK(suite, scheduler.pending_count() == 0);
  RPCMP_CHECK(suite, port.reset_count == 1);
}

void test_fault_latching(rpcmp::test::Suite& suite) {
  RecordingPort port;
  rpcmp::runtime::DeviceScheduler scheduler(port, 1'000, 4, 100);
  RPCMP_CHECK(suite, scheduler.submit(small_stream()) == rpcmp::runtime::SchedulerResult::Accepted);
  port.fault_accept = true;
  RPCMP_CHECK(suite, scheduler.advance_to(10) == rpcmp::runtime::SchedulerResult::DeviceFault);
  RPCMP_CHECK(suite, scheduler.faulted());
  RPCMP_CHECK(suite, scheduler.pending_count() == 2);
  RPCMP_CHECK(suite, scheduler.advance_to(11) == rpcmp::runtime::SchedulerResult::DeviceFault);

  port.fault_accept = false;
  RPCMP_CHECK(suite, scheduler.reset() == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, !scheduler.faulted());
  RPCMP_CHECK(suite, scheduler.pending_count() == 0);

  port.fault_reset = true;
  RPCMP_CHECK(suite, scheduler.reset() == rpcmp::runtime::SchedulerResult::DeviceFault);
  RPCMP_CHECK(suite, scheduler.faulted());

  port.fault_reset = false;
  port.backpressure_reset = true;
  RPCMP_CHECK(suite, scheduler.reset() == rpcmp::runtime::SchedulerResult::Backpressure);
  RPCMP_CHECK(suite, !scheduler.faulted());
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  test_fixed_sequence_order(suite);
  test_backpressure_and_equal_time_order(suite);
  test_admission_validation(suite);
  test_time_and_reset(suite);
  test_fault_latching(suite);
  return suite.finish("device_scheduler");
}
