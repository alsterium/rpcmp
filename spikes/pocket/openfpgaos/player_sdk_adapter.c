#include "player_sdk_adapter.h"
#include "of.h"

uint32_t rpcmp_player_cpu_hz(void) {
  const struct of_capabilities* caps = of_get_caps();
  if (!caps || caps->magic != OF_CAPS_MAGIC || caps->version < 2 ||
      caps->platform_id != OF_PLATFORM_POCKET || caps->gpu_base != 0)
    return 0;
  return caps->cpu_freq_hz;
}

int rpcmp_player_video_init(const uint32_t* palette, uint32_t count) {
  const of_video_mode_t requested = {640, 480, 640, OF_VIDEO_MODE_8BIT, 0};
  of_video_mode_t actual;
  if (!palette || count == 0 || count > 256)
    return -1;
  of_video_init();
  if (of_video_set_mode(&requested) != 0)
    return -1;
  of_video_get_mode(&actual);
  if (actual.width != 640 || actual.height != 480 || actual.stride != 640 ||
      actual.color_mode != OF_VIDEO_MODE_8BIT)
    return -1;
  of_video_palette_bulk(palette, (int)count);
  of_video_set_display_mode(OF_DISPLAY_FRAMEBUFFER);
  return 0;
}

uint8_t* rpcmp_player_video_surface(void) { return of_video_surface(); }

int rpcmp_player_video_present(void) {
  /* The pinned Pocket OS triple buffer owns the current draw surface. Do not
   * replace a queued swap or wait for vsync while audio mailboxes are live.
   * video_flip still cleans the complete frame; measure that CPU cost on target. */
  if ((*(volatile const uint32_t*)(uintptr_t)0x40000018 & 1u) != 0)
    return 1;
  of_video_flip();
  return 0;
}
