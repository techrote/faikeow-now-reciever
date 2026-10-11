#ifndef FNR_UART_BUFFER_H
#define FNR_UART_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FNR_UART_RX_CAPACITY = 1024,
    FNR_UART_GAP_OVERFLOW = 1u,
    FNR_UART_GAP_ERROR = 2u,
    FNR_UART_GAP_RESET = 4u,
    FNR_UART_GAP_TIMEOUT = 8u
};

typedef struct fnr_uart_rx_counters {
    uint32_t received, popped, dropped, overflows, faults;
} fnr_uart_rx_counters;

/* Serially called storage, NOT a lock-free ring. The target must exclude its
 * UART ISR while foreground reads/mutates this object. No allocation, SDK
 * types, or borrowed payloads. Every loss flushes queued bytes and blocks new
 * publication until foreground observes the gap. The parser must then discard
 * the incomplete segment through the next delimiter. Counters saturate. */
typedef struct fnr_uart_rx_buffer {
    uint8_t bytes[FNR_UART_RX_CAPACITY];
    size_t head, tail, count;
    uint32_t gap_flags;
    fnr_uart_rx_counters counters;
} fnr_uart_rx_buffer;

void fnr_uart_rx_init(fnr_uart_rx_buffer *buffer);
bool fnr_uart_rx_push(fnr_uart_rx_buffer *buffer, uint8_t byte);
void fnr_uart_rx_fault(fnr_uart_rx_buffer *buffer, uint32_t reason);
/* First returns an outstanding gap with zero bytes; later calls may return
 * copied bytes. A null/zero-capacity output does not consume bytes or events. */
size_t fnr_uart_rx_take(fnr_uart_rx_buffer *buffer, uint8_t *out,
                       size_t capacity, uint32_t *gap_flags);

#ifdef __cplusplus
}
#endif
#endif
