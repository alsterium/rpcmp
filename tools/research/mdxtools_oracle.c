#include "adpcm_driver.h"
#include "fm_opm_driver.h"
#include "mdx.h"
#include "mdx_driver.h"
#include "timer_driver.h"
#include "vgm_logger.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int oracle_tick;
static int oracle_sequence;

static int hex_nibble(const int character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

static int read_hex(const char* path, uint8_t* bytes, const int capacity) {
  FILE* input = fopen(path, "rb");
  if (input == NULL) {
    return -1;
  }
  int count = 0;
  int high = -1;
  for (;;) {
    const int character = fgetc(input);
    if (character == EOF) {
      break;
    }
    const int nibble = hex_nibble(character);
    if (nibble < 0) {
      continue;
    }
    if (high < 0) {
      high = nibble;
    } else {
      if (count == capacity) {
        fclose(input);
        return -1;
      }
      bytes[count++] = (uint8_t)((high << 4) | nibble);
      high = -1;
    }
  }
  fclose(input);
  return high < 0 ? count : -1;
}

static void capture_write(struct fm_opm_driver* driver, const uint8_t reg, const uint8_t value) {
  (void)driver;
  printf("W %d %d %02x %02x\n", oracle_tick, oracle_sequence++, reg, value);
}

static void capture_tempo(struct mdx_driver* driver, const int value, void* data_ptr) {
  (void)driver;
  (void)data_ptr;
  printf("T %d %d %02x\n", oracle_tick, oracle_sequence++, value);
}

int vgm_logger_write_ym2151(struct vgm_logger* logger, const uint8_t reg, const uint8_t value) {
  (void)logger;
  (void)reg;
  (void)value;
  return 0;
}

int main(const int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "usage: mdxtools_oracle fixture.mdx.hex\n");
    return 2;
  }
  uint8_t bytes[4096];
  const int size = read_hex(argv[1], bytes, (int)sizeof(bytes));
  if (size < 0) {
    fprintf(stderr, "invalid fixture\n");
    return 3;
  }

  struct mdx_file file;
  if (mdx_file_load(&file, bytes, size) != MDX_SUCCESS) {
    fprintf(stderr, "mdxtools rejected fixture\n");
    return 4;
  }
  struct timer_driver timer;
  struct fm_opm_driver fm;
  struct adpcm_driver adpcm;
  struct mdx_driver driver;
  timer_driver_init(&timer);
  fm_opm_driver_init(&fm, NULL);
  fm.write = capture_write;
  adpcm_driver_init(&adpcm);
  mdx_driver_init(&driver, &timer, (struct fm_driver*)&fm, &adpcm);
  driver.set_tempo = capture_tempo;
  mdx_driver_load(&driver, &file, NULL);

  for (oracle_tick = 0; oracle_tick < 5; ++oracle_tick) {
    oracle_sequence = 0;
    mdx_driver_tick(&driver);
  }
  return 0;
}
