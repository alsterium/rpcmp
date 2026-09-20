/* ROM-only, single owner, after common reset. No Start/feed or OS calls.
 * Reset Success itself proves the session's physical reset/quiescence bound.
 * A failure leaves INHIBIT requested and the boot caller stays in ROM. */
static int rpcmp_boot_reset_sound(uint32_t poll_limit) {
    if (rpcmp_boot_read32(0x40000400u) != 0x52534D31u ||
        rpcmp_boot_read32(0x40000404u) != 0x00010001u ||
        rpcmp_boot_read32(0x40000408u) != 0x0000000Fu)
        return 0;
    rpcmp_boot_write32(0x40000410u, 1u);
    uint32_t remaining = poll_limit;
    uint32_t status;
    uint32_t epoch_lo, epoch_hi;
    int valid;
    do {
        if (remaining == 0)
            goto failed;
        --remaining;
        status = rpcmp_boot_read32(0x4000040Cu);
    } while (!(status & 0x800u) || (status & 0x400u));
    /* Never steal an existing owner's request or copied completion. */
    if (status & 0x333Fu)
        goto failed;
    for (uint32_t offset = 0x20u; offset <= 0x40u; offset += 4u)
        rpcmp_boot_write32(0x40000400u + offset,
                          (offset == 0x20u || offset == 0x28u) ? 1u : 0u);
    rpcmp_boot_write32(0x40000444u, 1u);
    do {
        if (remaining == 0)
            goto failed;
        --remaining;
        status = rpcmp_boot_read32(0x4000040Cu);
    } while (!(status & 2u));
    valid = (status & 0x700u) == 0u && (status & 0x800u) != 0u;
    for (uint32_t offset = 0x180u; offset <= 0x1A8u; offset += 4u) {
        const uint32_t expected = (offset == 0x180u || offset == 0x188u) ? 1u : 0u;
        if (rpcmp_boot_read32(0x40000400u + offset) != expected)
            valid = 0;
    }
    valid = valid && rpcmp_boot_read32(0x400005ACu) == 1u;
    epoch_lo = rpcmp_boot_read32(0x400005B0u);
    epoch_hi = rpcmp_boot_read32(0x400005B4u);
    valid = valid && (epoch_lo != 0u || epoch_hi != 0u) &&
            rpcmp_boot_read32(0x400005B8u) == 1u &&
            rpcmp_boot_read32(0x400005BCu) == 0u;
    rpcmp_boot_write32(0x40000448u, 1u);
    status = rpcmp_boot_read32(0x4000040Cu);
    if (valid && (status & 0xFC3u) == 0x800u)
        return 1;
failed:
    rpcmp_boot_write32(0x40000410u, 1u);
    return 0;
}
