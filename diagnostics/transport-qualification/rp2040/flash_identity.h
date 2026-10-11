/* Reuse the accepted FNR-002 SRAM read-only flash identity procedure.
 * pico_unique_id intentionally returns 0xee placeholders with PICO_NO_FLASH.
 */
#ifndef FNR_QUALIFICATION_FLASH_IDENTITY_H
#define FNR_QUALIFICATION_FLASH_IDENTITY_H
#include "pico/bootrom.h"
#include "hardware/sync.h"
#include "hardware/structs/ssi.h"
#include "hardware/structs/io_qspi.h"
/* SDK 2.2.0 excludes flash_do_cmd in no_flash builds. This RAM-only subset
 * of its RP2040 SSI loop accepts only the read-only JEDEC and unique-ID ops.
 * Based on hardware_flash/flash.c, copyright 2020 Raspberry Pi (Trading) Ltd.,
 * SPDX-License-Identifier: BSD-3-Clause.
 */
static void read_flash_identity(const uint8_t *tx, uint8_t *rx, size_t count) {
    if (tx[0] != 0x9f && tx[0] != 0x4b) return;
    rom_flash_exit_xip_fn exit_xip = (rom_flash_exit_xip_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_EXIT_XIP);
    rom_flash_flush_cache_fn flush = (rom_flash_flush_cache_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_FLUSH_CACHE);
    rom_flash_enter_cmd_xip_fn enter_xip = (rom_flash_enter_cmd_xip_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_ENTER_CMD_XIP);
    uint32_t saved = save_and_disable_interrupts();
    exit_xip();
    hw_write_masked(&io_qspi_hw->io[1].ctrl,
                   IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_VALUE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_LSB,
                   IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_BITS);
    size_t tx_remaining = count, rx_remaining = count;
    while (tx_remaining || rx_remaining) {
        uint32_t flags = ssi_hw->sr;
        if ((flags & SSI_SR_TFNF_BITS) && tx_remaining && rx_remaining - tx_remaining < 14) {
            ssi_hw->dr0 = *tx++; --tx_remaining;
        }
        if ((flags & SSI_SR_RFNE_BITS) && rx_remaining) {
            *rx++ = (uint8_t)ssi_hw->dr0; --rx_remaining;
        }
    }
    hw_write_masked(&io_qspi_hw->io[1].ctrl,
                   IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_VALUE_HIGH << IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_LSB,
                   IO_QSPI_GPIO_QSPI_SS_CTRL_OUTOVER_BITS);
    flush(); enter_xip();
    restore_interrupts(saved);
}

static void qualification_flash_uid(char out[17]) {
    uint8_t tx[13] = {0x4b}, rx[13];
    read_flash_identity(tx, rx, sizeof(tx));
    const char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < 8; ++i) {
        out[2 * i] = hex[rx[5 + i] >> 4];
        out[2 * i + 1] = hex[rx[5 + i] & 15u];
    }
    out[16] = 0;
}
#endif
