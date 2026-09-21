#ifndef RPCMP_HYBRID_RENDERER_H
#define RPCMP_HYBRID_RENDERER_H

#include <stdint.h>

enum { RPCMP_HYBRID_FRAMES = 1536, RPCMP_HYBRID_EVENTS = 1024 };
typedef struct {
  uint32_t sample;
  uint32_t operation;
} RpcmpHybridEvent;
typedef struct {
  uint32_t first_frame;
  uint32_t frame_count;
  uint32_t event_count;
  uint32_t ended;
  RpcmpHybridEvent events[RPCMP_HYBRID_EVENTS];
  int32_t pcm[RPCMP_HYBRID_FRAMES * 2];
} RpcmpHybridBlock;

#ifdef __cplusplus
extern "C" {
#endif
// Input blobs have the reference's ten-byte wrapper and sixteen zero guard
// bytes beyond each logical length. Load/validate them before starting audio.
int rpcmp_hybrid_open(const void* mdx, uint32_t mdx_size, const void* pdx, uint32_t pdx_size);
int rpcmp_hybrid_authored(unsigned pcm_kind);
// The copied block remains unchanged until the next call. NULL means failure
// or a completed/closed stream. EOF may have zero frames. Event 0x10001 starts
// the HYB2 five-second envelope; other events are YM register/value pairs.
const RpcmpHybridBlock* rpcmp_hybrid_render(void);
void rpcmp_hybrid_close(void);
#ifdef __cplusplus
}
#endif

#endif
