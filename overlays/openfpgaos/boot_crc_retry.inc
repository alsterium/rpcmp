/* RPCMP M5: bounded CRC retries. Included by the pinned Pocket boot.c and
 * the host regression harness. All reads and CRC checks use the boot ports. */
static int boot_load_os_sd(uint32_t total) {
    for (unsigned attempt = 0; attempt < 2u * OS_LOAD_MAX_ATTEMPTS; ++attempt) {
        int rc = boot_load_os_sd_once_to(
            total, (uint32_t)(uintptr_t)_osdata_init_vma_start);
        if (rc < 0)
            return rc;
        if (boot_verify_os_image_at(
                total, (uint32_t)(uintptr_t)_osdata_init_vma_start))
            return 0;
        ++os_load_crc_retries;
        pd_dbg_info = 0xC0DE0000u | (os_load_crc_retries & 0xFFFFu);
        boot_fb_puts0(1, "OS CRC retry");
        boot_fb_putchar(13, 1, '0' + os_load_crc_retries);
    }
    return -5;
}
