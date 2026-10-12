#include <Arduino.h>
#include "radio_adapter.h"
#include "transport_adapter.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
        std::abort(); \
    } \
} while (0)

namespace {
using Bytes = std::vector<uint8_t>;
struct Write {
    int reported_free;
    Bytes requested;
    size_t accepted;
};
struct Fake {
    uint32_t now = 0;
    int reported_free = 128;
    size_t accept_limit = std::numeric_limits<size_t>::max();
    unsigned begin_calls = 0;
    unsigned debug_calls = 0;
    unsigned print_calls = 0;
    unsigned radio_take_calls = 0;
    unsigned long baud = 0;
    int config = 0;
    int mode = 0;
    bool debug_enabled = true;
    Bytes wire;
    std::vector<Write> writes;
} fake;
fnr_radio_ingress ingress{};
uint32_t radio_generation = 0;
const fnr_radio_config radio_config{6, {2,3,4,5,6,7}, true};
constexpr uint64_t boot_generation = UINT64_C(0x1234567812345678);

void start(uint32_t now = 0, uint32_t generation = 1) {
    fake = Fake{};
    fake.now = now;
    ingress = fnr_radio_ingress{};
    fnr_radio_reset(&ingress, &radio_config);
    fnr_radio_transition(&ingress, FNR_RADIO_READY);
    radio_generation = generation;
    fnr_esp_transport_init();
    CHECK(fake.begin_calls == 1 && fake.baud == 115200);
    CHECK(fake.config == SERIAL_8N1 && fake.mode == SERIAL_TX_ONLY);
    CHECK(fake.debug_calls >= 1 && !fake.debug_enabled);
    CHECK(fake.print_calls >= 1 && fake.writes.empty());
}

void task(int free_bytes = 128) {
    fake.reported_free = free_bytes;
    fnr_esp_transport_task();
}

void drain() {
    for (unsigned iteration = 0; iteration < 100; ++iteration) {
        const size_t before = fake.writes.size();
        task();
        if (fake.writes.size() == before) return;
    }
    CHECK(false);
}

void publish(uint8_t marker, size_t length = 250) {
    std::array<uint8_t, FNR_DATAGRAM_CAPACITY> bytes{};
    bytes.fill(marker);
    CHECK(length >= 1 && length <= bytes.size());
    const size_t writes_before = fake.writes.size();
    fnr_radio_receive(&ingress, radio_config.allowed_mac, bytes.data(), length);
    CHECK(fake.writes.size() == writes_before);
    /* Reuse callback storage immediately, just as the SDK may do. */
    bytes.fill(0);
}

struct Decoded {
    std::vector<fnr_transport_frame> frames;
    unsigned rejected = 0;
};

Decoded decode(const Bytes &wire) {
    Decoded result;
    size_t begin = 0;
    for (size_t end = 0; end < wire.size(); ++end) {
        if (wire[end] != 0) continue;
        if (end != begin) {
            fnr_transport_frame frame{};
            if (fnr_transport_decode(wire.data() + begin, end - begin, &frame) == FNR_TRANSPORT_OK) {
                result.frames.push_back(frame);
            } else {
                ++result.rejected;
            }
        }
        begin = end + 1;
    }
    CHECK(begin == wire.size());
    return result;
}

fnr_transport_radio_status unpack_status(const fnr_transport_frame &frame) {
    fnr_transport_radio_status status{};
    CHECK(fnr_transport_unpack_status(&frame, &status) == FNR_TRANSPORT_OK);
    return status;
}

fnr_datagram unpack_datagram(const fnr_transport_frame &frame) {
    fnr_datagram datagram{};
    CHECK(fnr_transport_unpack_datagram(&frame, &datagram) == FNR_TRANSPORT_OK);
    return datagram;
}

void test_fifo_boundaries() {
    start();
    for (const int free_bytes : {-1, 0, 1}) {
        task(free_bytes);
        CHECK(fake.writes.empty());
    }
    task(2);
    CHECK(fake.writes.size() == 1);
    CHECK(fake.writes.back().requested.size() == 1 && fake.wire == Bytes{0});
    task(33);
    CHECK(fake.writes.back().requested.size() == 32);
    task(128);
    CHECK(fake.writes.back().requested.size() == 32);
    task(3);
    CHECK(fake.writes.back().requested.size() == 2);
    drain();
    const auto decoded = decode(fake.wire);
    CHECK(decoded.rejected == 0 && decoded.frames.size() == 1);
    CHECK(decoded.frames.front().generation == boot_generation);
    CHECK(decoded.frames.front().sequence == 0);
    const auto status = unpack_status(decoded.frames.front());
    CHECK(status.state == FNR_RADIO_READY && status.channel == 6);
    CHECK(status.flags == FNR_TRANSPORT_STATUS_RESET);
    fnr_transport_tx_counters counters{};
    fnr_esp_transport_snapshot(&counters);
    CHECK(counters.frames_completed == 1 && counters.bytes_written == fake.wire.size());
    std::puts("PASS actual ESP adapter: reported free 1/2/33/128 preserves the UART FIFO spare byte");
}

void test_foreground_ownership_and_partial_writes() {
    start();
    publish(0x11);
    publish(0x22);
    CHECK(ingress.counters.dropped == 1 && ingress.counters.consumed == 0);
    task(1);
    CHECK(fake.writes.empty() && ingress.counters.consumed == 1);
    CHECK(fake.radio_take_calls == 1 && !ingress.pending);
    publish(0x33);
    task(1);
    CHECK(ingress.counters.consumed == 2 && !ingress.pending);
    fnr_transport_tx_counters counters{};
    fnr_esp_transport_snapshot(&counters);
    CHECK(counters.datagrams_replaced == 1 && counters.datagrams_offered == 2);
    drain();
    const auto decoded = decode(fake.wire);
    CHECK(decoded.rejected == 0 && decoded.frames.size() == 2);
    CHECK(decoded.frames[0].type == FNR_TRANSPORT_RADIO_STATUS);
    CHECK(decoded.frames[1].sequence == 1);
    const auto newest = unpack_datagram(decoded.frames[1]);
    CHECK(newest.size == 250 && newest.payload[0] == 0x33 && newest.payload[249] == 0x33);
    CHECK(std::memcmp(newest.source_mac, radio_config.allowed_mac, FNR_MAC_SIZE) == 0);
    CHECK(unpack_status(decoded.frames[0]).tx_replaced == 1);

    fake.wire.clear();
    fake.writes.clear();
    publish(0x55);
    fake.accept_limit = 0;
    task(33);
    CHECK(fake.writes.size() == 1 && fake.writes.back().accepted == 0);
    const Bytes first_offer = fake.writes.back().requested;
    CHECK(first_offer.size() == 32 && fake.wire.empty());
    fake.accept_limit = 3;
    task(33);
    CHECK(fake.writes.back().requested == first_offer && fake.writes.back().accepted == 3);
    fake.accept_limit = std::numeric_limits<size_t>::max();
    drain();
    const auto partial = decode(fake.wire);
    CHECK(partial.rejected == 0 && partial.frames.size() == 1);
    CHECK(unpack_datagram(partial.frames.front()).payload[0] == 0x55);
    CHECK(partial.frames.front().sequence == 2);
    std::puts("PASS actual ESP adapter: callback/foreground ownership, full FIFO drops and zero/short writes");
}

void test_lifecycle_retirement() {
    start(0, UINT32_MAX);
    drain();
    fake.wire.clear();
    fake.writes.clear();
    publish(0xa1);
    task(14);
    CHECK(fake.wire.size() == 13);
    CHECK(std::find(fake.wire.begin(), fake.wire.end(), uint8_t{0}) == fake.wire.end());
    publish(0xa2);
    task(1);
    CHECK(fake.wire.size() == 13);
    /* Restart detection must use lifecycle generation even once diagnostic
     * counters saturate. The uint32 lifecycle marker may itself wrap. */
    ingress.counters.restarts = UINT32_MAX;
    fnr_radio_reset(&ingress, &radio_config);
    fnr_radio_transition(&ingress, FNR_RADIO_READY);
    ++radio_generation;
    CHECK(radio_generation == 0 && ingress.counters.restarts == UINT32_MAX);
    publish(0xb1);
    task(2);
    CHECK(fake.wire.size() == 14 && fake.wire.back() == 0);
    drain();
    const auto decoded = decode(fake.wire);
    CHECK(decoded.rejected == 1 && decoded.frames.size() == 2);
    CHECK(decoded.frames[0].type == FNR_TRANSPORT_RADIO_STATUS);
    CHECK(decoded.frames[0].sequence == 0 && decoded.frames[0].generation == boot_generation + 1);
    const auto status = unpack_status(decoded.frames[0]);
    CHECK(status.flags == FNR_TRANSPORT_STATUS_RESET && status.tx_aborted == 2);
    CHECK(status.radio.restarts == UINT32_MAX);
    CHECK(decoded.frames[1].sequence == 1 && decoded.frames[1].generation == boot_generation + 1);
    const auto fresh = unpack_datagram(decoded.frames[1]);
    CHECK(fresh.payload[0] == 0xb1 && fresh.payload[249] == 0xb1);
    fnr_transport_tx_counters counters{};
    fnr_esp_transport_snapshot(&counters);
    CHECK(counters.restarts == 1 && counters.records_aborted == 2);
    std::puts("PASS actual ESP adapter: lifecycle wrap/saturated counters retire old suffix and pending data");
}

void test_periodic_status_and_clock_wrap() {
    const uint32_t initial = UINT32_MAX - 499u;
    start(initial);
    drain();
    fake.wire.clear();
    fake.writes.clear();
    fake.now = initial + 999u;
    task();
    CHECK(fake.wire.empty());
    fake.now = initial + 1000u;
    task();
    CHECK(!fake.wire.empty());
    drain();
    const auto first = decode(fake.wire);
    CHECK(first.rejected == 0 && first.frames.size() == 1);
    CHECK(first.frames[0].sequence == 1 && first.frames[0].generation == boot_generation);
    CHECK(unpack_status(first.frames[0]).flags == 0);
    fake.wire.clear();
    fake.now = initial + 1999u;
    task();
    CHECK(fake.wire.empty());
    fake.now = initial + 2000u;
    drain();
    const auto second = decode(fake.wire);
    CHECK(second.rejected == 0 && second.frames.size() == 1);
    CHECK(second.frames[0].sequence == 2 && second.frames[0].generation == boot_generation);
    CHECK(unpack_status(second.frames[0]).flags == 0);
    fnr_esp_transport_snapshot(nullptr);
    std::puts("PASS actual ESP adapter: 1000 ms status cadence across millis wrap without repeated RESET");
}
}  // namespace

FnrFakeSerial Serial;
FnrFakeESP ESP;

void FnrFakeSerial::begin(unsigned long baud, int config, int mode) {
    ++fake.begin_calls;
    fake.baud = baud;
    fake.config = config;
    fake.mode = mode;
}

void FnrFakeSerial::setDebugOutput(bool enabled) {
    ++fake.debug_calls;
    fake.debug_enabled = enabled;
}

int FnrFakeSerial::availableForWrite() { return fake.reported_free; }

size_t FnrFakeSerial::write(const uint8_t *bytes, size_t length) {
    CHECK(bytes != nullptr && length != 0);
    CHECK(fake.reported_free > 1);
    /* Pinned ESP core advertises 128 - occupancy but its byte writer waits
     * at occupancy >= 127. Model that exact nonblocking precondition. */
    const size_t usable = static_cast<size_t>(fake.reported_free - 1);
    CHECK(length <= usable && length <= 32);
    const size_t accepted = std::min(length, fake.accept_limit);
    fake.writes.push_back({fake.reported_free, Bytes(bytes, bytes + length), accepted});
    fake.wire.insert(fake.wire.end(), bytes, bytes + accepted);
    fake.reported_free -= static_cast<int>(accepted);
    CHECK(fake.reported_free >= 1);
    return accepted;
}

uint32_t FnrFakeESP::random() { return 0x12345678; }
uint32_t millis() { return fake.now; }

extern "C" void system_set_os_print(uint8_t enabled) {
    CHECK(enabled == 0);
    ++fake.print_calls;
}

extern "C" bool fnr_radio_take_foreground(fnr_datagram *out) {
    ++fake.radio_take_calls;
    return fnr_radio_take(&ingress, out);
}

extern "C" void fnr_radio_status_foreground(fnr_radio_ingress *out) {
    fnr_radio_snapshot(&ingress, out);
}

extern "C" uint32_t fnr_radio_generation_foreground(void) { return radio_generation; }

int main() {
    test_fifo_boundaries();
    test_foreground_ownership_and_partial_writes();
    test_lifecycle_retirement();
    test_periodic_status_and_clock_wrap();
}
