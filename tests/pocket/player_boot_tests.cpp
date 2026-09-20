#include "test_support.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace {
struct Device {
  std::array<std::uint32_t, 256> words{};
  std::vector<std::uint32_t> operations;
  std::uint32_t status{0x800}, corrupt_offset{~0U}, corrupt_value{}, reply{1};
  unsigned polls{}, inhibit_wait{}, control_wait{}, submits{}, releases{}, invalid{};
  bool stuck_inhibit{}, stuck_control{}, offline{}, post_fault{};
  Device() noexcept {
    words[0] = 0x52534d31;
    words[1] = 0x10001;
    words[2] = 15;
  }
} device;
std::uint32_t rpcmp_boot_read32(std::uint32_t address) {
  if (address < 0x40000400 || address >= 0x40000800 || address % 4 != 0) {
    ++device.invalid;
    return 0;
  }
  const auto offset = address - 0x40000400;
  if (offset == 0x0c) {
    ++device.polls;
    if (device.inhibit_wait && !device.stuck_inhibit) {
      --device.inhibit_wait;
      if (device.inhibit_wait == 0)
        device.status &= ~0x400U;
    }
    if (device.control_wait && !device.stuck_control) {
      --device.control_wait;
      if (device.control_wait == 0) {
        device.status = device.reply == 1 ? 0x803 : 0x8c3;
        for (unsigned i = 0; i < 9; ++i)
          device.words[0x180 / 4 + i] = device.words[0x20 / 4 + i];
        device.words[0x1ac / 4] = device.reply;
        device.words[0x1b0 / 4] = 1;
        device.words[0x1b8 / 4] = 1;
      }
    }
    return device.status & (device.offline ? ~0x800U : ~0U);
  }
  if (offset >= 0x180 && (device.status & 2) == 0)
    ++device.invalid;
  return offset == device.corrupt_offset ? device.corrupt_value : device.words[offset / 4];
}
void rpcmp_boot_write32(std::uint32_t address, std::uint32_t value) {
  device.operations.push_back(address);
  if (address == 0x40000410 && value == 1) {
    device.status |= 0x480;
    device.inhibit_wait = 3;
  } else if (address >= 0x40000420 && address <= 0x40000440 && address % 4 == 0) {
    device.words[(address - 0x40000400) / 4] = value;
  } else if (address == 0x40000444 && value == 1) {
    if ((device.status & 0x333f) || (device.status & 0xc00) != 0x800)
      ++device.invalid;
    for (unsigned offset = 0x20; offset <= 0x40; offset += 4)
      if (device.words[offset / 4] != ((offset == 0x20 || offset == 0x28) ? 1U : 0U))
        ++device.invalid;
    ++device.submits;
    device.status |= 1;
    device.control_wait = 5;
  } else if (address == 0x40000448 && value == 1) {
    if ((device.status & 2) == 0)
      ++device.invalid;
    device.status &= ~3U;
    if (device.post_fault)
      device.status |= 0xc0;
    ++device.releases;
  } else {
    ++device.invalid;
  }
}
#ifndef RPCMP_PLAYER_BOOT_FILE
#define RPCMP_PLAYER_BOOT_FILE "../../overlays/openfpgaos/boot_m6_sound_reset.inc"
#endif
#include RPCMP_PLAYER_BOOT_FILE

void expect_failure(rpcmp::test::Suite& suite, bool completed = false) {
  RPCMP_CHECK(suite, rpcmp_boot_reset_sound(20) == 0);
  RPCMP_CHECK(suite, !device.operations.empty() && device.operations.back() == 0x40000410);
  RPCMP_CHECK(suite, device.polls <= 21 && device.invalid == 0);
  RPCMP_CHECK(suite, device.releases == (completed ? 1U : 0U));
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  RPCMP_CHECK(suite, rpcmp_boot_reset_sound(20) == 1);
  RPCMP_CHECK(suite, device.submits == 1 && device.releases == 1 && device.invalid == 0 &&
                         device.status == 0x800);
  for (const auto offset : {0x180U, 0x184U, 0x188U, 0x18cU, 0x190U, 0x194U, 0x198U, 0x19cU, 0x1a0U,
                            0x1a4U, 0x1a8U, 0x1b0U, 0x1b8U, 0x1bcU}) {
    device = {};
    device.corrupt_offset = offset;
    device.corrupt_value =
        (offset == 0x180 || offset == 0x188 || offset == 0x1b0 || offset == 0x1b8) ? 0U : 1U;
    expect_failure(suite, true);
  }
  for (unsigned reply = 2; reply <= 4; ++reply) {
    device = {};
    device.reply = reply;
    expect_failure(suite, true);
  }
  for (const auto busy : {1U, 2U, 4U, 8U, 16U, 32U, 256U, 512U, 4096U, 8192U}) {
    device = {};
    device.status |= busy;
    expect_failure(suite);
    RPCMP_CHECK(suite, device.submits == 0);
  }
  device = {};
  device.stuck_inhibit = true;
  expect_failure(suite);
  RPCMP_CHECK(suite, device.submits == 0);
  device = {};
  device.stuck_control = true;
  expect_failure(suite);
  RPCMP_CHECK(suite, device.submits == 1);
  device = {};
  device.offline = true;
  expect_failure(suite);
  device = {};
  device.post_fault = true;
  expect_failure(suite, true);
  for (unsigned field = 0; field < 3; ++field) {
    device = {};
    device.words[field] = 0;
    RPCMP_CHECK(suite, rpcmp_boot_reset_sound(20) == 0 && device.operations.empty());
  }
  device = {};
  RPCMP_CHECK(suite, rpcmp_boot_reset_sound(0) == 0 && device.polls == 0);
  return suite.finish("M6 ROM sound reset");
}
