/* HYB1 ROM failure path: clear audio and wait for acknowledged reset/release. */
static int rpcmp_boot_reset_sound(uint32_t poll_limit) {
    if (rpcmp_boot_read32(0x40000400u) != 0x48594233u)
        return 0;
    while (!(rpcmp_boot_read32(0x40000404u) & 1u)) {
        if (poll_limit-- == 0)
            return 0;
    }
    rpcmp_boot_write32(0x40000408u, 1u);
    while (!(rpcmp_boot_read32(0x40000404u) & 1u)) {
        if (poll_limit-- == 0)
            return 0;
    }
    return rpcmp_boot_read32(0x40000404u) == 1u;
}
