/* Diagnostic/recovery only. No radio reset/download GPIO, no flash operations.
 * Candidate GP0 TX / GP1 RX comes from cited prior art plus passive boot-mode
 * observations. ROM bidirectional identification must qualify it on each board.
 */
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/bootrom.h"
#include "hardware/uart.h"
#include "tusb.h"

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
    stdio_init_all();
    stdio_set_translate_crlf(&stdio_usb, false);
    char bytes[64];
    while (true) {
        unsigned count = 0;
        while (count < sizeof(bytes) && uart_is_readable(uart0))
            bytes[count++] = (char)uart_getc(uart0);
        if (count) stdio_put_string(bytes, (int)count, false, false);
        // Bound each direction's work so continuous USB data cannot starve RX.
        for (unsigned n = 0; n < sizeof(bytes) && uart_is_writable(uart0); ++n) {
            int ch = getchar_timeout_us(0);
            if (ch == PICO_ERROR_TIMEOUT) break;
            uart_putc_raw(uart0, (char)ch);
        }
        tight_loop_contents();
    }
}
