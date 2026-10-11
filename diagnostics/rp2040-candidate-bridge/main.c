/* Diagnostic/recovery only. No radio reset/download GPIO, no flash operations.
 * Candidate GP0 TX / GP1 RX comes from cited prior art plus passive boot-mode
 * observations. ROM bidirectional identification must qualify it on each board.
 */
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/bootrom.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include <stdio.h>
#include "tusb.h"

#define RX_CAPACITY 8192u
static uint8_t ring[RX_CAPACITY];
static volatile uint32_t head, tail;
static volatile uint32_t rx_bytes, tx_bytes, software_drops, overruns, framing, parity, breaks;
static volatile bool status_requested;

static void uart_rx_irq(void) {
    while (uart_is_readable(uart0)) {
        uint32_t value = uart_get_hw(uart0)->dr;
        if (value & UART_UARTDR_OE_BITS) overruns++;
        if (value & UART_UARTDR_FE_BITS) framing++;
        if (value & UART_UARTDR_PE_BITS) parity++;
        if (value & UART_UARTDR_BE_BITS) breaks++;
        rx_bytes++;
        uint32_t next = (head + 1u) & (RX_CAPACITY - 1u);
        if (next == tail) software_drops++;
        else { ring[head] = (uint8_t)value; __dmb(); head = next; }
    }
}

// USB BREAK requests diagnostic counters; it never generates a radio break.
// esptool no_reset does not use this request. Query only between transactions.
void tud_cdc_send_break_cb(uint8_t itf, uint16_t duration) {
    (void)itf;
    if (duration) status_requested = true;
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *coding) {
    (void)itf;
    if (coding->bit_rate == 1200) reset_usb_boot(0, 0);
    if (coding->bit_rate >= 9600 && coding->bit_rate <= 921600)
        uart_set_baudrate(uart0, coding->bit_rate);
}

int main(void) {
    uart_init(uart0, 115200);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_hw_flow(uart0, false, false);
    gpio_set_drive_strength(0, GPIO_DRIVE_STRENGTH_2MA);
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);
    gpio_disable_pulls(0);
    gpio_disable_pulls(1);
    irq_set_exclusive_handler(UART0_IRQ, uart_rx_irq);
    irq_set_priority(UART0_IRQ, 0x40);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);
    stdio_init_all();
    stdio_set_translate_crlf(&stdio_usb, false);
    char bytes[64];
    while (true) {
        // One foreground owner for TinyUSB: no stdio background task and no
        // concurrent FIFO reader. Advance ring tail only for accepted bytes.
        tud_task();
        unsigned count = 0;
        uint32_t cursor = tail;
        unsigned available = tud_cdc_write_available();
        while (count < sizeof(bytes) && count < available && cursor != head) {
            bytes[count++] = (char)ring[cursor];
            cursor = (cursor + 1u) & (RX_CAPACITY - 1u);
        }
        if (count) {
            unsigned accepted = tud_cdc_write(bytes, count);
            __dmb();
            tail = (tail + accepted) & (RX_CAPACITY - 1u);
        }
        tud_cdc_write_flush();
        // Bound each direction's work so continuous USB data cannot starve RX.
        for (unsigned n = 0; n < sizeof(bytes) && uart_is_writable(uart0); ++n) {
            if (!tud_cdc_available()) break;
            int ch = tud_cdc_read_char();
            if (ch < 0) break;
            uart_putc_raw(uart0, (char)ch);
            tx_bytes++;
        }
        if (status_requested) {
            status_requested = false;
            printf("FNRBRIDGE rx=%lu tx=%lu drops=%lu overruns=%lu framing=%lu parity=%lu breaks=%lu\n",
                   (unsigned long)rx_bytes, (unsigned long)tx_bytes, (unsigned long)software_drops,
                   (unsigned long)overruns, (unsigned long)framing, (unsigned long)parity, (unsigned long)breaks);
        }
        tight_loop_contents();
    }
}
