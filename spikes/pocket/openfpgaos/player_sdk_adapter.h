#ifndef RPCMP_PLAYER_SDK_ADAPTER_H
#define RPCMP_PLAYER_SDK_ADAPTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
uint32_t rpcmp_player_cpu_hz(void);
uint32_t rpcmp_player_mmio_read(uintptr_t address);
void rpcmp_player_mmio_write(uintptr_t address, uint32_t value);
int rpcmp_player_video_init(const uint32_t* palette, uint32_t count);
uint8_t* rpcmp_player_video_surface(void);
int rpcmp_player_video_present(void); /* 0 presented, 1 busy */
int rpcmp_pocket_slot_size(uint32_t slot, uint32_t* size);
int rpcmp_pocket_target_read_prepare(uint32_t length);
int rpcmp_pocket_target_read(uint32_t slot, uint32_t offset, void* output, uint32_t length,
                             uint32_t* elapsed_us);
void rpcmp_pocket_terminal_init(void);
void rpcmp_pocket_hold_result(void);
#ifdef __cplusplus
}
#endif
#endif
