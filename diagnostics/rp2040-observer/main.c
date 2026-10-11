/* Temporary observation tool: GPIO0..29 are inputs, with pulls disabled.
 * No radio TX, reset, download, flash program or erase command exists here.
 * PIO UART sampling follows Raspberry Pi pico-examples uart_rx (BSD-3-Clause),
 * with pull-up removed to preserve high impedance.
 */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/flash.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"
#include "hardware/clocks.h"
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

static void identify(void) {
    uint8_t tx[4] = {0x9f, 0, 0, 0}, rx[4];
    uint8_t uid_tx[13] = {0x4b}, uid_rx[13];
    read_flash_identity(tx, rx, sizeof(tx));
    read_flash_identity(uid_tx, uid_rx, sizeof(uid_tx));
    printf("INFO jedec=%02x%02x%02x flash_uid=", rx[1], rx[2], rx[3]);
    for (unsigned i = 5; i < sizeof(uid_rx); ++i) printf("%02x", uid_rx[i]);
    printf(" gpio=%08lx output_enable=%08lx\n", (unsigned long)gpio_get_all(),
           (unsigned long)sio_hw->gpio_oe);
}

static void edges(unsigned ms) {
    uint32_t counts[30] = {0};
    uint32_t prev = gpio_get_all();
    absolute_time_t end = make_timeout_time_ms(ms);
    while (!time_reached(end)) {
        uint32_t now = gpio_get_all(), changed = now ^ prev;
        for (unsigned pin = 0; pin < 30; ++pin) {
            if (changed & (1u << pin)) counts[pin]++;
        }
        prev = now;
    }
    printf("EDGES ms=%u idle=%08lx", ms, (unsigned long)prev);
    for (unsigned pin = 0; pin < 30; ++pin) printf(" %u:%lu", pin, (unsigned long)counts[pin]);
    puts("");
}

static void capture(unsigned pin, unsigned baud, unsigned ms) {
    uint16_t code[] = {
        pio_encode_wait_pin(false, 0),
        pio_encode_set(pio_x, 7) | pio_encode_delay(10),
        pio_encode_in(pio_pins, 1),
        pio_encode_jmp_x_dec(2) | pio_encode_delay(6),
        pio_encode_jmp_pin(8),
        pio_encode_irq_set(true, 4),
        pio_encode_wait_pin(true, 0),
        pio_encode_jmp(0),
        pio_encode_push(false, true)
    };
    struct pio_program program = {.instructions = code, .length = 9, .origin = -1};
    PIO pio = pio0;
    unsigned sm = 0, offset = pio_add_program(pio, &program);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, false);
    pio_gpio_init(pio, pin);
    gpio_disable_pulls(pin);
    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_wrap(&config, offset, offset + 8);
    sm_config_set_in_pins(&config, pin);
    sm_config_set_jmp_pin(&config, pin);
    sm_config_set_in_shift(&config, true, false, 32);
    sm_config_set_fifo_join(&config, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&config, (float)clock_get_hz(clk_sys) / (8.0f * (float)baud));
    pio_sm_init(pio, sm, offset, &config);
    pio_interrupt_clear(pio, 4);
    pio_sm_set_enabled(pio, sm, true);
    static uint8_t bytes[4096];
    unsigned count = 0, discarded = 0;
    absolute_time_t end = make_timeout_time_ms(ms);
    while (!time_reached(end)) {
        if (!pio_sm_is_rx_fifo_empty(pio, sm)) {
            uint8_t byte = (uint8_t)(pio_sm_get(pio, sm) >> 24);
            if (count < sizeof(bytes)) bytes[count++] = byte; else discarded++;
        }
    }
    pio_sm_set_enabled(pio, sm, false);
    printf("RX pin=%u baud=%u ms=%u count=%u discarded=%u framing_error_seen=%u hex=",
           pin, baud, ms, count, discarded, (unsigned)pio_interrupt_get(pio, 4));
    for (unsigned i = 0; i < count; ++i) printf("%02x", bytes[i]);
    puts("");
    pio_remove_program(pio, &program, offset);
    gpio_init(pin);
    gpio_disable_pulls(pin);
}

int main(void) {
    for (unsigned pin = 0; pin < 30; ++pin) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_disable_pulls(pin);
    }
    flash_start_xip();
    stdio_init_all();
    char line[96];
    unsigned used = 0;
    while (true) {
        int ch = getchar_timeout_us(1000);
        if (ch == PICO_ERROR_TIMEOUT) continue;
        if (ch == '\n' || ch == '\r') {
            if (!used) continue;
            line[used] = '\0'; used = 0;
            unsigned pin, baud, ms;
            if (!strcmp(line, "INFO")) identify();
            else if (!strcmp(line, "BOOTSEL")) reset_usb_boot(0, 0);
            else if (sscanf(line, "EDGES %u", &ms) == 1 && ms >= 1 && ms <= 10000) edges(ms);
            else if (sscanf(line, "RX %u %u %u", &pin, &baud, &ms) == 3 &&
                     pin < 30 && baud >= 9600 && baud <= 1000000 && ms >= 1 && ms <= 10000)
                capture(pin, baud, ms);
            else puts("ERROR commands: INFO | EDGES 1..10000 | RX pin baud ms | BOOTSEL");
        } else if (used + 1 < sizeof(line)) line[used++] = (char)ch;
        else { used = 0; puts("ERROR line too long"); }
    }
}
