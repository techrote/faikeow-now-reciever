#ifndef FNR_FAKE_PICO_H
#define FNR_FAKE_PICO_H

/* Minimal deterministic SDK boundary for the actual RP2040 transport adapter.
 * No device, clock, serial port or operating-system hardware API is accessed.
 * The FIFO can exceed physical capacity to model a hostile continuous source
 * and prove that the adapter's ISR/retirement loops remain bounded. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct uart_inst { unsigned index; } uart_inst_t;
typedef struct uart_hw { volatile uint32_t dr, rsr; } uart_hw_t;
typedef uint64_t absolute_time_t;

enum {
    UART_UARTRSR_FE_BITS = 1u,
    UART_UARTRSR_PE_BITS = 2u,
    UART_UARTRSR_BE_BITS = 4u,
    UART_UARTRSR_OE_BITS = 8u,
    UART_UARTRSR_BITS = 15u,
    UART_PARITY_NONE = 0,
    GPIO_FUNC_UART = 2,
    UART0_IRQ = 20,
    FNR_FAKE_FIFO_CAPACITY = 2048
};

static uart_inst_t fnr_fake_uart_instance;
static uart_hw_t fnr_fake_uart_registers;
#define uart0 (&fnr_fake_uart_instance)
static uint32_t fnr_fake_ms;
static uint32_t fnr_fake_irq_mask;
static void (*fnr_fake_restore_hook)(void);
static void (*fnr_fake_irq_handler)(void);
static bool fnr_fake_irq_enabled, fnr_fake_rx_irq, fnr_fake_tx_irq;
static bool fnr_fake_cts, fnr_fake_rts, fnr_fake_fifo_enabled;
static unsigned fnr_fake_baud, fnr_fake_data_bits, fnr_fake_stop_bits;
static unsigned fnr_fake_parity, fnr_fake_gpio_functions[2];
static uint32_t fnr_fake_fifo[FNR_FAKE_FIFO_CAPACITY];
static size_t fnr_fake_fifo_size, fnr_fake_fifo_offset, fnr_fake_fifo_reads;

static inline uart_hw_t *uart_get_hw(uart_inst_t *uart) {
    (void)uart;
    return &fnr_fake_uart_registers;
}

static inline bool uart_is_readable(uart_inst_t *uart) {
    (void)uart;
    if (fnr_fake_fifo_offset >= fnr_fake_fifo_size) return false;
    /* Each successful check in the adapter is followed by one DR read. Stage
     * that word here, since plain C register reads cannot pop a fake FIFO. */
    fnr_fake_uart_registers.dr = fnr_fake_fifo[fnr_fake_fifo_offset++];
    ++fnr_fake_fifo_reads;
    return true;
}

static inline uint32_t save_and_disable_interrupts(void) {
    const uint32_t previous = fnr_fake_irq_mask;
    fnr_fake_irq_mask = 1;
    return previous;
}

static inline void restore_interrupts(uint32_t previous) {
    fnr_fake_irq_mask = previous;
    if (previous == 0 && fnr_fake_restore_hook != NULL) {
        void (*hook)(void) = fnr_fake_restore_hook;
        fnr_fake_restore_hook = NULL;
        hook();
    }
}

static inline absolute_time_t get_absolute_time(void) {
    return (uint64_t)fnr_fake_ms * UINT64_C(1000);
}
static inline uint32_t to_ms_since_boot(absolute_time_t time) {
    return (uint32_t)(time / UINT64_C(1000));
}
static inline unsigned uart_init(uart_inst_t *uart, unsigned baud) {
    (void)uart;
    fnr_fake_baud = baud;
    return baud;
}
static inline void uart_set_hw_flow(uart_inst_t *uart, bool cts, bool rts) {
    (void)uart;
    fnr_fake_cts = cts;
    fnr_fake_rts = rts;
}
static inline void uart_set_format(uart_inst_t *uart, unsigned data_bits,
                                    unsigned stop_bits, unsigned parity) {
    (void)uart;
    fnr_fake_data_bits = data_bits;
    fnr_fake_stop_bits = stop_bits;
    fnr_fake_parity = parity;
}
static inline void uart_set_fifo_enabled(uart_inst_t *uart, bool enabled) {
    (void)uart;
    fnr_fake_fifo_enabled = enabled;
}
static inline void gpio_set_function(unsigned pin, unsigned function) {
    if (pin < 2u) fnr_fake_gpio_functions[pin] = function;
}
static inline void irq_set_exclusive_handler(unsigned irq, void (*handler)(void)) {
    (void)irq;
    fnr_fake_irq_handler = handler;
}
static inline void irq_set_enabled(unsigned irq, bool enabled) {
    (void)irq;
    fnr_fake_irq_enabled = enabled;
}
static inline void uart_set_irqs_enabled(uart_inst_t *uart, bool rx, bool tx) {
    (void)uart;
    fnr_fake_rx_irq = rx;
    fnr_fake_tx_irq = tx;
}

#endif
