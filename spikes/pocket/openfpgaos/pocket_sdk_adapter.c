#include "of.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t* target_read_stage;
static uint32_t target_read_stage_size;
static volatile int target_read_done;
static volatile int target_read_token;
static volatile int target_read_result;

static void target_read_callback(int token, int result) {
  target_read_token = token;
  target_read_result = result;
  target_read_done = 1;
}

static int slot_path(uint32_t slot_id, char* destination, size_t capacity) {
  const int length = snprintf(destination, capacity, "slot:%lu", (unsigned long)slot_id);
  return length >= 0 && (size_t)length < capacity ? 0 : -1;
}

int rpcmp_pocket_slot_size(uint32_t slot_id, uint32_t* size) {
  char path[24];
  if (size == NULL || slot_path(slot_id, path, sizeof(path)) != 0) {
    return -1;
  }

  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    return -1;
  }
  const int seek_result = fseek(file, 0, SEEK_END);
  const long measured = seek_result == 0 ? ftell(file) : -1;
  const int close_result = fclose(file);
  if (measured < 0 || (unsigned long)measured > UINT32_MAX || close_result != 0) {
    return -1;
  }
  *size = (uint32_t)measured;
  return 0;
}

int rpcmp_pocket_slot_read(uint32_t slot_id, uint32_t offset, void* destination, uint32_t length) {
  char path[24];
  if (destination == NULL || offset > LONG_MAX || slot_path(slot_id, path, sizeof(path)) != 0) {
    return -1;
  }

  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    return -1;
  }
  const int seek_result = fseek(file, (long)offset, SEEK_SET);
  const size_t read_count = seek_result == 0 ? fread(destination, 1, length, file) : 0;
  const int close_result = fclose(file);
  return read_count == length && close_result == 0 ? 0 : -1;
}

int rpcmp_pocket_target_read_prepare(uint32_t length) {
  if (length == 0U || length > of_file_async_max_read()) {
    return -1;
  }
  if (target_read_stage == NULL) {
    target_read_stage = of_file_dma_stage_alloc(length, 64U);
    target_read_stage_size = target_read_stage == NULL ? 0U : length;
  }
  return target_read_stage != NULL && length <= target_read_stage_size ? 0 : -1;
}

int rpcmp_pocket_target_read(uint32_t slot_id, uint32_t offset, void* destination,
                             uint32_t length, uint32_t* elapsed_us) {
  if (destination == NULL || elapsed_us == NULL || target_read_stage == NULL || length == 0U ||
      length > target_read_stage_size) {
    return -1;
  }

  const uint32_t retry_started = of_time_us();
  uint32_t started;
  int token;
  do {
    target_read_done = 0;
    target_read_token = -1;
    target_read_result = -1;
    started = of_time_us();
    token =
        of_file_read_async((int)slot_id, offset, target_read_stage, length, target_read_callback);
    if (token < 0 && of_time_us() - retry_started > 100000U) {
      return -1;
    }
  } while (token < 0);

  uint32_t spins = 0U;
  while (!target_read_done) {
    ++spins;
    if ((spins & 0x3FFFU) == 0U && of_time_us() - started > 2500000U) {
      return -1;
    }
  }
  *elapsed_us = of_time_us() - started;
  if (target_read_token != token || target_read_result != 0) {
    return -1;
  }
  memcpy(destination, target_read_stage, length);
  return 0;
}

uint32_t rpcmp_pocket_time_us(void) {
  return of_time_us();
}

uint32_t rpcmp_pocket_poll_actions(void) {
  uint32_t actions = 0;
  of_input_poll_p0();
  if (of_btn_pressed(OF_BTN_A)) {
    actions |= 1U << 0;
  }
  if (of_btn_pressed(OF_BTN_B)) {
    actions |= 1U << 1;
  }
  if (of_btn_pressed(OF_BTN_START)) {
    actions |= 1U << 2;
  }
  return actions;
}

void rpcmp_pocket_wait_vblank(void) {
  OF_SVC->video_vsync();
}

void rpcmp_pocket_terminal_init(void) {
  of_video_init();
  of_video_set_display_mode(OF_DISPLAY_TERMINAL);
  printf("\033[2J\033[H");
}

_Noreturn void rpcmp_pocket_hold_result(void) {
  for (;;) {
    rpcmp_pocket_wait_vblank();
  }
}
