#include "rpcmp/runtime/mdx_scheduler_bridge.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace {

class RecordingPort final : public rpcmp::runtime::IDevicePort {
public:
  rpcmp::contracts::DeviceId device_id() const noexcept override { return {1}; }
  rpcmp::contracts::DeviceType device_type() const noexcept override {
    return rpcmp::contracts::DeviceType::Ym2151;
  }
  rpcmp::runtime::DevicePortResult
  accept(const rpcmp::contracts::DeviceOperation& operation) override {
    if (blocked) {
      return rpcmp::runtime::DevicePortResult::Backpressure;
    }
    accepted.push_back(operation);
    return rpcmp::runtime::DevicePortResult::Accepted;
  }
  rpcmp::runtime::DevicePortResult reset() override {
    return rpcmp::runtime::DevicePortResult::Accepted;
  }

  bool blocked{};
  std::vector<rpcmp::contracts::DeviceOperation> accepted;
};

void fill_batch(rpcmp::runtime::mdx::TimedYm2151Batch& batch, const std::size_t count) {
  batch.count = count;
  for (std::size_t index = 0; index < count; ++index) {
    batch.writes[index] = {
        0, {static_cast<std::uint8_t>(index), static_cast<std::uint8_t>(index + 1), 0}};
  }
}

void test_chunked_order_and_resume(rpcmp::test::Suite& suite) {
  RecordingPort port;
  rpcmp::runtime::DeviceScheduler scheduler(port, 48'000, 64, 1'000);
  static rpcmp::runtime::mdx::TimedYm2151Batch batch{};
  fill_batch(batch, 130);
  rpcmp::runtime::mdx::MdxSchedulerBridgeState state{};
  rpcmp::runtime::mdx::MdxSchedulerBridgeScratch scratch{};

  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::begin_scheduler_batch(batch, {1}, scheduler, state, scratch) ==
                  rpcmp::runtime::mdx::MdxSchedulerBridgeResult::Pending);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::pump_scheduler_batch(batch, {1}, scheduler, state, scratch) ==
                  rpcmp::runtime::mdx::MdxSchedulerBridgeResult::Pending);
  RPCMP_CHECK(suite, state.cursor == 64);
  RPCMP_CHECK(suite, scheduler.pending_count() == 64);

  port.blocked = true;
  RPCMP_CHECK(suite, scheduler.advance_to(0) == rpcmp::runtime::SchedulerResult::Backpressure);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::pump_scheduler_batch(batch, {1}, scheduler, state, scratch) ==
                  rpcmp::runtime::mdx::MdxSchedulerBridgeResult::Pending);
  RPCMP_CHECK(suite, state.cursor == 64);

  port.blocked = false;
  RPCMP_CHECK(suite, scheduler.advance_to(0) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::pump_scheduler_batch(batch, {1}, scheduler, state, scratch) ==
                  rpcmp::runtime::mdx::MdxSchedulerBridgeResult::Pending);
  RPCMP_CHECK(suite, scheduler.advance_to(0) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::pump_scheduler_batch(batch, {1}, scheduler, state, scratch) ==
                  rpcmp::runtime::mdx::MdxSchedulerBridgeResult::Complete);
  RPCMP_CHECK(suite, scheduler.advance_to(0) == rpcmp::runtime::SchedulerResult::Accepted);
  RPCMP_CHECK(suite, port.accepted.size() == 130);
  for (std::size_t index = 0; index < port.accepted.size(); ++index) {
    const auto* write = std::get_if<rpcmp::contracts::WriteRegister>(&port.accepted[index]);
    RPCMP_CHECK(suite, write != nullptr);
    if (write != nullptr) {
      RPCMP_CHECK(suite, write->address == static_cast<std::uint8_t>(index));
      RPCMP_CHECK(suite, write->value == static_cast<std::uint8_t>(index + 1));
    }
  }
}

void test_transactional_validation(rpcmp::test::Suite& suite) {
  static rpcmp::runtime::mdx::TimedYm2151Batch batch{};
  fill_batch(batch, 3);
  batch.writes[2].at_tick = 1;
  batch.writes[1].at_tick = 2;
  rpcmp::runtime::mdx::MdxSchedulerBridgeState state{};
  rpcmp::runtime::mdx::MdxSchedulerBridgeScratch scratch{};
  RecordingPort port;
  rpcmp::runtime::DeviceScheduler scheduler(port, 48'000, 64, 1'000);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::begin_scheduler_batch(batch, {1}, scheduler, state, scratch) ==
                  rpcmp::runtime::mdx::MdxSchedulerBridgeResult::InvalidBatch);
  RPCMP_CHECK(suite, !state.active);
  RPCMP_CHECK(suite, state.cursor == 0);

  fill_batch(batch, 70);
  batch.writes[69].at_tick = 1'001;
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::begin_scheduler_batch(batch, {1}, scheduler, state, scratch) ==
                  rpcmp::runtime::mdx::MdxSchedulerBridgeResult::TimeOverflow);
  RPCMP_CHECK(suite, !state.active);
  RPCMP_CHECK(suite, scheduler.pending_count() == 0);

  fill_batch(batch, 1);
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::begin_scheduler_batch(batch, {2}, scheduler, state, scratch) ==
                  rpcmp::runtime::mdx::MdxSchedulerBridgeResult::InvalidDevice);
  RPCMP_CHECK(suite, !state.active);
  RPCMP_CHECK(suite, scheduler.pending_count() == 0);

  rpcmp::runtime::DeviceOpBatchView invalid_view{nullptr, 1,
                                                 rpcmp::contracts::kDeviceOpStreamVersion, 48'000};
  RPCMP_CHECK(suite,
              scheduler.submit(invalid_view) == rpcmp::runtime::SchedulerResult::InvalidOperation);
  RPCMP_CHECK(suite, scheduler.pending_count() == 0);
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  test_chunked_order_and_resume(suite);
  test_transactional_validation(suite);
  return suite.finish("MDX scheduler bridge");
}
