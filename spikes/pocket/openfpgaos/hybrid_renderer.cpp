#include "hybrid_renderer.h"

#include <cstdint>
#include <cstring>

static void hybrid_capture_event(std::uint32_t address, std::uint32_t value);
static void hybrid_progress();
extern "C" void rpcmp_hybrid_pcm(std::int32_t value);
#define MXDRVG_EXPORT
#define MXDRVG_CALLBACK
volatile unsigned char OpmReg1B;
// Pinned, generated reference source; the original checkout stays unchanged.
#include "mxdrvg/mxdrvg_core.h"

namespace {
RpcmpHybridBlock block;
std::uint32_t scalars;
bool failed, initialized, first_block, file_loaded;
bool fading, complete;
std::uint32_t stop_frame;
std::int16_t discarded[2048];
unsigned char authored_pcm[65536];

int initialize(std::uint32_t mdx_size, std::uint32_t pdx_size) {
  rpcmp_hybrid_close();
  std::memset(&block, 0, sizeof(block));
  scalars = 0;
  failed = false;
  first_block = true;
  file_loaded = false;
  fading = complete = false;
  stop_frame = UINT32_MAX;
  if (MXDRVG_Start(48000, 0, static_cast<int>(mdx_size), static_cast<int>(pdx_size)) != 0) {
    MXDRVG_End();
    return -1;
  }
  initialized = true;
  MXDRVG_TotalVolume(256);
  return 0;
}

void authored_register(unsigned address, unsigned value) {
  hybrid_capture_event(address, value);
  OPM.SetReg(address, value);
}
} // namespace

static void hybrid_event(std::uint32_t operation) {
  if (block.event_count == RPCMP_HYBRID_EVENTS) {
    failed = true;
    return;
  }
  block.events[block.event_count++] = {block.first_frame + scalars / 2, operation};
}
static void hybrid_capture_event(std::uint32_t address, std::uint32_t value) {
  if (address > 255 || value > 255) {
    failed = true;
    return;
  }
  hybrid_event((address << 8) | value);
}
static void hybrid_progress() {
  if (!file_loaded)
    return;
  const auto frame = block.first_frame + scalars / 2;
  if (MXDRVG_GetTerminated()) {
    if (frame < stop_frame)
      stop_frame = frame;
  } else if (!fading && G.L002246 >= 2) {
    if (frame > UINT32_MAX - 312500U) {
      failed = true;
      return;
    }
    fading = true;
    stop_frame = frame + 312500U;
    hybrid_event(0x10001);
  }
}

extern "C" void rpcmp_hybrid_pcm(std::int32_t value) {
  if (scalars == RPCMP_HYBRID_FRAMES * 2) {
    failed = true;
    return;
  }
  block.pcm[scalars++] = value;
}

extern "C" void rpcmp_hybrid_close() {
  if (initialized) {
    MXDRVG_End();
    initialized = false;
  }
}

extern "C" int rpcmp_hybrid_open(const void* mdx, std::uint32_t mdx_size, const void* pdx,
                                 std::uint32_t pdx_size) {
  if (!mdx || mdx_size < 10 || mdx_size > 16 * 1024 * 1024 ||
      (pdx_size != 0 && (!pdx || pdx_size < 10)) || pdx_size > 16 * 1024 * 1024) {
    return -1;
  }
  if (initialize(mdx_size + 16, pdx_size + 16) != 0) {
    return -1;
  }
  MXDRVG_SetData(const_cast<void*>(mdx), mdx_size, pdx_size ? const_cast<void*>(pdx) : nullptr,
                 pdx_size);
  if (!G.L002230 || (pdx_size != 0 && !G.L002231)) {
    rpcmp_hybrid_close();
    return -1;
  }
  file_loaded = true;
  MXDRVG_PlayAt(0, 65534, 0); // HYB2 owns the fixed two-loop/five-second envelope.
  if (failed || G.FATALERROR) {
    rpcmp_hybrid_close();
    return -1;
  }
  return 0;
}

extern "C" int rpcmp_hybrid_authored(unsigned pcm_kind) {
  if (pcm_kind < 4 || pcm_kind > 6 || initialize(65536, 1048576) != 0) {
    return -1;
  }
  for (unsigned ch = 0; ch < 8; ++ch) {
    authored_register(0x20 + ch, 0xc7);
    authored_register(0x28 + ch, 0x30 + ch);
    for (unsigned op = 0; op < 4; ++op) {
      const unsigned at = op * 8 + ch;
      authored_register(0x40 + at, 1 + op);
      authored_register(0x60 + at, 48);
      authored_register(0x80 + at, 31);
      authored_register(0xa0 + at, 0);
      authored_register(0xc0 + at, 0);
      authored_register(0xe0 + at, 15);
    }
  }
  for (unsigned ch = 0; ch < 8; ++ch) {
    authored_register(8, 0x78 | ch);
  }
  for (unsigned i = 0; i < sizeof(authored_pcm); ++i) {
    if (pcm_kind == 4) {
      authored_pcm[i] = (i & 32U) ? 0x99 : 0x11;
    } else if (pcm_kind == 5) {
      const int value = (static_cast<int>((i / 2) % 64) - 32) * 64;
      authored_pcm[i] = static_cast<unsigned char>((i & 1U) ? value & 255 : (value >> 8) & 255);
    } else {
      authored_pcm[i] = static_cast<unsigned char>((i % 64) - 32);
    }
  }
  for (int ch = 0; ch < 8; ++ch) {
    PCM8.Out(ch, authored_pcm, (2 << 16) | (static_cast<int>(pcm_kind) << 8) | 3,
             sizeof(authored_pcm));
  }
  return failed ? -1 : 0;
}

extern "C" const RpcmpHybridBlock* rpcmp_hybrid_render() {
  if (!initialized || failed || complete ||
      block.first_frame > UINT32_MAX - RPCMP_HYBRID_FRAMES * 2) {
    return nullptr;
  }
  if (!first_block) {
    block.first_frame += block.frame_count;
    block.event_count = 0;
    scalars = 0;
  }
  first_block = false;
  if (MXDRVG_GetPCM(discarded, 1024) != 1024 || failed || G.FATALERROR || (scalars & 1U)) {
    return nullptr;
  }
  block.frame_count = scalars / 2;
  hybrid_progress();
  if (failed)
    return nullptr;
  block.ended = stop_frame <= block.first_frame + block.frame_count;
  if (block.ended) {
    block.frame_count = stop_frame - block.first_frame;
    // The driver may have rendered ahead inside its final bounded chunk.
    while (block.event_count && block.events[block.event_count - 1].sample >= stop_frame)
      --block.event_count;
    complete = true;
  }
  return &block;
}
