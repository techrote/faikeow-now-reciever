#include "fnr/transport_tx.h"
#include "fnr/uart_buffer.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); std::abort(); } } while (0)

static fnr_transport_radio_status ready() {
    fnr_transport_radio_status status{};
    status.state = FNR_RADIO_READY;
    status.channel = 6;
    return status;
}

static fnr_datagram packet(uint8_t marker, size_t size = FNR_DATAGRAM_CAPACITY) {
    fnr_datagram datagram{};
    const uint8_t mac[FNR_MAC_SIZE] = {2, 3, 4, 5, 6, 7};
    std::array<uint8_t, FNR_DATAGRAM_CAPACITY> bytes{};
    bytes.fill(marker);
    CHECK(fnr_datagram_copy(&datagram, mac, bytes.data(), size) == FNR_COPY_OK);
    return datagram;
}

static std::vector<uint8_t> pump(fnr_transport_tx &tx, size_t quantum = 32) {
    std::vector<uint8_t> wire;
    for (size_t i = 0; i < 10000; ++i) {
        const uint8_t *bytes = nullptr;
        const size_t count = fnr_transport_tx_peek(&tx, &bytes, quantum);
        if (count == 0) return wire;
        CHECK(bytes != nullptr && count <= quantum);
        // A sink may accept fewer than the offered bytes, including zero.
        CHECK(fnr_transport_tx_advance(&tx, 0));
        const size_t written = 1u + i % count;
        wire.insert(wire.end(), bytes, bytes + written);
        CHECK(fnr_transport_tx_advance(&tx, written));
    }
    CHECK(false); // Bounded forward progress must have emptied finite storage.
    return wire;
}

static void feed(fnr_transport_rx &rx, const std::vector<uint8_t> &bytes) {
    for (size_t i = 0; i < bytes.size();) {
        const size_t count = std::min<size_t>(1u + i % 61u, bytes.size() - i);
        (void)fnr_transport_rx_feed(&rx, bytes.data() + i, count);
        i += count;
    }
}

static fnr_datagram take(fnr_transport_rx &rx) {
    fnr_transport_frame frame{};
    CHECK(fnr_transport_rx_take_datagram(&rx, &frame));
    fnr_datagram out{};
    CHECK(fnr_transport_unpack_datagram(&frame, &out) == FNR_TRANSPORT_OK);
    return out;
}

static void test_backpressure_and_barrier() {
    fnr_transport_tx tx{};
    const auto status = ready();
    CHECK(fnr_transport_tx_init(&tx, 123, &status));
    for (unsigned i = 0; i < 1000; ++i) {
        auto datagram = packet(static_cast<uint8_t>(i));
        CHECK(fnr_transport_tx_offer_datagram(&tx, &datagram));
        std::memset(&datagram, 0, sizeof(datagram));
    }
    CHECK(tx.counters.datagrams_replaced == 999);
    CHECK(tx.counters.frames_started == 0 && tx.datagram_pending);
    CHECK(fnr_transport_tx_offer_status(&tx, &status));
    CHECK((tx.status.flags & FNR_TRANSPORT_STATUS_RESET) != 0);
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(1);
    CHECK(fnr_transport_tx_peek(&tx, &bytes, 0) == 0 && bytes == nullptr);
    CHECK(!fnr_transport_tx_advance(&tx, 1));
    const auto wire = pump(tx, 17);
    CHECK(wire.front() == 0);
    CHECK(tx.counters.frames_completed == 2);
    CHECK(tx.counters.bytes_written == wire.size());
    fnr_transport_rx rx{};
    fnr_transport_rx_init(&rx);
    feed(rx, wire);
    fnr_transport_frame frame{};
    CHECK(!fnr_transport_rx_take_datagram(&rx, &frame));
    CHECK((fnr_transport_rx_take_events(&rx) & FNR_TRANSPORT_EVENT_RADIO_RESET) != 0);
    CHECK(fnr_transport_rx_take_status(&rx, &frame));
    fnr_transport_radio_status received{};
    CHECK(fnr_transport_unpack_status(&frame, &received) == FNR_TRANSPORT_OK);
    CHECK(received.tx_replaced == 999);
    CHECK(frame.sequence == 0 && frame.generation == 123);
    const auto datagram = take(rx);
    CHECK(datagram.size == 250 && datagram.payload[0] == static_cast<uint8_t>(999));
    CHECK(datagram.source_mac[0] == 2);
    CHECK(rx.counters.frames_valid == 2 && rx.counters.bad_crc == 0);
}

static void test_active_frame_is_immutable() {
    fnr_transport_tx tx{};
    const auto status = ready();
    CHECK(fnr_transport_tx_init(&tx, 22, &status));
    fnr_transport_rx rx{};
    fnr_transport_rx_init(&rx);
    feed(rx, pump(tx));
    (void)fnr_transport_rx_take_events(&rx);
    const auto first = packet(0x55);
    auto next = packet(0x66);
    CHECK(fnr_transport_tx_offer_datagram(&tx, &first));
    const uint8_t *bytes = nullptr;
    CHECK(fnr_transport_tx_peek(&tx, &bytes, 11) == 11);
    const std::vector<uint8_t> prefix(bytes, bytes + 11);
    CHECK(fnr_transport_tx_advance(&tx, 11));
    const auto active_before = tx;
    CHECK(fnr_transport_tx_offer_datagram(&tx, &next));
    next = packet(0x77);
    CHECK(fnr_transport_tx_offer_datagram(&tx, &next));
    CHECK(tx.counters.datagrams_replaced == 1);
    CHECK(std::memcmp(tx.wire, active_before.wire, sizeof(tx.wire)) == 0);
    feed(rx, prefix);
    feed(rx, pump(tx, 1));
    CHECK(rx.counters.datagrams_received == 2);
    CHECK((fnr_transport_rx_take_events(&rx) & FNR_TRANSPORT_EVENT_DATAGRAM_REPLACED) != 0);
    CHECK(take(rx).payload[0] == 0x77);
    CHECK(rx.counters.bad_crc == 0 && rx.counters.bad_cobs == 0);
    CHECK(tx.counters.frames_started == tx.counters.frames_completed);
}

static void test_restart_aborts_old_suffix_and_pending() {
    fnr_transport_tx tx{};
    auto status = ready();
    CHECK(fnr_transport_tx_init(&tx, 40, &status));
    fnr_transport_rx rx{};
    fnr_transport_rx_init(&rx);
    feed(rx, pump(tx));
    (void)fnr_transport_rx_take_events(&rx);
    auto old = packet(0xa1);
    CHECK(fnr_transport_tx_offer_datagram(&tx, &old));
    const uint8_t *bytes = nullptr;
    CHECK(fnr_transport_tx_peek(&tx, &bytes, 13) == 13);
    CHECK(fnr_transport_rx_feed(&rx, bytes, 13) == 0);
    CHECK(fnr_transport_tx_advance(&tx, 13));
    old = packet(0xa2);
    CHECK(fnr_transport_tx_offer_datagram(&tx, &old));
    CHECK(fnr_transport_tx_offer_status(&tx, &status));
    CHECK(fnr_transport_tx_restart(&tx, 41, &status));
    CHECK(tx.counters.records_aborted == 3);
    CHECK(tx.counters.restarts == 1 && !tx.datagram_pending);
    auto fresh = packet(0xb1);
    CHECK(fnr_transport_tx_offer_datagram(&tx, &fresh));
    const auto wire = pump(tx, 7);
    CHECK(wire.front() == 0);
    feed(rx, wire);
    const uint32_t events = fnr_transport_rx_take_events(&rx);
    CHECK((events & FNR_TRANSPORT_EVENT_LINK_ERROR) != 0);
    CHECK((events & FNR_TRANSPORT_EVENT_GENERATION_CHANGE) != 0);
    CHECK((events & FNR_TRANSPORT_EVENT_RADIO_RESET) != 0);
    CHECK(rx.counters.datagrams_received == 1);
    CHECK(take(rx).payload[0] == 0xb1);
    fnr_transport_frame frame{};
    CHECK(fnr_transport_rx_take_status(&rx, &frame));
    CHECK(frame.sequence == 0 && frame.generation == 41);
    fnr_transport_radio_status received{};
    CHECK(fnr_transport_unpack_status(&frame, &received) == FNR_TRANSPORT_OK);
    CHECK(received.tx_aborted == 3);
    CHECK(!fnr_transport_rx_take_datagram(&rx, &frame));
    status.state = FNR_RADIO_UNCONFIGURED;
    status.channel = 0;
    CHECK(fnr_transport_tx_restart(&tx, 42, &status));
    CHECK(!fnr_transport_tx_offer_datagram(&tx, &fresh));
    CHECK(tx.counters.invalid_offers == 1);
}

static void test_tx_wrap_and_invalid_input() {
    fnr_transport_tx tx{};
    auto status = ready();
    CHECK(!fnr_transport_tx_init(nullptr, 0, &status));
    CHECK(!fnr_transport_tx_init(&tx, 0, nullptr));
    CHECK(fnr_transport_tx_init(&tx, UINT64_MAX, &status));
    (void)pump(tx);
    tx.next_sequence = UINT32_MAX;
    auto datagram = packet(1, 1);
    CHECK(fnr_transport_tx_offer_datagram(&tx, &datagram));
    auto wire = pump(tx);
    fnr_transport_frame frame{};
    CHECK(fnr_transport_decode(wire.data(), wire.size() - 1, &frame) == FNR_TRANSPORT_OK);
    CHECK(frame.sequence == UINT32_MAX && frame.generation == UINT64_MAX);
    CHECK(fnr_transport_tx_offer_datagram(&tx, &datagram));
    wire = pump(tx);
    CHECK(fnr_transport_decode(wire.data(), wire.size() - 1, &frame) == FNR_TRANSPORT_OK);
    CHECK(frame.sequence == 0);
    datagram.size = 251;
    CHECK(!fnr_transport_tx_offer_datagram(&tx, &datagram));
    const auto before = tx;
    status.channel = 255;
    CHECK(!fnr_transport_tx_restart(&tx, 2, &status));
    CHECK(std::memcmp(&tx, &before, sizeof(tx)) == 0);
    tx.counters.invalid_offers = UINT32_MAX;
    CHECK(!fnr_transport_tx_offer_status(&tx, &status));
    CHECK(tx.counters.invalid_offers == UINT32_MAX);
    status = ready();
    CHECK(fnr_transport_tx_offer_status(&tx, &status));
    const auto before_reset_offer = tx;
    status.flags = FNR_TRANSPORT_STATUS_RESET;
    CHECK(!fnr_transport_tx_offer_status(&tx, &status));
    CHECK(tx.datagram_pending == before_reset_offer.datagram_pending);
    status.flags = 0;
    CHECK(fnr_transport_tx_restart(&tx, 99, &tx.status));
    CHECK(tx.status.state == FNR_RADIO_READY && tx.status.channel == 6);
    CHECK((tx.status.flags & FNR_TRANSPORT_STATUS_RESET) != 0);
    tx.counters.restarts = UINT32_MAX;
    tx.counters.records_aborted = UINT32_MAX;
    CHECK(fnr_transport_tx_restart(&tx, 0, &status));
    CHECK(tx.counters.restarts == UINT32_MAX && tx.counters.records_aborted == UINT32_MAX);
}

static void test_ring_capacity_gap_and_wrap() {
    fnr_uart_rx_buffer ring{};
    fnr_uart_rx_init(&ring);
    for (size_t i = 0; i < FNR_UART_RX_CAPACITY; ++i)
        CHECK(fnr_uart_rx_push(&ring, static_cast<uint8_t>(i)));
    CHECK(!fnr_uart_rx_push(&ring, 9));
    CHECK(ring.count == 0 && ring.counters.dropped == FNR_UART_RX_CAPACITY + 1u);
    CHECK(ring.counters.overflows == 1);
    CHECK(!fnr_uart_rx_push(&ring, 0)); // Cannot swallow loss by a delimiter in IRQ.
    uint32_t flags = 99;
    std::array<uint8_t, 67> bytes{};
    CHECK(fnr_uart_rx_take(&ring, nullptr, bytes.size(), &flags) == 0);
    CHECK(fnr_uart_rx_take(&ring, bytes.data(), 0, &flags) == 0);
    CHECK(ring.gap_flags == FNR_UART_GAP_OVERFLOW);
    CHECK(fnr_uart_rx_take(&ring, bytes.data(), bytes.size(), &flags) == 0);
    CHECK(flags == FNR_UART_GAP_OVERFLOW);
    for (size_t run = 0; run < 100; ++run) {
        for (size_t i = 0; i < bytes.size(); ++i)
            CHECK(fnr_uart_rx_push(&ring, static_cast<uint8_t>(i + run)));
        CHECK(fnr_uart_rx_take(&ring, bytes.data(), bytes.size(), &flags) == bytes.size());
        CHECK(flags == 0);
        for (size_t i = 0; i < bytes.size(); ++i)
            CHECK(bytes[i] == static_cast<uint8_t>(i + run));
    }
    CHECK(fnr_uart_rx_push(&ring, 7));
    fnr_uart_rx_fault(&ring, FNR_UART_GAP_ERROR);
    fnr_uart_rx_fault(&ring, FNR_UART_GAP_RESET);
    CHECK(ring.count == 0);
    CHECK(fnr_uart_rx_take(&ring, bytes.data(), bytes.size(), &flags) == 0);
    CHECK(flags == (FNR_UART_GAP_ERROR | FNR_UART_GAP_RESET));
    ring.counters.received = ring.counters.dropped = ring.counters.faults = UINT32_MAX;
    fnr_uart_rx_fault(&ring, FNR_UART_GAP_ERROR);
    CHECK(!fnr_uart_rx_push(&ring, 8));
    CHECK(ring.counters.received == UINT32_MAX && ring.counters.dropped == UINT32_MAX);
    CHECK(ring.counters.faults == UINT32_MAX);
}

static void test_ingress_to_transport_accounting() {
    fnr_radio_ingress ingress{};
    const fnr_radio_config config{6, {2, 3, 4, 5, 6, 7}, true};
    fnr_radio_reset(&ingress, &config);
    fnr_radio_transition(&ingress, FNR_RADIO_READY);
    fnr_transport_tx tx{};
    auto status = ready();
    CHECK(fnr_transport_tx_init(&tx, 77, &status));
    fnr_transport_rx rx{};
    fnr_transport_rx_init(&rx);
    uint32_t seed = UINT32_C(0x73905a01);
    for (size_t i = 0; i < 30000; ++i) {
        seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
        const auto incoming = packet(static_cast<uint8_t>(seed));
        fnr_radio_receive(&ingress, incoming.source_mac, incoming.payload, incoming.size);
        if (i % 3u == 0) {
            fnr_datagram datagram{};
            CHECK(fnr_radio_take(&ingress, &datagram));
            CHECK(fnr_transport_tx_offer_datagram(&tx, &datagram));
        }
        if (i % 5u == 0) {
            const uint8_t *bytes = nullptr;
            const size_t count = fnr_transport_tx_peek(&tx, &bytes, 1u + seed % 32u);
            if (count != 0) {
                (void)fnr_transport_rx_feed(&rx, bytes, count);
                CHECK(fnr_transport_tx_advance(&tx, count));
            }
        }
    }
    fnr_datagram last{};
    if (fnr_radio_take(&ingress, &last)) CHECK(fnr_transport_tx_offer_datagram(&tx, &last));
    feed(rx, pump(tx));
    CHECK(ingress.counters.accepted == ingress.counters.consumed + ingress.counters.dropped);
    CHECK(tx.counters.datagrams_offered == ingress.counters.consumed);
    CHECK(tx.counters.datagrams_offered == tx.counters.datagrams_replaced + rx.counters.datagrams_received);
    CHECK(tx.counters.frames_completed == rx.counters.frames_valid);
    CHECK(tx.counters.frames_started == tx.counters.frames_completed);
    CHECK(rx.counters.datagrams_dropped + 1u == rx.counters.datagrams_received);
    CHECK(rx.counters.bad_crc == 0 && rx.counters.bad_cobs == 0);
    CHECK(rx.counters.sequence_discontinuities == 0);
    (void)fnr_transport_rx_take_events(&rx);
    CHECK(take(rx).payload[0] == last.payload[0]);
}

int main() {
    test_backpressure_and_barrier();
    test_active_frame_is_immutable();
    test_restart_aborts_old_suffix_and_pending();
    test_tx_wrap_and_invalid_input();
    test_ring_capacity_gap_and_wrap();
    test_ingress_to_transport_accounting();
    std::puts("PASS: bounded TX/UART ownership, partial writes, restart barriers and pipeline accounting");
}
