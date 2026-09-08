/* The boot failure path never submits playback work. Reset the sound path
 * through the existing v1 ports before the ROM's terminal hold. */
static int rpcmp_boot_reset_sound(uint32_t poll_limit) {
    if (rpcmp_boot_read32(0x40000200u) != 0x52514D31u ||
        rpcmp_boot_read32(0x40000204u) != 0x00010008u ||
        rpcmp_boot_read32(0x40000240u) != 0x52534331u ||
        rpcmp_boot_read32(0x40000244u) != 0x00010100u)
        return 0;
    uint32_t status = 0;
    uint32_t remaining = poll_limit;
    do {
        if (remaining == 0)
            return 0;
        --remaining;
        status = rpcmp_boot_read32(0x40000248u);
    } while (status & 1u);
    const uint32_t generation = ((status >> 16) + 1u) & 0xFFFFu;
    rpcmp_boot_write32(0x4000024Cu, 3u);
    while (remaining != 0) {
        --remaining;
        status = rpcmp_boot_read32(0x40000248u);
        if (status & 2u)
            return 0;
        if (!(status & 1u) && (status >> 16) == generation)
            return (rpcmp_boot_read32(0x40000208u) & 0xF0Fu) == 0u &&
                   (rpcmp_boot_read32(0x40000250u) & 7u) == 0u;
    }
    return 0;
}
