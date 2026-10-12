#include "fnr/transport.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
        std::abort(); \
    } \
} while (0)

namespace {
using Bytes = std::vector<uint8_t>;

/* Independent fixtures: CRC cross-checked with Python binascii.crc_hqx(data,
 * 0xffff); header/COBS bytes are literal, not emitted by the codec under test.
 * Datagram: MAC 02:03:04:05:06:07, application a5, local sequence 89abcdef,
 * radio generation 0102030405060708. */
const Bytes golden_raw = {
    0x01,0x01,0x07,0x00,0xef,0xcd,0xab,0x89,
    0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,
    0x02,0x03,0x04,0x05,0x06,0x07,0xa5,0x3b,0x9a
};
const Bytes golden_wire = {
    0x04,0x01,0x01,0x07,0x16,0xef,0xcd,0xab,0x89,
    0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,
    0x02,0x03,0x04,0x05,0x06,0x07,0xa5,0x3b,0x9a,0x00
};

uint16_t reference_crc(const uint8_t *data, size_t length) {
    uint32_t remainder = 0xffff;
    for (size_t n = 0; n < length; ++n) {
        remainder ^= static_cast<uint32_t>(data[n]) << 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            const bool top_bit = (remainder & 0x8000u) != 0;
            remainder = (remainder << 1) & 0xffffu;
            if (top_bit) remainder ^= 0x1021u;
        }
    }
    return static_cast<uint16_t>(remainder);
}

void append_le(Bytes &bytes, uint64_t value, unsigned width) {
    for (unsigned n = 0; n < width; ++n) {
        bytes.push_back(static_cast<uint8_t>(value >> (8u * n)));
    }
}

void replace_crc(Bytes &raw) {
    CHECK(raw.size() >= 2);
    const uint16_t crc = reference_crc(raw.data(), raw.size() - 2);
    raw[raw.size() - 2] = static_cast<uint8_t>(crc);
    raw[raw.size() - 1] = static_cast<uint8_t>(crc >> 8);
}

Bytes reference_raw(uint8_t version, uint8_t type, uint16_t declared_length,
                    uint32_t sequence, uint64_t generation, const Bytes &payload) {
    Bytes raw{version, type};
    append_le(raw, declared_length, 2);
    append_le(raw, sequence, 4);
    append_le(raw, generation, 8);
    raw.insert(raw.end(), payload.begin(), payload.end());
    raw.push_back(0);
    raw.push_back(0);
    replace_crc(raw);
    return raw;
}

/* Test-only run/split encoder independent of the production index-based
 * implementation. It emits the conventional final empty block after a full
 * 254-byte run. Both valid representations of that boundary are tested. */
Bytes reference_wire(const Bytes &raw) {
    Bytes wire;
    size_t begin = 0;
    for (;;) {
        size_t end = begin;
        while (end < raw.size() && raw[end] != 0) ++end;
        size_t remaining = end - begin;
        while (remaining >= 254) {
            wire.push_back(0xff);
            for (size_t n = 0; n < 254; ++n) wire.push_back(raw[begin + n]);
            begin += 254;
            remaining -= 254;
        }
        wire.push_back(static_cast<uint8_t>(remaining + 1));
        for (size_t n = 0; n < remaining; ++n) wire.push_back(raw[begin + n]);
        if (end == raw.size()) break;
        begin = end + 1;
    }
    wire.push_back(0);
    return wire;
}

struct Random {
    uint32_t state = 0xd6e8feb8u;
    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    uint8_t byte() { return static_cast<uint8_t>(next()); }
};

fnr_transport_frame datagram_frame(size_t length = 1, uint32_t sequence = 0,
                                    uint64_t generation = 1, uint8_t fill = 0xa5) {
    CHECK(length >= 1 && length <= FNR_DATAGRAM_CAPACITY);
    fnr_datagram input{};
    for (size_t n = 0; n < FNR_MAC_SIZE; ++n) {
        input.source_mac[n] = static_cast<uint8_t>(n + 2);
    }
    input.size = static_cast<uint16_t>(length);
    std::memset(input.payload, fill, length);
    fnr_transport_frame result{};
    CHECK(fnr_transport_make_datagram(&result, &input, sequence, generation) ==
          FNR_TRANSPORT_OK);
    return result;
}

fnr_transport_radio_status status_input(uint8_t flags = 0,
                                        fnr_radio_state state = FNR_RADIO_READY) {
    fnr_transport_radio_status status{};
    status.flags = flags;
    status.state = state;
    status.channel = 6;
    status.radio.received = 0x01020304;
    status.radio.accepted = 0x11121314;
    status.radio.rejected_source = 0x21222324;
    status.radio.invalid_length = 0x31323334;
    status.radio.dropped = 0x41424344;
    status.radio.consumed = 0x51525354;
    status.radio.starts = 0x61626364;
    status.radio.restarts = 0x71727374;
    status.radio.errors = 0x81828384;
    status.tx_replaced = 0x91929394;
    status.tx_aborted = 0xa1a2a3a4;
    return status;
}

fnr_transport_frame status_frame(uint32_t sequence = 0, uint64_t generation = 1,
                                uint8_t flags = 0,
                                fnr_radio_state state = FNR_RADIO_READY) {
    const fnr_transport_radio_status input = status_input(flags, state);
    fnr_transport_frame result{};
    CHECK(fnr_transport_make_status(&result, &input, sequence, generation) ==
          FNR_TRANSPORT_OK);
    return result;
}

Bytes encode_frame(const fnr_transport_frame &frame) {
    std::array<uint8_t, FNR_TRANSPORT_WIRE_CAPACITY + 16> output{};
    output.fill(0x5a);
    size_t written = std::numeric_limits<size_t>::max();
    CHECK(fnr_transport_encode(&frame, output.data(), FNR_TRANSPORT_WIRE_CAPACITY,
                              &written) == FNR_TRANSPORT_OK);
    CHECK(written >= 1 && written <= FNR_TRANSPORT_WIRE_CAPACITY);
    CHECK(output[written - 1] == 0);
    for (size_t n = 0; n + 1 < written; ++n) CHECK(output[n] != 0);
    for (size_t n = FNR_TRANSPORT_WIRE_CAPACITY; n < output.size(); ++n) {
        CHECK(output[n] == 0x5a);
    }
    return Bytes(output.begin(), output.begin() + static_cast<ptrdiff_t>(written));
}

void equal_frames(const fnr_transport_frame &actual,
                  const fnr_transport_frame &expected) {
    CHECK(actual.version == expected.version);
    CHECK(actual.type == expected.type);
    CHECK(actual.payload_length == expected.payload_length);
    CHECK(actual.sequence == expected.sequence);
    CHECK(actual.generation == expected.generation);
    CHECK(actual.payload_length <= FNR_TRANSPORT_PAYLOAD_CAPACITY);
    CHECK(std::memcmp(actual.payload, expected.payload, actual.payload_length) == 0);
}

fnr_transport_rx ready_rx() {
    fnr_transport_rx rx{};
    fnr_transport_rx_init(&rx);
    CHECK(fnr_transport_rx_take_events(&rx) == FNR_TRANSPORT_EVENT_RESET);
    CHECK(fnr_transport_rx_take_events(&rx) == 0);
    const uint8_t delimiter = 0;
    CHECK(fnr_transport_rx_feed(&rx, &delimiter, 1) == 0);
    CHECK(!rx.discarding && rx.encoded_size == 0);
    return rx;
}

size_t feed(fnr_transport_rx &rx, const Bytes &wire) {
    return fnr_transport_rx_feed(&rx, wire.data(), wire.size());
}

fnr_transport_frame take_datagram(fnr_transport_rx &rx) {
    fnr_transport_frame output{};
    CHECK(fnr_transport_rx_take_datagram(&rx, &output));
    CHECK(!rx.datagram_pending);
    return output;
}

void test_crc_and_cobs() {
    CHECK(FNR_TRANSPORT_HEADER_SIZE == 16);
    CHECK(FNR_TRANSPORT_PAYLOAD_CAPACITY == 256);
    CHECK(FNR_TRANSPORT_RAW_CAPACITY == 274);
    CHECK(FNR_TRANSPORT_ENCODED_CAPACITY == 276);
    CHECK(FNR_TRANSPORT_WIRE_CAPACITY == 277);
    const Bytes digits{'1','2','3','4','5','6','7','8','9'};
    CHECK(fnr_transport_crc16(digits.data(), digits.size()) == 0x29b1);
    CHECK(fnr_transport_crc16(nullptr, 0) == 0xffff);
    Bytes all_bytes(256);
    for (size_t n = 0; n < all_bytes.size(); ++n) all_bytes[n] = static_cast<uint8_t>(n);
    CHECK(fnr_transport_crc16(all_bytes.data(), all_bytes.size()) == 0x3fbd);
    CHECK(reference_crc(golden_raw.data(), golden_raw.size() - 2) == 0x9a3b);
    CHECK(reference_wire(golden_raw) == golden_wire);
    CHECK(fnr_transport_cobs_bound(0) == 1);
    CHECK(fnr_transport_cobs_bound(253) == 254);
    CHECK(fnr_transport_cobs_bound(254) == 256);
    CHECK(fnr_transport_cobs_bound(255) == 257);
    CHECK(fnr_transport_cobs_bound(274) == 276);
    CHECK(fnr_transport_cobs_bound(std::numeric_limits<size_t>::max()) == 0);
    CHECK(fnr_transport_cobs_bound(std::numeric_limits<size_t>::max() - 1) == 0);

    const std::vector<std::pair<Bytes, Bytes>> vectors = {
        {{}, {1}},
        {{0}, {1,1}},
        {{0,0}, {1,1,1}},
        {{0x11,0x22,0,0x33}, {3,0x11,0x22,2,0x33}},
        {{0x11,0x22,0x33}, {4,0x11,0x22,0x33}},
        {{0x11,0,0,0}, {2,0x11,1,1,1}},
        {{0,0x11,0}, {1,2,0x11,1}}
    };
    for (const auto &vector : vectors) {
        Bytes encoded(vector.second.size() + 8, 0xa5);
        size_t size = 999;
        CHECK(fnr_transport_cobs_encode(vector.first.data(), vector.first.size(),
                                       encoded.data(), vector.second.size(), &size) ==
              FNR_TRANSPORT_OK);
        CHECK(size == vector.second.size());
        CHECK(std::equal(vector.second.begin(), vector.second.end(), encoded.begin()));
        for (size_t n = size; n < encoded.size(); ++n) CHECK(encoded[n] == 0xa5);
        Bytes decoded(vector.first.size() + 8, 0x5a);
        CHECK(fnr_transport_cobs_decode(vector.second.data(), vector.second.size(),
                                       decoded.data(), vector.first.size(), &size) ==
              FNR_TRANSPORT_OK);
        CHECK(size == vector.first.size());
        CHECK(std::equal(vector.first.begin(), vector.first.end(), decoded.begin()));
        for (size_t n = size; n < decoded.size(); ++n) CHECK(decoded[n] == 0x5a);
    }

    Random random;
    for (size_t length = 0; length <= 1024; ++length) {
        for (unsigned pattern = 0; pattern < 4; ++pattern) {
            Bytes raw(length);
            for (size_t n = 0; n < length; ++n) {
                raw[n] = pattern == 0 ? 0 : pattern == 1 ? 0x7f :
                    pattern == 2 ? static_cast<uint8_t>(n) : random.byte();
            }
            Bytes expected = reference_wire(raw);
            expected.pop_back();
            const size_t capacity = fnr_transport_cobs_bound(length);
            Bytes encoded(capacity + 8, 0xa5);
            size_t size = 0;
            CHECK(fnr_transport_cobs_encode(raw.data(), length, encoded.data(),
                                           capacity, &size) == FNR_TRANSPORT_OK);
            CHECK(size <= capacity && size == expected.size());
            CHECK(std::equal(expected.begin(), expected.end(), encoded.begin()));
            for (size_t n = capacity; n < encoded.size(); ++n) CHECK(encoded[n] == 0xa5);
            Bytes decoded(length + 8, 0x5a);
            size_t decoded_size = 0;
            CHECK(fnr_transport_cobs_decode(encoded.data(), size, decoded.data(),
                                           length, &decoded_size) == FNR_TRANSPORT_OK);
            CHECK(decoded_size == length);
            CHECK(std::equal(raw.begin(), raw.end(), decoded.begin()));
            for (size_t n = length; n < decoded.size(); ++n) CHECK(decoded[n] == 0x5a);
            size_t ignored = 999;
            CHECK(fnr_transport_cobs_encode(raw.data(), length, encoded.data(),
                                           expected.size() - 1, &ignored) ==
                  FNR_TRANSPORT_NO_SPACE);
            CHECK(ignored == 0);
            if (length != 0) {
                CHECK(fnr_transport_cobs_decode(expected.data(), expected.size(),
                                               decoded.data(), length - 1, &ignored) ==
                      FNR_TRANSPORT_NO_SPACE);
                CHECK(ignored == 0);
            }
            CHECK(fnr_transport_crc16(raw.data(), length) == reference_crc(raw.data(), length));
        }
    }

    Bytes run(254, 0x6d);
    Bytes long_form{0xff};
    long_form.insert(long_form.end(), run.begin(), run.end());
    Bytes decoded(254);
    size_t decoded_size = 0;
    CHECK(fnr_transport_cobs_decode(long_form.data(), long_form.size(), decoded.data(),
                                   decoded.size(), &decoded_size) == FNR_TRANSPORT_OK);
    CHECK(decoded_size == 254 && decoded == run);
    long_form.push_back(1);
    CHECK(fnr_transport_cobs_decode(long_form.data(), long_form.size(), decoded.data(),
                                   decoded.size(), &decoded_size) == FNR_TRANSPORT_OK);
    CHECK(decoded_size == 254 && decoded == run);

    const std::vector<Bytes> malformed = {{0}, {3,1}, {2,0}, {1,0}, {0xff,1,2}};
    for (const Bytes &input : malformed) {
        std::array<uint8_t, 512> output{};
        size_t size = 999;
        CHECK(fnr_transport_cobs_decode(input.data(), input.size(), output.data(),
                                       output.size(), &size) == FNR_TRANSPORT_MALFORMED_COBS);
        CHECK(size == 0);
    }
    uint8_t byte = 1;
    size_t size = 999;
    CHECK(fnr_transport_cobs_encode(nullptr, 1, &byte, 1, &size) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(size == 0);
    CHECK(fnr_transport_cobs_encode(&byte, 1, nullptr, 1, &size) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_cobs_encode(&byte, 1, &byte, 1, nullptr) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_cobs_decode(nullptr, 1, &byte, 1, &size) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_cobs_decode(&byte, 1, nullptr, 1, &size) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_cobs_decode(&byte, 1, &byte, 1, nullptr) == FNR_TRANSPORT_INVALID_ARGUMENT);
    std::printf("PASS transport CRC, immutable COBS vectors, 4,100 bounded reference round trips\n");
}

void test_frame_codec() {
    const auto golden = datagram_frame(1, 0x89abcdefu, UINT64_C(0x0102030405060708));
    CHECK(encode_frame(golden) == golden_wire);
    fnr_transport_frame decoded{};
    CHECK(fnr_transport_decode(golden_wire.data(), golden_wire.size() - 1, &decoded) ==
          FNR_TRANSPORT_OK);
    equal_frames(decoded, golden);
    CHECK(decoded.payload_length == 7);
    CHECK(decoded.sequence == 0x89abcdefu);
    CHECK(decoded.generation == UINT64_C(0x0102030405060708));
    CHECK(fnr_transport_sequence_next(0) == 1);
    CHECK(fnr_transport_sequence_next(UINT32_MAX - 1) == UINT32_MAX);
    CHECK(fnr_transport_sequence_next(UINT32_MAX) == 0);

    Random random;
    size_t maximum_wire_size = 0;
    for (size_t length = 1; length <= FNR_DATAGRAM_CAPACITY; ++length) {
        for (unsigned pattern = 0; pattern < 3; ++pattern) {
            const uint32_t sequence = random.next();
            const uint64_t high_generation = random.next();
            const uint64_t generation = (high_generation << 32) | random.next();
            auto frame = datagram_frame(length, sequence, generation);
            for (size_t n = 0; n < frame.payload_length; ++n) {
                frame.payload[n] = pattern == 0 ? 0 : pattern == 1 ? 0xff : random.byte();
            }
            const Bytes payload(frame.payload, frame.payload + frame.payload_length);
            const Bytes reference = reference_raw(1, 1, static_cast<uint16_t>(payload.size()),
                                                  frame.sequence, frame.generation, payload);
            const Bytes wire = encode_frame(frame);
            maximum_wire_size = std::max(maximum_wire_size, wire.size());
            CHECK(wire == reference_wire(reference));
            CHECK(fnr_transport_decode(wire.data(), wire.size() - 1, &decoded) == FNR_TRANSPORT_OK);
            equal_frames(decoded, frame);
            fnr_datagram datagram{};
            CHECK(fnr_transport_unpack_datagram(&decoded, &datagram) == FNR_TRANSPORT_OK);
            CHECK(datagram.size == length);
            CHECK(std::memcmp(datagram.source_mac, payload.data(), FNR_MAC_SIZE) == 0);
            CHECK(std::memcmp(datagram.payload, payload.data() + FNR_MAC_SIZE, length) == 0);
            for (size_t n = length; n < FNR_DATAGRAM_CAPACITY; ++n) CHECK(datagram.payload[n] == 0);
            const auto owned = datagram;
            std::memset(decoded.payload, 0x3c, sizeof(decoded.payload));
            CHECK(std::memcmp(owned.payload, datagram.payload, length) == 0);
        }
    }
    CHECK(maximum_wire_size == FNR_TRANSPORT_WIRE_CAPACITY);

    const auto status = status_frame(0x12345678, UINT64_C(0xfedcba9876543210),
                                     FNR_TRANSPORT_STATUS_RESET);
    const Bytes expected_status = {
        1,2,6,0,
        4,3,2,1,0x14,0x13,0x12,0x11,0x24,0x23,0x22,0x21,
        0x34,0x33,0x32,0x31,0x44,0x43,0x42,0x41,0x54,0x53,0x52,0x51,
        0x64,0x63,0x62,0x61,0x74,0x73,0x72,0x71,0x84,0x83,0x82,0x81,
        0x94,0x93,0x92,0x91,0xa4,0xa3,0xa2,0xa1
    };
    CHECK(status.payload_length == 48 && expected_status.size() == 48);
    CHECK(std::memcmp(status.payload, expected_status.data(), 48) == 0);
    const Bytes status_wire = encode_frame(status);
    CHECK(status_wire == reference_wire(reference_raw(1, 2, 48, 0x12345678,
        UINT64_C(0xfedcba9876543210), expected_status)));
    CHECK(fnr_transport_decode(status_wire.data(), status_wire.size() - 1, &decoded) == FNR_TRANSPORT_OK);
    equal_frames(decoded, status);
    fnr_transport_radio_status unpacked{};
    CHECK(fnr_transport_unpack_status(&decoded, &unpacked) == FNR_TRANSPORT_OK);
    CHECK(unpacked.flags == 1 && unpacked.state == FNR_RADIO_READY && unpacked.channel == 6);
    CHECK(unpacked.radio.received == 0x01020304 && unpacked.radio.accepted == 0x11121314);
    CHECK(unpacked.radio.rejected_source == 0x21222324 && unpacked.radio.invalid_length == 0x31323334);
    CHECK(unpacked.radio.dropped == 0x41424344 && unpacked.radio.consumed == 0x51525354);
    CHECK(unpacked.radio.starts == 0x61626364 && unpacked.radio.restarts == 0x71727374);
    CHECK(unpacked.radio.errors == 0x81828384);
    CHECK(unpacked.tx_replaced == 0x91929394 && unpacked.tx_aborted == 0xa1a2a3a4);

    for (const fnr_radio_state state : {
        FNR_RADIO_UNCONFIGURED, FNR_RADIO_STARTING, FNR_RADIO_READY, FNR_RADIO_ERROR
    }) {
        for (const uint8_t channel : {uint8_t{0}, uint8_t{1}, uint8_t{14}, uint8_t{15}}) {
            auto candidate = status_input(0, state);
            candidate.channel = channel;
            const bool valid = channel <= 14 && (state != FNR_RADIO_READY || channel >= 1);
            fnr_transport_frame encoded_status{};
            CHECK((fnr_transport_make_status(&encoded_status, &candidate, 0, 0) ==
                   FNR_TRANSPORT_OK) == valid);
            if (valid) {
                const Bytes candidate_wire = encode_frame(encoded_status);
                CHECK(fnr_transport_decode(candidate_wire.data(), candidate_wire.size() - 1,
                                           &decoded) == FNR_TRANSPORT_OK);
                equal_frames(decoded, encoded_status);
            }
        }
    }

    fnr_datagram bad_datagram{};
    std::memset(&decoded, 0xa5, sizeof(decoded));
    std::array<uint8_t, sizeof(decoded)> before{};
    std::memcpy(before.data(), &decoded, sizeof(decoded));
    for (const uint16_t length : {uint16_t{0}, uint16_t{251}, uint16_t{65535}}) {
        bad_datagram.size = length;
        CHECK(fnr_transport_make_datagram(&decoded, &bad_datagram, 0, 0) != FNR_TRANSPORT_OK);
        CHECK(std::memcmp(before.data(), &decoded, sizeof(decoded)) == 0);
    }
    CHECK(fnr_transport_make_datagram(nullptr, &bad_datagram, 0, 0) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_make_datagram(&decoded, nullptr, 0, 0) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_make_status(&decoded, nullptr, 0, 0) == FNR_TRANSPORT_INVALID_ARGUMENT);
    const auto valid_status = status_input();
    CHECK(fnr_transport_make_status(nullptr, &valid_status, 0, 0) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_unpack_datagram(&status, &bad_datagram) != FNR_TRANSPORT_OK);
    CHECK(fnr_transport_unpack_status(&golden, &unpacked) != FNR_TRANSPORT_OK);
    CHECK(fnr_transport_unpack_datagram(nullptr, &bad_datagram) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_unpack_datagram(&golden, nullptr) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_unpack_status(nullptr, &unpacked) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_unpack_status(&status, nullptr) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_decode(nullptr, 1, &decoded) == FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_decode(golden_wire.data(), golden_wire.size() - 1, nullptr) ==
          FNR_TRANSPORT_INVALID_ARGUMENT);
    std::array<uint8_t, FNR_TRANSPORT_WIRE_CAPACITY> output{};
    size_t written = 999;
    CHECK(fnr_transport_encode(nullptr, output.data(), output.size(), &written) ==
          FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(written == 0);
    CHECK(fnr_transport_encode(&golden, nullptr, output.size(), &written) ==
          FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_encode(&golden, output.data(), output.size(), nullptr) ==
          FNR_TRANSPORT_INVALID_ARGUMENT);
    CHECK(fnr_transport_encode(&golden, output.data(), golden_wire.size() - 1, &written) ==
          FNR_TRANSPORT_NO_SPACE);
    CHECK(written == 0);
    std::printf("PASS transport frame golden bytes, 750 all-length cases, status counters, local wrap\n");
}

void test_streaming() {
    const auto frame = datagram_frame(250, UINT32_MAX, UINT64_C(0x0807060504030201), 0);
    const Bytes wire = encode_frame(frame);
    auto rx = ready_rx();
    for (size_t n = 0; n < wire.size(); ++n) {
        CHECK(fnr_transport_rx_feed(&rx, wire.data() + n, 1) == (n + 1 == wire.size() ? 1u : 0u));
        CHECK(rx.encoded_size <= FNR_TRANSPORT_ENCODED_CAPACITY);
        if (n + 1 != wire.size()) {
            fnr_transport_frame premature{};
            CHECK(!fnr_transport_rx_take_datagram(&rx, &premature));
        }
    }
    CHECK(fnr_transport_rx_take_events(&rx) == 0);
    equal_frames(take_datagram(rx), frame);
    CHECK(rx.counters.frames_valid == 1 && rx.counters.datagrams_consumed == 1);

    /* Every possible split verifies persistence across a call boundary, including
     * the delimiter boundary and zero-length calls. */
    for (size_t split = 0; split <= wire.size(); ++split) {
        rx = ready_rx();
        CHECK(fnr_transport_rx_feed(&rx, wire.data(), split) == (split == wire.size() ? 1u : 0u));
        CHECK(fnr_transport_rx_feed(&rx, wire.data() + split, wire.size() - split) ==
              (split == wire.size() ? 0u : 1u));
        CHECK(fnr_transport_rx_take_events(&rx) == 0);
        equal_frames(take_datagram(rx), frame);
    }
    for (size_t chunk = 1; chunk <= wire.size() + 1; ++chunk) {
        rx = ready_rx();
        size_t valid = 0;
        for (size_t offset = 0; offset < wire.size();) {
            const size_t length = std::min(chunk, wire.size() - offset);
            valid += fnr_transport_rx_feed(&rx, wire.data() + offset, length);
            offset += length;
        }
        CHECK(valid == 1 && rx.counters.frames_valid == 1);
        CHECK(fnr_transport_rx_take_events(&rx) == 0);
        equal_frames(take_datagram(rx), frame);
    }

    rx = ready_rx();
    const auto first = datagram_frame(32, 17, 2, 0x11);
    const auto second = datagram_frame(250, 18, 2, 0x22);
    Bytes concatenated = encode_frame(first);
    const Bytes second_wire = encode_frame(second);
    concatenated.insert(concatenated.end(), second_wire.begin(), second_wire.end());
    CHECK(feed(rx, concatenated) == 2);
    CHECK(rx.counters.frames_valid == 2 && rx.counters.datagrams_received == 2);
    CHECK(rx.counters.datagrams_replaced == 1);
    fnr_transport_frame output{};
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
    CHECK((fnr_transport_rx_take_events(&rx) & FNR_TRANSPORT_EVENT_DATAGRAM_REPLACED) != 0);
    equal_frames(take_datagram(rx), second);
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));

    rx = ready_rx();
    const auto mixed_datagram = datagram_frame(1, 1, 7);
    const auto mixed_status = status_frame(2, 7);
    Bytes mixed = encode_frame(mixed_datagram);
    const Bytes mixed_status_wire = encode_frame(mixed_status);
    mixed.insert(mixed.end(), mixed_status_wire.begin(), mixed_status_wire.end());
    CHECK(feed(rx, mixed) == 2);
    CHECK(fnr_transport_rx_take_events(&rx) == 0);
    equal_frames(take_datagram(rx), mixed_datagram);
    CHECK(fnr_transport_rx_take_status(&rx, &output));
    equal_frames(output, mixed_status);
    CHECK(!fnr_transport_rx_take_status(&rx, &output));

    rx = ready_rx();
    const uint32_t prior_empty = rx.counters.empty_delimiters;
    CHECK(feed(rx, Bytes(128, 0)) == 0);
    CHECK(rx.counters.empty_delimiters == prior_empty + 128);
    CHECK(rx.events == 0 && rx.encoded_size == 0 && !rx.discarding);
    CHECK(feed(rx, golden_wire) == 1);
    CHECK(fnr_transport_rx_take_events(&rx) == 0);
    equal_frames(take_datagram(rx), datagram_frame(1, 0x89abcdefu, UINT64_C(0x0102030405060708)));
    CHECK(fnr_transport_rx_feed(&rx, nullptr, 0) == 0);
    CHECK(fnr_transport_rx_feed(&rx, nullptr, 3) == 0);
    CHECK(fnr_transport_rx_feed(nullptr, golden_wire.data(), golden_wire.size()) == 0);
    std::printf("PASS transport bytewise, every split/chunk, concatenated and mixed frames, delimiters\n");
}

using Counter = uint32_t fnr_transport_rx_counters::*;

void check_rejection_and_recovery(const Bytes &wire, fnr_transport_result reason,
                                 Counter counter) {
    CHECK(!wire.empty() && wire.back() == 0);
    fnr_transport_frame output{};
    std::memset(&output, 0xa5, sizeof(output));
    std::array<uint8_t, sizeof(output)> before{};
    std::memcpy(before.data(), &output, sizeof(output));
    CHECK(fnr_transport_decode(wire.data(), wire.size() - 1, &output) == reason);
    CHECK(std::memcmp(before.data(), &output, sizeof(output)) == 0);
    auto rx = ready_rx();
    const size_t split = wire.size() / 2;
    CHECK(fnr_transport_rx_feed(&rx, wire.data(), split) == 0);
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
    CHECK(fnr_transport_rx_feed(&rx, wire.data() + split, wire.size() - split) == 0);
    CHECK(rx.counters.*counter == 1);
    CHECK(rx.counters.frames_valid == 0 && !rx.datagram_pending && !rx.status_pending);
    CHECK(!rx.discarding && rx.encoded_size == 0);
    CHECK((fnr_transport_rx_take_events(&rx) & FNR_TRANSPORT_EVENT_LINK_ERROR) != 0);
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
    CHECK(!fnr_transport_rx_take_status(&rx, &output));
    /* The rejecting delimiter itself re-establishes framing. No extra reset,
     * recovery frame or second delimiter is required here. */
    CHECK(feed(rx, golden_wire) == 1);
    CHECK(fnr_transport_rx_take_events(&rx) == 0);
    equal_frames(take_datagram(rx), datagram_frame(1, 0x89abcdefu, UINT64_C(0x0102030405060708)));
}

void test_rejection_and_recovery() {
    check_rejection_and_recovery({3,1,0}, FNR_TRANSPORT_MALFORMED_COBS,
                                &fnr_transport_rx_counters::bad_cobs);
    check_rejection_and_recovery({1,0}, FNR_TRANSPORT_INVALID_LENGTH,
                                &fnr_transport_rx_counters::bad_length);
    Bytes raw = golden_raw;
    raw.back() ^= 0x80;
    check_rejection_and_recovery(reference_wire(raw), FNR_TRANSPORT_BAD_CRC,
                                &fnr_transport_rx_counters::bad_crc);
    raw = golden_raw;
    raw[0] = 2;
    replace_crc(raw);
    check_rejection_and_recovery(reference_wire(raw), FNR_TRANSPORT_UNSUPPORTED_VERSION,
                                &fnr_transport_rx_counters::bad_version);
    for (const uint8_t type : {uint8_t{0}, uint8_t{3}, uint8_t{4}, uint8_t{255}}) {
        raw = golden_raw;
        raw[1] = type;
        replace_crc(raw);
        check_rejection_and_recovery(reference_wire(raw), FNR_TRANSPORT_UNKNOWN_TYPE,
                                    &fnr_transport_rx_counters::bad_type);
    }
    for (const uint16_t length : {uint16_t{0}, uint16_t{6}, uint16_t{8}, uint16_t{257}, uint16_t{65535}}) {
        raw = golden_raw;
        raw[2] = static_cast<uint8_t>(length);
        raw[3] = static_cast<uint8_t>(length >> 8);
        replace_crc(raw);
        check_rejection_and_recovery(reference_wire(raw), FNR_TRANSPORT_INVALID_LENGTH,
                                    &fnr_transport_rx_counters::bad_length);
    }
    for (uint16_t length = 0; length < FNR_MAC_SIZE + 1; ++length) {
        check_rejection_and_recovery(reference_wire(reference_raw(1, 1, length, 0, 0,
                                      Bytes(length, 0))), FNR_TRANSPORT_INVALID_LENGTH,
                                     &fnr_transport_rx_counters::bad_length);
    }
    for (const uint16_t length : {uint16_t{0}, uint16_t{47}, uint16_t{49}}) {
        check_rejection_and_recovery(reference_wire(reference_raw(1, 2, length, 0, 0,
                                      Bytes(length, 0))), FNR_TRANSPORT_INVALID_LENGTH,
                                     &fnr_transport_rx_counters::bad_length);
    }
    const auto status = status_frame();
    const Bytes status_payload(status.payload, status.payload + status.payload_length);
    const Bytes status_raw = reference_raw(1, 2, 48, 0, 1, status_payload);
    for (const std::pair<size_t, uint8_t> &change : {
        std::pair<size_t, uint8_t>{16,2}, {17,4}, {18,0}, {18,15}, {19,1}
    }) {
        raw = status_raw;
        raw[change.first] = change.second;
        replace_crc(raw);
        check_rejection_and_recovery(reference_wire(raw), FNR_TRANSPORT_INVALID_PAYLOAD,
                                    &fnr_transport_rx_counters::bad_payload);
    }

    /* Every single-byte deletion, insertion and nonzero-bit mutation of the
     * independent fixture must reject, then accept a frame after the delimiter.
     * Keep the delimiter intact here so the recovery boundary is unambiguous. */
    for (unsigned operation = 0; operation < 3; ++operation) {
        for (size_t offset = 0; offset + 1 < golden_wire.size(); ++offset) {
            Bytes corrupt = golden_wire;
            if (operation == 0) {
                corrupt.erase(corrupt.begin() + static_cast<ptrdiff_t>(offset));
            } else if (operation == 1) {
                corrupt.insert(corrupt.begin() + static_cast<ptrdiff_t>(offset), 0x7e);
            } else {
                corrupt[offset] ^= 0x80;
            }
            auto rx = ready_rx();
            CHECK(feed(rx, corrupt) == 0);
            CHECK(rx.counters.frames_valid == 0);
            fnr_transport_frame output{};
            CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
            CHECK((fnr_transport_rx_take_events(&rx) & FNR_TRANSPORT_EVENT_LINK_ERROR) != 0);
            CHECK(feed(rx, golden_wire) == 1);
            CHECK(fnr_transport_rx_take_events(&rx) == 0);
            equal_frames(take_datagram(rx), datagram_frame(1, 0x89abcdefu, UINT64_C(0x0102030405060708)));
        }
    }

    auto rx = ready_rx();
    Bytes oversized(FNR_TRANSPORT_ENCODED_CAPACITY + 33, 0x55);
    CHECK(feed(rx, oversized) == 0);
    CHECK(rx.discarding && rx.encoded_size == 0);
    CHECK(rx.counters.oversized_frames == 1);
    CHECK(feed(rx, oversized) == 0);
    CHECK(rx.counters.oversized_frames == 1);
    CHECK(feed(rx, Bytes{0}) == 0);
    CHECK(!rx.discarding);
    CHECK((fnr_transport_rx_take_events(&rx) & FNR_TRANSPORT_EVENT_LINK_ERROR) != 0);
    CHECK(feed(rx, golden_wire) == 1);
    CHECK(fnr_transport_rx_take_events(&rx) == 0);
    (void)take_datagram(rx);

    /* A missing delimiter can sacrifice the next frame, but its delimiter ends
     * the bad segment. The following intact frame must always recover. */
    rx = ready_rx();
    Bytes without_delimiter = golden_wire;
    without_delimiter.pop_back();
    CHECK(feed(rx, without_delimiter) == 0);
    CHECK(feed(rx, golden_wire) == 0);
    CHECK(fnr_transport_rx_take_events(&rx) != 0);
    CHECK(feed(rx, golden_wire) == 1);
    CHECK(fnr_transport_rx_take_events(&rx) == 0);
    (void)take_datagram(rx);

    for (size_t truncated = 1; truncated + 1 < golden_wire.size(); ++truncated) {
        rx = ready_rx();
        CHECK(fnr_transport_rx_feed(&rx, golden_wire.data(), truncated) == 0);
        CHECK(feed(rx, Bytes{0}) == 0);
        CHECK((fnr_transport_rx_take_events(&rx) & FNR_TRANSPORT_EVENT_LINK_ERROR) != 0);
        CHECK(feed(rx, golden_wire) == 1);
        CHECK(fnr_transport_rx_take_events(&rx) == 0);
        (void)take_datagram(rx);
    }
    std::printf("PASS transport CRC-valid bad headers/status, all byte mutations, oversize/truncation recovery\n");
}

void test_bounded_ownership_and_events() {
    fnr_transport_rx rx{};
    fnr_transport_rx_init(&rx);
    fnr_transport_frame output{};
    /* Startup begins midstream safely: the first complete segment is discarded
     * until its zero, and the initial reset event cannot be bypassed. */
    CHECK(feed(rx, golden_wire) == 0);
    CHECK(rx.counters.bytes_discarded == golden_wire.size() - 1);
    CHECK(feed(rx, golden_wire) == 1);
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
    CHECK(fnr_transport_rx_take_events(&rx) == FNR_TRANSPORT_EVENT_RESET);
    (void)take_datagram(rx);

    rx = ready_rx();
    const auto first = datagram_frame(8, 40, 7, 0x11);
    const auto second = datagram_frame(16, 41, 7, 0x22);
    const auto third = datagram_frame(250, 42, 7, 0x33);
    CHECK(feed(rx, encode_frame(first)) == 1);
    CHECK(!fnr_transport_rx_take_datagram(&rx, nullptr));
    CHECK(rx.datagram_pending && rx.counters.datagrams_consumed == 0);
    CHECK(feed(rx, encode_frame(second)) == 1);
    CHECK(feed(rx, encode_frame(third)) == 1);
    CHECK(rx.counters.datagrams_replaced == 2 && rx.counters.datagrams_dropped == 2);
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
    CHECK(fnr_transport_rx_take_events(&rx) == FNR_TRANSPORT_EVENT_DATAGRAM_REPLACED);
    equal_frames(take_datagram(rx), third);
    CHECK(rx.counters.datagrams_received == 3 && rx.counters.datagrams_consumed == 1);
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));

    /* Once taken, a frame owns its bytes independently of both parser storage
     * and the caller's input buffer. */
    const auto fourth = datagram_frame(250, 43, 7, 0x44);
    Bytes mutable_wire = encode_frame(fourth);
    CHECK(feed(rx, mutable_wire) == 1);
    mutable_wire.assign(mutable_wire.size(), 0xee);
    output = take_datagram(rx);
    equal_frames(output, fourth);
    const auto fifth = datagram_frame(250, 44, 7, 0x55);
    CHECK(feed(rx, encode_frame(fifth)) == 1);
    equal_frames(output, fourth);
    equal_frames(take_datagram(rx), fifth);

    rx = ready_rx();
    CHECK(feed(rx, encode_frame(datagram_frame(1, 0, 4))) == 1);
    CHECK(feed(rx, encode_frame(status_frame(1, 4))) == 1);
    CHECK(feed(rx, encode_frame(datagram_frame(1, 2, 4, 0x42))) == 1);
    CHECK(rx.status_pending && rx.datagram_pending);
    CHECK(rx.counters.status_dropped == 0 && rx.counters.datagrams_dropped == 1);
    CHECK(fnr_transport_rx_take_events(&rx) == FNR_TRANSPORT_EVENT_DATAGRAM_REPLACED);
    CHECK(fnr_transport_rx_take_status(&rx, &output));
    equal_frames(output, status_frame(1, 4));
    equal_frames(take_datagram(rx), datagram_frame(1, 2, 4, 0x42));

    /* Status replacement must preserve the radio-reset event even if the
     * newest status no longer contains the RESET flag. */
    rx = ready_rx();
    CHECK(feed(rx, encode_frame(status_frame(0, 6, FNR_TRANSPORT_STATUS_RESET))) == 1);
    CHECK(feed(rx, encode_frame(status_frame(1, 6))) == 1);
    CHECK(feed(rx, encode_frame(datagram_frame(1, 2, 6))) == 1);
    CHECK(!fnr_transport_rx_take_status(&rx, &output));
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
    const uint32_t reset_events = fnr_transport_rx_take_events(&rx);
    CHECK((reset_events & FNR_TRANSPORT_EVENT_RADIO_RESET) != 0);
    CHECK((reset_events & FNR_TRANSPORT_EVENT_STATUS_REPLACED) != 0);
    CHECK(rx.counters.radio_resets == 1 && rx.counters.status_replaced == 1);
    CHECK(rx.counters.status_dropped == 1);
    CHECK(fnr_transport_rx_take_status(&rx, &output));
    equal_frames(output, status_frame(1, 6));
    (void)take_datagram(rx);

    rx = ready_rx();
    CHECK(feed(rx, encode_frame(datagram_frame(1, 0, 3))) == 1);
    CHECK(feed(rx, encode_frame(status_frame(1, 3))) == 1);
    CHECK(feed(rx, encode_frame(status_frame(2, 3, FNR_TRANSPORT_STATUS_RESET))) == 1);
    CHECK(rx.counters.datagrams_dropped == 1 && rx.counters.status_dropped == 1);
    CHECK(!rx.datagram_pending && rx.status_pending);
    CHECK(fnr_transport_rx_take_events(&rx) == FNR_TRANSPORT_EVENT_RADIO_RESET);
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
    CHECK(fnr_transport_rx_take_status(&rx, &output));

    for (const fnr_radio_state state : {FNR_RADIO_UNCONFIGURED, FNR_RADIO_STARTING, FNR_RADIO_ERROR}) {
        rx = ready_rx();
        CHECK(feed(rx, encode_frame(datagram_frame(1, 0, 3))) == 1);
        CHECK(feed(rx, encode_frame(status_frame(1, 3, 0, state))) == 1);
        CHECK(!rx.datagram_pending && rx.status_pending);
        CHECK(fnr_transport_rx_take_events(&rx) == FNR_TRANSPORT_EVENT_RADIO_UNAVAILABLE);
        CHECK(rx.counters.radio_unavailable == 1 && rx.counters.datagrams_dropped == 1);
        CHECK(fnr_transport_rx_take_status(&rx, &output));
        CHECK(output.payload[1] == static_cast<uint8_t>(state));
    }

    rx = ready_rx();
    CHECK(feed(rx, encode_frame(datagram_frame(1, 100, 1))) == 1);
    CHECK(feed(rx, encode_frame(status_frame(101, 1))) == 1);
    const auto new_generation = datagram_frame(1, 0, 2, 0x7b);
    CHECK(feed(rx, encode_frame(new_generation)) == 1);
    CHECK(rx.counters.generation_changes == 1 && rx.counters.sequence_discontinuities == 0);
    CHECK(rx.counters.datagrams_dropped == 1 && rx.counters.status_dropped == 1);
    CHECK(!rx.status_pending);
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
    CHECK(fnr_transport_rx_take_events(&rx) == FNR_TRANSPORT_EVENT_GENERATION_CHANGE);
    equal_frames(take_datagram(rx), new_generation);

    rx = ready_rx();
    for (const uint32_t sequence : {UINT32_MAX - 1, UINT32_MAX, uint32_t{0}, uint32_t{1}}) {
        const auto frame = datagram_frame(1, sequence, 1);
        CHECK(feed(rx, encode_frame(frame)) == 1);
        CHECK(fnr_transport_rx_take_events(&rx) == 0);
        equal_frames(take_datagram(rx), frame);
    }
    CHECK(rx.counters.sequence_discontinuities == 0);
    for (const uint32_t sequence : {uint32_t{1}, uint32_t{0}, uint32_t{100}, uint32_t{3}}) {
        const auto frame = datagram_frame(1, sequence, 1);
        CHECK(feed(rx, encode_frame(frame)) == 1);
        CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
        CHECK(fnr_transport_rx_take_events(&rx) == FNR_TRANSPORT_EVENT_SEQUENCE_DISCONTINUITY);
        equal_frames(take_datagram(rx), frame);
    }
    CHECK(rx.counters.sequence_discontinuities == 4);
    std::printf("PASS transport owned newest slots, status/reset event barriers, generation and diagnostic ordering\n");
}

void test_reset_discontinuity_and_saturation() {
    fnr_transport_frame output{};
    for (unsigned action = 0; action < 3; ++action) {
        auto rx = ready_rx();
        CHECK(feed(rx, encode_frame(datagram_frame(1, 50, 9))) == 1);
        CHECK(feed(rx, encode_frame(status_frame(51, 9))) == 1);
        const Bytes interrupted = encode_frame(datagram_frame(250, 52, 9));
        CHECK(fnr_transport_rx_feed(&rx, interrupted.data(), 20) == 0);
        CHECK(rx.encoded_size == 20);
        if (action == 0) fnr_transport_rx_reset(&rx);
        if (action == 1) fnr_transport_rx_discontinuity(&rx);
        if (action == 2) fnr_transport_rx_timeout(&rx);
        CHECK(rx.encoded_size == 0 && rx.discarding && !rx.have_sequence);
        CHECK(rx.have_generation == (action != 0));
        CHECK(!rx.datagram_pending && !rx.status_pending);
        CHECK(rx.counters.datagrams_dropped == 1 && rx.counters.status_dropped == 1);
        CHECK(rx.counters.frames_valid == 2);
        CHECK(rx.counters.parser_resets == (action == 0 ? 1u : 0u));
        CHECK(rx.counters.link_discontinuities == (action == 1 ? 1u : 0u));
        CHECK(rx.counters.link_timeouts == (action == 2 ? 1u : 0u));
        const uint32_t expected_event = action == 0 ? FNR_TRANSPORT_EVENT_RESET :
            action == 1 ? FNR_TRANSPORT_EVENT_LINK_ERROR : FNR_TRANSPORT_EVENT_TIMEOUT;
        CHECK(fnr_transport_rx_take_events(&rx) == expected_event);
        CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
        CHECK(!fnr_transport_rx_take_status(&rx, &output));
        CHECK(fnr_transport_rx_feed(&rx, interrupted.data() + 20, interrupted.size() - 20) == 0);
        CHECK(!rx.discarding && rx.counters.frames_valid == 2);
        const auto recovered = datagram_frame(1, 99, 9, 0x42);
        CHECK(feed(rx, encode_frame(recovered)) == 1);
        CHECK(fnr_transport_rx_take_events(&rx) == 0);
        equal_frames(take_datagram(rx), recovered);
    }

    /* Corruption drops older fully validated pending frames too. A later clean
     * frame may occupy a slot, but must wait for the invalidation event. */
    auto rx = ready_rx();
    CHECK(feed(rx, encode_frame(datagram_frame(1, 0, 1))) == 1);
    CHECK(feed(rx, encode_frame(status_frame(1, 1))) == 1);
    CHECK(feed(rx, Bytes{3,1,0}) == 0);
    CHECK(!rx.datagram_pending && !rx.status_pending);
    CHECK(rx.counters.datagrams_dropped == 1 && rx.counters.status_dropped == 1);
    CHECK(feed(rx, encode_frame(datagram_frame(1, 2, 1))) == 1);
    CHECK(!fnr_transport_rx_take_datagram(&rx, &output));
    CHECK(fnr_transport_rx_take_events(&rx) == FNR_TRANSPORT_EVENT_LINK_ERROR);
    equal_frames(take_datagram(rx), datagram_frame(1, 2, 1));

    rx = ready_rx();
    rx.counters.bytes_received = UINT32_MAX - 1;
    rx.counters.frames_valid = UINT32_MAX;
    rx.counters.datagrams_received = UINT32_MAX;
    rx.counters.datagrams_consumed = UINT32_MAX;
    CHECK(feed(rx, golden_wire) == 1);
    CHECK(rx.counters.bytes_received == UINT32_MAX && rx.counters.frames_valid == UINT32_MAX);
    CHECK(rx.counters.datagrams_received == UINT32_MAX);
    CHECK(fnr_transport_rx_take_events(&rx) == 0);
    (void)take_datagram(rx);
    CHECK(rx.counters.datagrams_consumed == UINT32_MAX);
    rx.counters.bad_cobs = UINT32_MAX;
    CHECK(feed(rx, Bytes{3,1,0}) == 0);
    CHECK(rx.counters.bad_cobs == UINT32_MAX);
    rx.counters.parser_resets = UINT32_MAX;
    fnr_transport_rx_reset(&rx);
    CHECK(rx.counters.parser_resets == UINT32_MAX);
    CHECK(rx.counters.frames_valid == UINT32_MAX);
    CHECK((fnr_transport_rx_take_events(&rx) & FNR_TRANSPORT_EVENT_RESET) != 0);
    fnr_transport_rx_init(&rx);
    CHECK(rx.counters.frames_valid == 0 && rx.counters.bytes_received == 0);
    CHECK(rx.counters.parser_resets == 0 && !rx.datagram_pending && !rx.status_pending);
    fnr_transport_rx_init(nullptr);
    fnr_transport_rx_reset(nullptr);
    fnr_transport_rx_discontinuity(nullptr);
    fnr_transport_rx_timeout(nullptr);
    CHECK(fnr_transport_rx_take_events(nullptr) == 0);
    CHECK(!fnr_transport_rx_take_datagram(nullptr, &output));
    CHECK(!fnr_transport_rx_take_status(nullptr, &output));
    std::printf("PASS transport parser/reset/interruption/timeout retirement and saturating diagnostics\n");
}

void validate_accepted_metadata(const fnr_transport_frame &frame) {
    CHECK(frame.version == 1);
    CHECK(frame.payload_length <= FNR_TRANSPORT_PAYLOAD_CAPACITY);
    CHECK(frame.type == FNR_TRANSPORT_RADIO_DATAGRAM || frame.type == FNR_TRANSPORT_RADIO_STATUS);
    if (frame.type == FNR_TRANSPORT_RADIO_DATAGRAM) {
        CHECK(frame.payload_length >= FNR_MAC_SIZE + 1);
        fnr_datagram datagram{};
        CHECK(fnr_transport_unpack_datagram(&frame, &datagram) == FNR_TRANSPORT_OK);
        CHECK(datagram.size >= 1 && datagram.size <= FNR_DATAGRAM_CAPACITY);
    } else {
        fnr_transport_radio_status status{};
        CHECK(fnr_transport_unpack_status(&frame, &status) == FNR_TRANSPORT_OK);
        CHECK(frame.payload_length == 48 && frame.payload[3] == 0);
        CHECK((status.flags & ~FNR_TRANSPORT_STATUS_RESET) == 0);
        CHECK(status.state >= FNR_RADIO_UNCONFIGURED && status.state <= FNR_RADIO_ERROR);
        CHECK(status.channel <= 14);
        CHECK(status.state != FNR_RADIO_READY || status.channel >= 1);
    }
}

void test_deterministic_hostile_streams() {
    struct GuardedRx {
        std::array<uint8_t, 32> before{};
        fnr_transport_rx rx{};
        std::array<uint8_t, 32> after{};
    } guarded;
    guarded.before.fill(0x6b);
    guarded.after.fill(0x9d);
    auto &rx = guarded.rx;
    rx = ready_rx();
    Random random;
    uint64_t bytes_checked = 0;
    unsigned accepted_by_decoder = 0;
    for (unsigned iteration = 0; iteration < 12000; ++iteration) {
        Bytes hostile;
        const unsigned mode = random.next() % 5;
        if (mode < 2) {
            hostile.resize(static_cast<size_t>(random.next() % 701));
            for (uint8_t &byte : hostile) byte = random.byte();
            if (mode == 0) std::replace(hostile.begin(), hostile.end(), uint8_t{0}, uint8_t{0x7f});
        } else if (mode == 2) {
            const size_t length = 1 + random.next() % 250;
            const uint32_t sequence = random.next();
            const uint64_t generation = random.next();
            hostile = encode_frame(datagram_frame(length, sequence, generation));
            const size_t mutations = 1 + random.next() % 4;
            for (size_t n = 0; n < mutations; ++n) {
                const size_t offset = static_cast<size_t>(random.next()) % hostile.size();
                hostile[offset] ^= static_cast<uint8_t>(1u << (random.next() % 8));
            }
        } else if (mode == 3) {
            const uint32_t sequence = random.next();
            const uint64_t generation = random.next();
            hostile = encode_frame(status_frame(sequence, generation));
        } else {
            const size_t count = random.next() % 701;
            const uint8_t value = random.byte();
            hostile.assign(count, value);
        }

        fnr_transport_frame decoded{};
        std::memset(&decoded, 0xa5, sizeof(decoded));
        std::array<uint8_t, sizeof(decoded)> prior{};
        std::memcpy(prior.data(), &decoded, sizeof(decoded));
        const size_t segment_size = hostile.size() - (!hostile.empty() && hostile.back() == 0 ? 1u : 0u);
        const auto result = fnr_transport_decode(hostile.data(), segment_size, &decoded);
        if (result == FNR_TRANSPORT_OK) {
            ++accepted_by_decoder;
            validate_accepted_metadata(decoded);
        } else {
            CHECK(std::memcmp(prior.data(), &decoded, sizeof(decoded)) == 0);
        }

        for (size_t offset = 0; offset < hostile.size();) {
            const size_t chunk = std::min(static_cast<size_t>(1 + random.next() % 47), hostile.size() - offset);
            const size_t accepted = fnr_transport_rx_feed(&rx, hostile.data() + offset, chunk);
            CHECK(accepted <= chunk);
            offset += chunk;
            bytes_checked += chunk;
            CHECK(rx.encoded_size <= FNR_TRANSPORT_ENCODED_CAPACITY);
            if (rx.discarding) CHECK(rx.encoded_size == 0);
            if ((random.next() & 3u) == 0) {
                if (rx.events != 0) {
                    CHECK(!fnr_transport_rx_take_datagram(&rx, &decoded));
                    CHECK(!fnr_transport_rx_take_status(&rx, &decoded));
                }
                (void)fnr_transport_rx_take_events(&rx);
                if (fnr_transport_rx_take_datagram(&rx, &decoded)) validate_accepted_metadata(decoded);
                if (fnr_transport_rx_take_status(&rx, &decoded)) validate_accepted_metadata(decoded);
            }
        }

        /* This asserts bounded forward progress after EVERY hostile input,
         * independent of its shape, size, last byte or partially held frame. */
        if (iteration % 3 == 0) fnr_transport_rx_reset(&rx);
        CHECK(feed(rx, Bytes{0}) == 0);
        (void)fnr_transport_rx_take_events(&rx);
        CHECK(feed(rx, golden_wire) == 1);
        (void)fnr_transport_rx_take_events(&rx);
        equal_frames(take_datagram(rx), datagram_frame(1, 0x89abcdefu, UINT64_C(0x0102030405060708)));
        (void)fnr_transport_rx_take_status(&rx, &decoded);
        CHECK(!fnr_transport_rx_take_datagram(&rx, &decoded));
        CHECK(std::all_of(guarded.before.begin(), guarded.before.end(), [](uint8_t byte) { return byte == 0x6b; }));
        CHECK(std::all_of(guarded.after.begin(), guarded.after.end(), [](uint8_t byte) { return byte == 0x9d; }));
    }
    CHECK(bytes_checked > 1000000);
    CHECK(accepted_by_decoder > 1000);
    std::printf("PASS transport fixed seed d6e8feb8: 12,000 hostile streams, %llu bytes, %u independent accepts\n",
                static_cast<unsigned long long>(bytes_checked), accepted_by_decoder);
}

}  // namespace

int main() {
    test_crc_and_cobs();
    test_frame_codec();
    test_streaming();
    test_rejection_and_recovery();
    test_bounded_ownership_and_events();
    test_reset_discontinuity_and_saturation();
    test_deterministic_hostile_streams();
}
