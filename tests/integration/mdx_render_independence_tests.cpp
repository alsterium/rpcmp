#include "rpcmp/runtime/mdx_engine.hpp"
#include "rpcmp/runtime/mdx_scheduler_bridge.hpp"
#include "rpcmp/ui/text_ui.hpp"
#include "test_support.hpp"

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

struct DispatchedWrite {
  std::uint64_t at_tick{};
  rpcmp::contracts::DeviceOperation operation;
};

bool operator==(const DispatchedWrite& left, const DispatchedWrite& right) {
  return left.at_tick == right.at_tick && left.operation == right.operation;
}

class RecordingPort final : public rpcmp::runtime::IDevicePort {
public:
  rpcmp::contracts::DeviceId device_id() const noexcept override { return {1}; }
  rpcmp::contracts::DeviceType device_type() const noexcept override {
    return rpcmp::contracts::DeviceType::Ym2151;
  }
  rpcmp::runtime::DevicePortResult
  accept(const rpcmp::contracts::DeviceOperation& operation) override {
    dispatched.push_back({dispatch_tick, operation});
    return rpcmp::runtime::DevicePortResult::Accepted;
  }
  rpcmp::runtime::DevicePortResult reset() override {
    return rpcmp::runtime::DevicePortResult::Accepted;
  }

  std::uint64_t dispatch_tick{};
  std::vector<DispatchedWrite> dispatched;
};

std::vector<std::uint8_t> read_hex(const std::string& path) {
  std::ifstream input(path);
  std::vector<std::uint8_t> bytes;
  int high = -1;
  char character = 0;
  while (input.get(character)) {
    int nibble = -1;
    if (character >= '0' && character <= '9') {
      nibble = character - '0';
    } else if (character >= 'a' && character <= 'f') {
      nibble = character - 'a' + 10;
    }
    if (nibble < 0) {
      continue;
    }
    if (high < 0) {
      high = nibble;
    } else {
      bytes.push_back(static_cast<std::uint8_t>((high << 4) | nibble));
      high = -1;
    }
  }
  return high < 0 ? bytes : std::vector<std::uint8_t>{};
}

rpcmp::contracts::PlayerSnapshot snapshot_from(const rpcmp::runtime::mdx::MdxEngineState& state,
                                               const std::uint64_t sequence) {
  rpcmp::contracts::PlayerSnapshot snapshot{};
  snapshot.sequence = sequence;
  snapshot.published_at_tick = state.timeline.scheduler_tick;
  snapshot.position.position_ticks = state.timeline.scheduler_tick;
  snapshot.position.tick_rate = 48'000;
  snapshot.transport = rpcmp::contracts::TransportState::Playing;
  snapshot.track =
      rpcmp::contracts::TrackSummary{{1}, "Oracle FM", "RPCMP", std::nullopt, std::nullopt};
  snapshot.devices.push_back({{1},
                              rpcmp::contracts::DeviceType::Ym2151,
                              0,
                              8,
                              rpcmp::contracts::DeviceOperationalState::Ready,
                              {0}});
  for (std::uint16_t channel = 0; channel < 8; ++channel) {
    const auto& source = state.ym2151.channels[channel];
    rpcmp::contracts::ChannelState observed{};
    observed.channel_id = {channel};
    observed.label = "FM " + std::to_string(channel + 1);
    observed.kind = rpcmp::contracts::ChannelKind::Fm;
    observed.enabled = true;
    observed.key_on = source.key_on;
    observed.device_id = {1};
    observed.device_channel = channel;
    snapshot.channels.push_back(std::move(observed));
  }
  return snapshot;
}

std::vector<DispatchedWrite> run(const bool render_heavily, rpcmp::test::Suite& suite) {
  const auto bytes =
      read_hex(std::string(RPCMP_SOURCE_DIR) + "/tests/fixtures/mdx/oracle-fm.mdx.hex");
  rpcmp::runtime::mdx::MdxDocument document{};
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::parse({bytes.data(), bytes.size()}, document).ok());
  static rpcmp::runtime::mdx::MdxEngineScratch engine_scratch{};
  rpcmp::runtime::mdx::DocumentValidation validation{};
  RPCMP_CHECK(suite,
              rpcmp::runtime::mdx::prepare_mdx_playback(document, validation, engine_scratch).ok());

  RecordingPort port;
  rpcmp::runtime::DeviceScheduler scheduler(port, 48'000, 64, 100'000);
  rpcmp::runtime::mdx::MdxEngineState engine{};
  rpcmp::runtime::mdx::TimedYm2151Batch batch{};
  rpcmp::runtime::mdx::MdxSchedulerBridgeState bridge{};
  rpcmp::runtime::mdx::MdxSchedulerBridgeScratch bridge_scratch{};
  rpcmp::ui::UiState ui{};
  std::size_t rendered_lines = 0;

  for (std::uint64_t tick = 0; tick < 8; ++tick) {
    const auto dispatch_tick = engine.timeline.scheduler_tick;
    RPCMP_CHECK(suite, rpcmp::runtime::mdx::advance_mdx_tick(document, 48'000, engine, batch,
                                                             engine_scratch)
                           .ok());
    const auto begun =
        rpcmp::runtime::mdx::begin_scheduler_batch(batch, {1}, scheduler, bridge, bridge_scratch);
    RPCMP_CHECK(suite, begun == rpcmp::runtime::mdx::MdxSchedulerBridgeResult::Pending ||
                           begun == rpcmp::runtime::mdx::MdxSchedulerBridgeResult::Complete);
    if (begun == rpcmp::runtime::mdx::MdxSchedulerBridgeResult::Pending) {
      RPCMP_CHECK(suite, rpcmp::runtime::mdx::pump_scheduler_batch(batch, {1}, scheduler, bridge,
                                                                   bridge_scratch) ==
                             rpcmp::runtime::mdx::MdxSchedulerBridgeResult::Complete);
    }

    if (render_heavily) {
      const auto snapshot = snapshot_from(engine, tick + 1);
      for (std::size_t render = 0; render < 257; ++render) {
        rpcmp::ui::apply_action(ui, rpcmp::ui::UiAction::SwitchView, snapshot.channels.size());
        rendered_lines += rpcmp::ui::render(snapshot, ui).size();
      }
    }
    port.dispatch_tick = dispatch_tick;
    RPCMP_CHECK(suite,
                scheduler.advance_to(dispatch_tick) == rpcmp::runtime::SchedulerResult::Accepted);
  }
  if (render_heavily) {
    RPCMP_CHECK(suite, rendered_lines != 0);
  }
  return port.dispatched;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  const auto headless = run(false, suite);
  const auto delayed_renderer = run(true, suite);
  RPCMP_CHECK(suite, !headless.empty());
  RPCMP_CHECK(suite, headless == delayed_renderer);
  return suite.finish("MDX renderer independence");
}
