#include "fnr/uart_buffer.h"

#include <limits.h>
#include <string.h>

static void add(uint32_t *value, uint32_t amount) {
    *value = UINT32_MAX - *value < amount ? UINT32_MAX : *value + amount;
}

static void discard(fnr_uart_rx_buffer *buffer, uint32_t reason) {
    add(&buffer->counters.dropped, (uint32_t)buffer->count);
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
    buffer->gap_flags |= reason;
}

void fnr_uart_rx_init(fnr_uart_rx_buffer *buffer) {
    if (buffer != NULL) memset(buffer, 0, sizeof(*buffer));
}

bool fnr_uart_rx_push(fnr_uart_rx_buffer *buffer, uint8_t byte) {
    if (buffer == NULL) return false;
    add(&buffer->counters.received, 1);
    if (buffer->gap_flags != 0) {
        add(&buffer->counters.dropped, 1);
        return false;
    }
    if (buffer->count == FNR_UART_RX_CAPACITY) {
        add(&buffer->counters.overflows, 1);
        discard(buffer, FNR_UART_GAP_OVERFLOW);
        add(&buffer->counters.dropped, 1);
        return false;
    }
    buffer->bytes[buffer->head] = byte;
    buffer->head = (buffer->head + 1u) % FNR_UART_RX_CAPACITY;
    ++buffer->count;
    return true;
}

void fnr_uart_rx_fault(fnr_uart_rx_buffer *buffer, uint32_t reason) {
    if (buffer == NULL) return;
    add(&buffer->counters.faults, 1);
    discard(buffer, reason == 0 ? FNR_UART_GAP_ERROR : reason);
}

size_t fnr_uart_rx_take(fnr_uart_rx_buffer *buffer, uint8_t *out,
                       size_t capacity, uint32_t *gap_flags) {
    if (buffer == NULL || out == NULL || capacity == 0 || gap_flags == NULL)
        return 0;
    *gap_flags = buffer->gap_flags;
    if (buffer->gap_flags != 0) {
        buffer->gap_flags = 0;
        return 0;
    }
    size_t count = buffer->count < capacity ? buffer->count : capacity;
    for (size_t i = 0; i < count; ++i) {
        out[i] = buffer->bytes[buffer->tail];
        buffer->tail = (buffer->tail + 1u) % FNR_UART_RX_CAPACITY;
    }
    buffer->count -= count;
    add(&buffer->counters.popped, (uint32_t)count);
    return count;
}
