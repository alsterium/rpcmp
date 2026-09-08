#include "test_support.hpp"

#include <cstdint>

namespace {
unsigned reads{};
unsigned checks{};
unsigned success_on{};
unsigned io_failure_on{};
std::uint32_t os_load_crc_retries{};
std::uint32_t pd_dbg_info{};
char _osdata_init_vma_start[4]{};
constexpr unsigned OS_LOAD_MAX_ATTEMPTS = 4;

int boot_load_os_sd_once_to(std::uint32_t, std::uint32_t) {
  ++reads;
  return reads == io_failure_on ? -2 : 0;
}
int boot_verify_os_image_at(std::uint32_t, std::uint32_t) {
  ++checks;
  return checks == success_on;
}
void boot_fb_puts0(int, const char*) {}
void boot_fb_putchar(int, int, unsigned) {}

#ifndef RPCMP_BOOT_RETRY_FILE
#define RPCMP_BOOT_RETRY_FILE "../../overlays/openfpgaos/boot_crc_retry.inc"
#endif
#include RPCMP_BOOT_RETRY_FILE

struct Sound {
  std::uint32_t generation{7};
  std::uint32_t queue_status{};
  std::uint32_t audio_status{};
  unsigned writes{};
  unsigned polls{};
  bool wrong_id{};
  bool stuck{};
  bool initially_busy{};
  bool invalid{};
  bool no_generation{};
} sound;

std::uint32_t rpcmp_boot_read32(std::uint32_t address) {
  switch (address) {
  case 0x40000200:
    return sound.wrong_id ? 0 : 0x52514D31;
  case 0x40000204:
    return 0x00010008;
  case 0x40000240:
    return 0x52534331;
  case 0x40000244:
    return 0x00010100;
  case 0x40000208:
    return sound.queue_status;
  case 0x40000250:
    return sound.audio_status;
  case 0x40000248:
    ++sound.polls;
    return (sound.generation << 16U) |
           ((sound.initially_busy || (sound.writes != 0 && sound.stuck)) ? 1U : 0U) |
           ((sound.writes != 0 && sound.invalid) ? 2U : 0U);
  default:
    return 0xFFFFFFFF;
  }
}
void rpcmp_boot_write32(std::uint32_t address, std::uint32_t value) {
  if (address == 0x4000024C && value == 3) {
    ++sound.writes;
    if (!sound.no_generation)
      sound.generation = (sound.generation + 1U) & 0xFFFFU;
  }
}
#include "../../overlays/openfpgaos/boot_sound_reset.inc"

void reset_case(unsigned success, unsigned io_failure) {
  reads = 0;
  checks = 0;
  success_on = success;
  io_failure_on = io_failure;
  os_load_crc_retries = 0;
  pd_dbg_info = 0;
}
} // namespace

int main() {
  rpcmp::test::Suite suite;
  reset_case(0, 0);
  RPCMP_CHECK(suite, boot_load_os_sd(256) < 0);
  RPCMP_CHECK(suite, reads == 8 && checks == 8 && os_load_crc_retries == 8);

  for (unsigned attempt = 1; attempt <= 8; ++attempt) {
    reset_case(attempt, 0);
    RPCMP_CHECK(suite, boot_load_os_sd(256) == 0);
    RPCMP_CHECK(suite, reads == attempt && checks == attempt);
    RPCMP_CHECK(suite, os_load_crc_retries == attempt - 1);

    reset_case(0, attempt);
    RPCMP_CHECK(suite, boot_load_os_sd(256) == -2);
    RPCMP_CHECK(suite, reads == attempt && checks == attempt - 1);
  }
  for (const auto generation : {7U, 65535U}) {
    sound = {};
    sound.generation = generation;
    RPCMP_CHECK(suite, rpcmp_boot_reset_sound(8) == 1);
    RPCMP_CHECK(suite, sound.writes == 1 && sound.polls == 2);
  }
  for (unsigned fault = 0; fault < 7; ++fault) {
    sound = {};
    sound.wrong_id = fault == 0;
    sound.stuck = fault == 1;
    sound.initially_busy = fault == 2;
    sound.invalid = fault == 3;
    sound.no_generation = fault == 4;
    sound.queue_status = fault == 5 ? 0x800 : 0;
    sound.audio_status = fault == 6 ? 1 : 0;
    RPCMP_CHECK(suite, rpcmp_boot_reset_sound(8) == 0);
    RPCMP_CHECK(suite, sound.polls <= 8);
    RPCMP_CHECK(suite, sound.writes == (fault == 0 || fault == 2 ? 0U : 1U));
  }
  sound = {};
  RPCMP_CHECK(suite, rpcmp_boot_reset_sound(0) == 0 && sound.writes == 0);
  return suite.finish("boot_crc");
}
