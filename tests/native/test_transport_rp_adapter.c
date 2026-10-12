#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Compile the production adapter, not a parallel model. Only Pico SDK I/O and
 * the clock are substituted by fake_pico/. Static state is visible here so the
 * tests can inject at the IRQ/ring boundary without adding production hooks. */
#include "../../firmware/rp2040/transport_adapter.c"

static void start_at(uint32_t now) {
    fnr_fake_ms = now;
    fnr_fake_irq_mask = 0;
    fnr_fake_restore_hook = NULL;
    fnr_fake_irq_handler = NULL;
    fnr_fake_fifo_size = fnr_fake_fifo_offset = fnr_fake_fifo_reads = 0;
    fnr_fake_uart_registers.dr = fnr_fake_uart_registers.rsr = 0;
    fnr_rp_transport_init();
    assert(fnr_fake_baud == 115200u);
    assert(fnr_fake_data_bits == 8u && fnr_fake_stop_bits == 1u);
    assert(fnr_fake_parity == UART_PARITY_NONE);
    assert(!fnr_fake_cts && !fnr_fake_rts && fnr_fake_fifo_enabled);
    assert(fnr_fake_gpio_functions[0] == GPIO_FUNC_UART);
    assert(fnr_fake_gpio_functions[1] == GPIO_FUNC_UART);
    assert(fnr_fake_irq_enabled && fnr_fake_rx_irq && !fnr_fake_tx_irq);
    assert(fnr_fake_irq_handler != NULL);
    assert(fnr_transport_foreground_consume == NULL);
    assert(fnr_transport_take_events_foreground() == FNR_TRANSPORT_EVENT_RESET);
}

static void queue_bytes(const uint8_t *bytes, size_t length) {
    for (size_t i = 0; i < length; ++i)
        assert(fnr_uart_rx_push(&receive_bytes, bytes[i]));
}

static void queue_boundary(void) {
    const uint8_t zero = 0;
    queue_bytes(&zero, 1);
}

static void queue_datagram(uint32_t sequence, uint8_t marker) {
    fnr_datagram datagram = {{2, 3, 4, 5, 6, 7}, 1, {0}};
    datagram.payload[0] = marker;
    fnr_transport_frame frame;
    uint8_t wire[FNR_TRANSPORT_WIRE_CAPACITY];
    size_t size = 0;
    assert(fnr_transport_make_datagram(&frame, &datagram, sequence, 42) ==
           FNR_TRANSPORT_OK);
    assert(fnr_transport_encode(&frame, wire, sizeof(wire), &size) ==
           FNR_TRANSPORT_OK);
    queue_bytes(wire, size);
}

static void assert_delivery(uint32_t sequence, uint8_t marker) {
    fnr_transport_frame frame;
    fnr_datagram datagram;
    assert(fnr_transport_take_datagram_foreground(&frame));
    assert(frame.sequence == sequence && frame.generation == 42);
    assert(fnr_transport_unpack_datagram(&frame, &datagram) == FNR_TRANSPORT_OK);
    assert(datagram.size == 1 && datagram.payload[0] == marker);
    const uint8_t source[FNR_MAC_SIZE] = {2, 3, 4, 5, 6, 7};
    assert(memcmp(datagram.source_mac, source, sizeof(source)) == 0);
    assert(!fnr_transport_take_datagram_foreground(&frame));
}

static void establish_baseline(uint32_t now) {
    start_at(now);
    queue_boundary();
    queue_datagram(0, 11);
    fnr_rp_transport_task();
    assert(fnr_transport_take_events_foreground() == 0);
    assert_delivery(0, 11);
}

static void test_timeout_before_old_ring_input(void) {
    establish_baseline(0);
    queue_datagram(1, 22);
    fnr_fake_ms = 4000;
    fnr_rp_transport_task();
    fnr_transport_frame frame;
    assert(!fnr_transport_take_datagram_foreground(&frame));
    assert((fnr_transport_take_events_foreground() & FNR_TRANSPORT_EVENT_TIMEOUT) != 0);
    assert(!fnr_transport_take_datagram_foreground(&frame));
    assert(receiver.counters.link_timeouts == 1);
    assert(receive_bytes.count == 0);
    fnr_rp_transport_task();
    assert(receiver.counters.link_timeouts == 1); /* One event per observation. */

    ++fnr_fake_ms;
    queue_boundary();
    queue_datagram(2, 33);
    fnr_rp_transport_task();
    assert(fnr_transport_take_events_foreground() == 0);
    assert_delivery(2, 33);
}

static void test_second_stall_while_already_timed_out(void) {
    establish_baseline(0);
    fnr_fake_ms = 4000;
    fnr_rp_transport_task();
    assert((fnr_transport_take_events_foreground() & FNR_TRANSPORT_EVENT_TIMEOUT) != 0);
    queue_boundary();
    queue_datagram(1, 44);
    fnr_fake_ms = 8000;
    fnr_rp_transport_task();
    assert((fnr_transport_take_events_foreground() & FNR_TRANSPORT_EVENT_TIMEOUT) != 0);
    fnr_transport_frame frame;
    assert(!fnr_transport_take_datagram_foreground(&frame));
    assert(receiver.counters.link_timeouts == 2);
    ++fnr_fake_ms;
    queue_boundary();
    queue_datagram(2, 55);
    fnr_rp_transport_task();
    assert(fnr_transport_take_events_foreground() == 0);
    assert_delivery(2, 55);
}

static void test_getter_expires_unconsumed_record(void) {
    establish_baseline(0);
    queue_datagram(1, 66);
    fnr_rp_transport_task();
    fnr_fake_ms = 3000;
    fnr_transport_frame frame;
    assert(!fnr_transport_take_datagram_foreground(&frame));
    assert((fnr_transport_take_events_foreground() & FNR_TRANSPORT_EVENT_TIMEOUT) != 0);
    assert(!fnr_transport_take_datagram_foreground(&frame));
    assert(receiver.counters.datagrams_dropped == 1);
}

static void test_millisecond_wrap(void) {
    establish_baseline(UINT32_MAX - UINT32_C(1000));
    fnr_fake_ms = 500;
    fnr_rp_transport_task();
    assert(fnr_transport_take_events_foreground() == 0);
    assert(receiver.counters.link_timeouts == 0);
    fnr_fake_ms = 2200;
    fnr_rp_transport_task();
    assert((fnr_transport_take_events_foreground() & FNR_TRANSPORT_EVENT_TIMEOUT) != 0);
    assert(receiver.counters.link_timeouts == 1);
    queue_boundary();
    queue_datagram(1, 77);
    ++fnr_fake_ms;
    fnr_rp_transport_task();
    assert(fnr_transport_take_events_foreground() == 0);
    assert_delivery(1, 77);
}

static void inject_irq_error(void) {
    fnr_fake_uart_registers.rsr = UART_UARTRSR_FE_BITS;
    fnr_fake_irq_handler();
}

static void test_irq_gap_after_event_take(void) {
    establish_baseline(0);
    queue_datagram(1, 88);
    fnr_rp_transport_task();
    /* The IRQ fires as the event getter restores interrupts, after it read
     * zero events but before the next consumer retrieval. */
    fnr_fake_restore_hook = inject_irq_error;
    assert(fnr_transport_take_events_foreground() == 0);
    fnr_transport_frame frame;
    assert(!fnr_transport_take_datagram_foreground(&frame));
    assert((fnr_transport_take_events_foreground() & FNR_TRANSPORT_EVENT_LINK_ERROR) != 0);
    assert(!fnr_transport_take_datagram_foreground(&frame));
    assert(framing_errors == 1 && receiver.counters.link_discontinuities == 1);
    queue_boundary();
    queue_datagram(2, 99);
    fnr_rp_transport_task();
    assert(fnr_transport_take_events_foreground() == 0);
    assert_delivery(2, 99);
}

static void test_no_consumer_keeps_only_owned_newest_slot(void) {
    establish_baseline(0);
    queue_datagram(1, 12);
    queue_datagram(2, 13);
    fnr_rp_transport_task();
    assert(receiver.counters.datagrams_consumed == 1);
    fnr_transport_frame frame;
    assert(!fnr_transport_take_datagram_foreground(&frame));
    assert((fnr_transport_take_events_foreground() &
            FNR_TRANSPORT_EVENT_DATAGRAM_REPLACED) != 0);
    assert(receiver.counters.datagrams_replaced == 1);
    assert_delivery(2, 13);
}

static void test_bounded_foreground_and_irq_work(void) {
    start_at(0);
    for (size_t i = 0; i < FNR_UART_RX_CAPACITY; ++i)
        assert(fnr_uart_rx_push(&receive_bytes, 0x55));
    fnr_rp_transport_task();
    assert(receive_bytes.counters.popped == RX_TASK_BUDGET);
    assert(receive_bytes.count == FNR_UART_RX_CAPACITY - RX_TASK_BUDGET);

    start_at(0);
    fnr_fake_fifo_size = 96;
    for (size_t i = 0; i < fnr_fake_fifo_size; ++i) fnr_fake_fifo[i] = 0x55;
    fnr_fake_irq_handler();
    assert(fnr_fake_fifo_reads == UART_IRQ_QUANTUM);
    assert(receive_bytes.counters.received == UART_IRQ_QUANTUM);
    assert(receiver.counters.bytes_received == 0); /* ISR never parses. */
    fnr_transport_reset_foreground();
    assert(fnr_fake_fifo_reads == 2u * UART_IRQ_QUANTUM);
    assert(receive_bytes.count == 0);
    assert((fnr_transport_take_events_foreground() & FNR_TRANSPORT_EVENT_RESET) != 0);
}

int main(void) {
    test_timeout_before_old_ring_input();
    test_second_stall_while_already_timed_out();
    test_getter_expires_unconsumed_record();
    test_millisecond_wrap();
    test_irq_gap_after_event_take();
    test_no_consumer_keeps_only_owned_newest_slot();
    test_bounded_foreground_and_irq_work();
    puts("PASS: production RP transport adapter, fake-clock timeout/IRQ boundaries");
    return 0;
}
