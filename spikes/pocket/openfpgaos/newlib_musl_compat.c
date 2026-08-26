/*
 * Minimal newlib-to-musl compatibility symbols required when xPack's
 * libstdc++/libsupc++ are linked with the openfpgaOS SDK musl runtime.
 * This follows the ABI combination proven by openfpgaOS/Diablo at
 * c4f1d24ad9db011dcfd5f3b1d90423ceb28cfd17 without importing its player.
 */

#include <stddef.h>

extern int *__errno_location(void);

static unsigned char rpcmp_fake_reent[1024] __attribute__((aligned(16)));
void *_impure_ptr = rpcmp_fake_reent;
void *__dso_handle = NULL;

int *__errno(void) { return __errno_location(); }

int __locale_mb_cur_max(void) { return 1; }
