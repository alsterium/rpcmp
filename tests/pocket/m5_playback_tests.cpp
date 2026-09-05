#include "rpcmp/spike/m5_playback.hpp"
#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace {

using rpcmp::spike::M5PlaybackError;
using rpcmp::spike::M5PlaybackPump;
using rpcmp::spike::M5PlaybackState;
using rpcmp::spike::M5Submit;

struct Record {
  std::uint64_t due{};
  rpcmp::runtime::mdx::Ym2151Write write{};
};

class Device final : public rpcmp::spike::IM5PlaybackDevice {
public:
  std::uint64_t time{};
  std::uint64_t maximum_due{};
  bool full{};
  bool fault{};
  bool clock_fault{};
  bool reset_ok{true};
  unsigned resets{};
  std::vector<Record> records;
  bool now(std::uint64_t& value) noexcept override {
    value = time;
    return !clock_fault;
  }
  bool status(bool& empty) noexcept override {
    empty = !full && time >= maximum_due;
    return !fault;
  }
  M5Submit submit(const std::uint64_t due,
                  const rpcmp::runtime::mdx::Ym2151Write& write) noexcept override {
    if (fault)
      return M5Submit::Fault;
    if (full)
      return M5Submit::Backpressure;
    maximum_due = due;
    records.push_back({due, write});
    return M5Submit::Accepted;
  }
  bool reset() noexcept override {
    ++resets;
    return reset_ok;
  }
};

rpcmp::player::MdxLibrarySession session(const rpcmp::runtime::mdx::ByteView bytes) {
  static constexpr std::array<std::uint8_t, 2> kEnd{0xf1, 0x00};
  rpcmp::player::MdxLibrarySession result;
  for (std::size_t index = 0; index < result.document.tracks.size(); ++index) {
    result.document.tracks[index] = {static_cast<std::uint8_t>(index),
                                     index < 8 ? rpcmp::runtime::mdx::TrackTarget::Ym2151
                                               : rpcmp::runtime::mdx::TrackTarget::LegacyAdpcm,
                                     index * 100,
                                     {kEnd.data(), kEnd.size()}};
  }
  result.document.tracks[0].source = bytes;
  return result;
}

} // namespace

int main() {
  rpcmp::test::Suite suite;
  static rpcmp::player::MdxLibrarySessionWorkspace workspace;
  static constexpr std::array<std::uint8_t, 12> kRest{0xfe, 0x1b, 2,    0x7f, 0x7f, 0x7f,
                                                      0x7f, 0xfe, 0x1b, 3,    0xf1, 0};
  auto music = session({kRest.data(), kRest.size()});
  RPCMP_CHECK(suite, rpcmp::runtime::mdx::prepare_mdx_playback(music.document, music.validation,
                                                               workspace.engine)
                         .ok());
  Device device;
  auto pump = std::make_unique<M5PlaybackPump>(music, workspace, device);
  for (device.time = 0;
       device.time < 12ULL * 48'000ULL && pump->state() == M5PlaybackState::Running;
       device.time += 48) {
    static_cast<void>(pump->service());
    RPCMP_CHECK(suite, device.maximum_due <= device.time + rpcmp::spike::kM5Lookahead);
  }
  RPCMP_CHECK(suite, pump->state() == M5PlaybackState::Complete && device.resets == 1);
  RPCMP_CHECK(suite, device.records.size() == 2 && pump->writes() == 2);
  RPCMP_CHECK(suite, device.records[0].due == rpcmp::spike::kM5StartupLead);
  RPCMP_CHECK(suite, device.records[1].due - device.records[0].due > 2ULL * 48'000ULL);
  RPCMP_CHECK(suite, pump->service() == M5PlaybackState::Complete && device.resets == 1);

  // Direct engine output equals submitted output minus the documented startup lead.
  auto direct = session({kRest.data(), kRest.size()});
  static rpcmp::runtime::mdx::TimedYm2151Batch batch;
  std::size_t observed{};
  std::uint64_t digest{};
  for (unsigned tick = 0; tick < 520; ++tick) {
    RPCMP_CHECK(suite, rpcmp::runtime::mdx::advance_mdx_tick(direct.document, 48'000, direct.state,
                                                             batch, workspace.engine)
                           .ok());
    for (std::size_t index = 0; index < batch.count; ++index) {
      const auto& write = batch.writes[index];
      RPCMP_CHECK(suite, observed < device.records.size());
      if (observed < device.records.size()) {
        RPCMP_CHECK(suite,
                    device.records[observed].due == write.at_tick + rpcmp::spike::kM5StartupLead);
        RPCMP_CHECK(suite, device.records[observed].write.address == write.write.address);
        RPCMP_CHECK(suite, device.records[observed].write.value == write.write.value);
      }
      ++observed;
      digest = (digest * 1099511628211ULL) ^ write.at_tick;
      digest = (digest * 1099511628211ULL) ^ write.write.address;
      digest = (digest * 1099511628211ULL) ^ write.write.value;
    }
  }
  RPCMP_CHECK(suite, observed == pump->writes() && digest == pump->digest());

  static constexpr std::array<std::uint8_t, 8> kOrdered{0xfe, 0x1b, 2, 0xfe, 0x1b, 3, 0xf1, 0};
  music = session({kOrdered.data(), kOrdered.size()});
  device = {};
  device.full = true;
  pump = std::make_unique<M5PlaybackPump>(music, workspace, device);
  RPCMP_CHECK(suite, pump->service() == M5PlaybackState::Running && pump->writes() == 0);
  device.time = 48;
  device.full = false;
  RPCMP_CHECK(suite, pump->service() == M5PlaybackState::Running && pump->writes() == 2);
  RPCMP_CHECK(suite, device.records[0].write.value == 2 && device.records[1].write.value == 3);

  music = session({kOrdered.data(), kOrdered.size()});
  device = {};
  device.full = true;
  pump = std::make_unique<M5PlaybackPump>(music, workspace, device);
  static_cast<void>(pump->service());
  device.time = rpcmp::spike::kM5DeviceTimeout;
  RPCMP_CHECK(suite, pump->service() == M5PlaybackState::Fault);
  RPCMP_CHECK(suite, pump->error() == M5PlaybackError::Timeout && device.resets == 1);

  for (unsigned kind = 0; kind < 4; ++kind) {
    music = session({kOrdered.data(), kOrdered.size()});
    device = {};
    device.clock_fault = kind == 0;
    device.fault = kind == 1;
    device.reset_ok = kind != 1;
    if (kind == 2)
      music.state.timeline.scheduler_tick = std::numeric_limits<std::uint64_t>::max();
    if (kind == 3)
      music.document.tracks[0].source = {nullptr, 1};
    pump = std::make_unique<M5PlaybackPump>(music, workspace, device);
    RPCMP_CHECK(suite, pump->service() ==
                           (kind == 1 ? M5PlaybackState::ResetFailed : M5PlaybackState::Fault));
    RPCMP_CHECK(suite, device.resets == 1 && device.records.empty());
    static_cast<void>(pump->service());
    RPCMP_CHECK(suite, device.resets == 1);
  }
  return suite.finish("m5_playback");
}
