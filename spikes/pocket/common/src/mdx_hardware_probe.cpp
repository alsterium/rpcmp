#include "rpcmp/spike/mdx_hardware_probe.hpp"

#include "rpcmp/runtime/device_scheduler.hpp"
#include "rpcmp/runtime/mdx_engine.hpp"
#include "rpcmp/runtime/mdx_scheduler_bridge.hpp"

#include <array>
#include <cstdint>
#include <variant>

namespace rpcmp::spike {
namespace {

constexpr std::array<std::uint8_t, kMdxHardwareProbeFixtureBytes> kFixture{
    0x52, 0x50, 0x43, 0x4d, 0x50, 0x20, 0x4f, 0x52, 0x41, 0x43, 0x4c, 0x45, 0x0d, 0x0a, 0x1a, 0x00,
    0x00, 0x32, 0x00, 0x14, 0x00, 0x1f, 0x00, 0x24, 0x00, 0x26, 0x00, 0x28, 0x00, 0x2a, 0x00, 0x2c,
    0x00, 0x2e, 0x00, 0x30, 0xff, 0xc8, 0xfe, 0x1b, 0x02, 0xfd, 0x01, 0x80, 0x02, 0xf1, 0x00, 0xfe,
    0x20, 0x55, 0xf1, 0x00, 0xf1, 0x00, 0xf1, 0x00, 0xf1, 0x00, 0xf1, 0x00, 0xf1, 0x00, 0xf1, 0x00,
    0xf1, 0x00, 0x01, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

class HashingPort final : public runtime::IDevicePort {
public:
  contracts::DeviceId device_id() const noexcept override { return {1}; }
  contracts::DeviceType device_type() const noexcept override {
    return contracts::DeviceType::Ym2151;
  }
  runtime::DevicePortResult accept(const contracts::DeviceOperation& operation) override {
    const auto* write = std::get_if<contracts::WriteRegister>(&operation);
    if (write == nullptr) {
      return runtime::DevicePortResult::DeviceFault;
    }
    add_integer(dispatch_tick_);
    add_byte(write->address);
    add_byte(write->value);
    if (writes_ < trace_.size()) {
      trace_[writes_] = {dispatch_tick_, write->address, write->value};
    }
    ++writes_;
    return runtime::DevicePortResult::Accepted;
  }
  runtime::DevicePortResult reset() override { return runtime::DevicePortResult::Accepted; }

  void clear() noexcept {
    dispatch_tick_ = 0;
    digest_ = 14'695'981'039'346'656'037ULL;
    writes_ = 0;
    trace_ = {};
  }
  void set_dispatch_tick(const std::uint64_t tick) noexcept { dispatch_tick_ = tick; }
  std::uint64_t digest() const noexcept { return digest_; }
  std::uint32_t writes() const noexcept { return writes_; }
  const std::array<MdxHardwareWrite, kMdxHardwareProbeExpectedWrites>& trace() const noexcept {
    return trace_;
  }

private:
  void add_byte(const std::uint8_t value) noexcept {
    digest_ ^= value;
    digest_ *= 1'099'511'628'211ULL;
  }
  void add_integer(std::uint64_t value) noexcept {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
      add_byte(static_cast<std::uint8_t>(value));
      value >>= 8U;
    }
  }

  std::uint64_t dispatch_tick_{};
  std::uint64_t digest_{14'695'981'039'346'656'037ULL};
  std::uint32_t writes_{};
  std::array<MdxHardwareWrite, kMdxHardwareProbeExpectedWrites> trace_{};
};

runtime::mdx::MdxDocument document;
runtime::mdx::MdxEngineState engine;
runtime::mdx::TimedYm2151Batch batch;
runtime::mdx::MdxEngineScratch engine_scratch;
runtime::mdx::MdxSchedulerBridgeState bridge;
runtime::mdx::MdxSchedulerBridgeScratch bridge_scratch;
runtime::mdx::DocumentValidation validation;
HashingPort port;

} // namespace

bool MdxHardwareProbeResult::passed() const noexcept {
  return parse_error == runtime::mdx::MdxError::None &&
         prepare_error == runtime::mdx::DecodeError::None &&
         playback_error == runtime::mdx::DecodeError::None && scheduler_ok &&
         driver_ticks == kMdxHardwareProbeDriverTicks &&
         writes == kMdxHardwareProbeExpectedWrites &&
         end_tick == kMdxHardwareProbeExpectedEndTick &&
         write_digest == kMdxHardwareProbeExpectedDigest;
}

MdxHardwareProbeResult run_mdx_hardware_probe(MdxHardwareWrite* const trace,
                                              const std::size_t trace_capacity) noexcept {
  MdxHardwareProbeResult result{};
  document = {};
  engine = {};
  batch = {};
  bridge = {};
  validation = {};
  port.clear();

  const auto parsed = runtime::mdx::parse({kFixture.data(), kFixture.size()}, document);
  result.parse_error = parsed.error;
  if (!parsed.ok()) {
    return result;
  }
  const auto prepared = runtime::mdx::prepare_mdx_playback(document, validation, engine_scratch);
  result.prepare_error = prepared.error;
  if (!prepared.ok()) {
    return result;
  }

  runtime::DeviceScheduler scheduler(port, 48'000, runtime::kMaxScheduledDeviceOps, 100'000);
  result.scheduler_ok = true;
  for (std::uint32_t tick = 0; tick < kMdxHardwareProbeDriverTicks; ++tick) {
    const auto dispatch_tick = engine.timeline.scheduler_tick;
    const auto advanced =
        runtime::mdx::advance_mdx_tick(document, 48'000, engine, batch, engine_scratch);
    result.playback_error = advanced.error;
    if (!advanced.ok()) {
      return result;
    }
    const auto begun =
        runtime::mdx::begin_scheduler_batch(batch, {1}, scheduler, bridge, bridge_scratch);
    if (begun == runtime::mdx::MdxSchedulerBridgeResult::Pending &&
        runtime::mdx::pump_scheduler_batch(batch, {1}, scheduler, bridge, bridge_scratch) !=
            runtime::mdx::MdxSchedulerBridgeResult::Complete) {
      result.scheduler_ok = false;
      return result;
    }
    if (begun != runtime::mdx::MdxSchedulerBridgeResult::Pending &&
        begun != runtime::mdx::MdxSchedulerBridgeResult::Complete) {
      result.scheduler_ok = false;
      return result;
    }
    port.set_dispatch_tick(dispatch_tick);
    if (scheduler.advance_to(dispatch_tick) != runtime::SchedulerResult::Accepted) {
      result.scheduler_ok = false;
      return result;
    }
    ++result.driver_ticks;
  }
  result.writes = port.writes();
  result.write_digest = port.digest();
  if (trace != nullptr && trace_capacity >= port.trace().size()) {
    for (std::size_t index = 0; index < port.trace().size(); ++index) {
      trace[index] = port.trace()[index];
    }
  }
  result.end_tick = engine.timeline.scheduler_tick;
  return result;
}

} // namespace rpcmp::spike
