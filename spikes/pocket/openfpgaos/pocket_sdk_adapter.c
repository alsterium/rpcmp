#include "of.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>

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

void rpcmp_pocket_terminal_init(void) {
  of_video_init();
  of_video_set_display_mode(OF_DISPLAY_TERMINAL);
  printf("\033[2J\033[H");
}

_Noreturn void rpcmp_pocket_hold_result(void) {
  for (;;) {
    OF_SVC->video_vsync();
  }
}
