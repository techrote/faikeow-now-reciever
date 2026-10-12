#include "transport_adapter.h"

#include <limits.h>
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"

enum { UART_IRQ_QUANTUM = 32, RX_CHUNK = 64, RX_TASK_BUDGET = 512,
       LINK_SILENCE_MS = 3000 };
static fnr_uart_rx_buffer receive_bytes;
static fnr_transport_rx receiver;
static uint32_t framing_errors, parity_errors, breaks, overruns;
static uint32_t last_valid_ms;
static uint32_t last_service_ms;
static bool timed_out;

static void bump(uint32_t *value) { if (*value != UINT32_MAX) ++*value; }

static void uart0_rx_irq(void) {
    /* Bounded ISR owns UART DR reads. No parsing, USB calls or allocation.
     * RX and receive-timeout IRQs service even a trailing single byte. */
    uint32_t errors = uart_get_hw(uart0)->rsr & UART_UARTRSR_BITS;
    for (size_t i = 0; i < UART_IRQ_QUANTUM && uart_is_readable(uart0); ++i) {
        const uint32_t word = uart_get_hw(uart0)->dr;
        errors |= (word >> 8u) & UART_UARTRSR_BITS;
        (void)fnr_uart_rx_push(&receive_bytes, (uint8_t)(word & 0xffu));
    }
    if (errors != 0) {
        if ((errors & UART_UARTRSR_FE_BITS) != 0) bump(&framing_errors);
        if ((errors & UART_UARTRSR_PE_BITS) != 0) bump(&parity_errors);
        if ((errors & UART_UARTRSR_BE_BITS) != 0) bump(&breaks);
        if ((errors & UART_UARTRSR_OE_BITS) != 0) bump(&overruns);
        fnr_uart_rx_fault(&receive_bytes, FNR_UART_GAP_ERROR);
        uart_get_hw(uart0)->rsr = 0;
    }
}

/* Caller masks interrupts. Observe any ISR gap before publishing/taking a
 * parser result, including a gap arriving while a foreground chunk was being
 * parsed. Clearing all queued bytes avoids joining either side of a loss. */
static void synchronize_gap(void) {
    if (receive_bytes.gap_flags == 0) return;
    uint8_t unused;
    uint32_t flags = 0;
    (void)fnr_uart_rx_take(&receive_bytes, &unused, 1, &flags);
    if ((flags & FNR_UART_GAP_RESET) != 0) fnr_transport_rx_reset(&receiver);
    else if ((flags & FNR_UART_GAP_TIMEOUT) != 0) fnr_transport_rx_timeout(&receiver);
    else fnr_transport_rx_discontinuity(&receiver);
}

/* Interrupts are masked. Retire old software AND hardware bytes. A timeout
 * must be handled before parsing buffered input; parsing old bytes at 'now'
 * would otherwise refresh the watchdog and conceal a long service stall. */
static void retire_uart(uint32_t reason) {
    fnr_uart_rx_fault(&receive_bytes, reason);
    for (size_t i = 0; i < UART_IRQ_QUANTUM && uart_is_readable(uart0); ++i)
        (void)uart_get_hw(uart0)->dr;
    uart_get_hw(uart0)->rsr = 0;
    synchronize_gap();
}

static void synchronize_link(void) {
    synchronize_gap();
    const uint32_t now = to_ms_since_boot(get_absolute_time());
    if (!timed_out && (uint32_t)(now - last_valid_ms) >= LINK_SILENCE_MS) {
        retire_uart(FNR_UART_GAP_TIMEOUT);
        timed_out = true;
    }
}

void fnr_rp_transport_init(void) {
    fnr_uart_rx_init(&receive_bytes);
    fnr_transport_rx_init(&receiver);
    framing_errors = parity_errors = breaks = overruns = 0;
    last_valid_ms = to_ms_since_boot(get_absolute_time());
    last_service_ms = last_valid_ms;
    timed_out = false;
    /* Accepted FNR-002 contract. No reset/download GPIO or flow-control pins. */
    uart_init(uart0, 115200);
    uart_set_hw_flow(uart0, false, false);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, true);
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);
    uart_get_hw(uart0)->rsr = 0;
    irq_set_exclusive_handler(UART0_IRQ, uart0_rx_irq);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irqs_enabled(uart0, true, false);
}

uint32_t fnr_transport_take_events_foreground(void) {
    const uint32_t saved = save_and_disable_interrupts();
    synchronize_link();
    const uint32_t events = fnr_transport_rx_take_events(&receiver);
    restore_interrupts(saved);
    return events;
}

bool fnr_transport_take_datagram_foreground(fnr_transport_frame *out) {
    const uint32_t saved = save_and_disable_interrupts();
    synchronize_link();
    const bool result = fnr_transport_rx_take_datagram(&receiver, out);
    restore_interrupts(saved);
    return result;
}

bool fnr_transport_take_status_foreground(fnr_transport_frame *out) {
    const uint32_t saved = save_and_disable_interrupts();
    synchronize_link();
    const bool result = fnr_transport_rx_take_status(&receiver, out);
    restore_interrupts(saved);
    return result;
}

void fnr_transport_snapshot_foreground(fnr_rp_transport_diagnostics *out) {
    if (out == NULL) return;
    const uint32_t saved = save_and_disable_interrupts();
    synchronize_link();
    out->parser = receiver.counters;
    out->uart = receive_bytes.counters;
    out->framing_errors = framing_errors;
    out->parity_errors = parity_errors;
    out->breaks = breaks;
    out->overruns = overruns;
    restore_interrupts(saved);
}

void fnr_transport_reset_foreground(void) {
    const uint32_t saved = save_and_disable_interrupts();
    retire_uart(FNR_UART_GAP_RESET);
    last_valid_ms = to_ms_since_boot(get_absolute_time());
    last_service_ms = last_valid_ms;
    timed_out = false;
    restore_interrupts(saved);
}

extern void fnr_transport_foreground_consume(const fnr_transport_consumer_api *api)
    __attribute__((weak));
static const fnr_transport_consumer_api consumer_api = {
    fnr_transport_take_events_foreground,
    fnr_transport_take_datagram_foreground,
    fnr_transport_take_status_foreground,
    fnr_transport_snapshot_foreground,
    fnr_transport_reset_foreground
};

void fnr_rp_transport_task(void) {
    const uint32_t entry_saved = save_and_disable_interrupts();
    const uint32_t entry_ms = to_ms_since_boot(get_absolute_time());
    if ((uint32_t)(entry_ms - last_service_ms) >= LINK_SILENCE_MS) {
        retire_uart(FNR_UART_GAP_TIMEOUT);
        timed_out = true;
    }
    last_service_ms = entry_ms;
    synchronize_link();
    restore_interrupts(entry_saved);
    uint8_t chunk[RX_CHUNK];
    for (size_t serviced = 0; serviced < RX_TASK_BUDGET; serviced += RX_CHUNK) {
        uint32_t flags = 0;
        const uint32_t saved = save_and_disable_interrupts();
        const size_t count = fnr_uart_rx_take(&receive_bytes, chunk, sizeof(chunk), &flags);
        restore_interrupts(saved);
        if (flags != 0) {
            if ((flags & FNR_UART_GAP_RESET) != 0) fnr_transport_rx_reset(&receiver);
            else if ((flags & FNR_UART_GAP_TIMEOUT) != 0) fnr_transport_rx_timeout(&receiver);
            else fnr_transport_rx_discontinuity(&receiver);
            continue;
        }
        if (count == 0) break;
        if (fnr_transport_rx_feed(&receiver, chunk, count) != 0) {
            last_valid_ms = to_ms_since_boot(get_absolute_time());
            timed_out = false;
        }
    }
    const uint32_t saved = save_and_disable_interrupts();
    synchronize_link();
    restore_interrupts(saved);
    /* The hook keeps the explicit generic consumer API linked without
     * implementing FNR-006. There is no default consumer or direct HID path. */
    if (fnr_transport_foreground_consume != NULL)
        fnr_transport_foreground_consume(&consumer_api);
}
