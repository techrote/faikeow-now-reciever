#include "fnr/transport.h"
#include <string.h>

static void bump(uint32_t *value) {
    if (*value != UINT32_MAX) ++*value;
}

static uint16_t get16(const uint8_t *bytes) {
    return (uint16_t)((uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u));
}

static uint32_t get32(const uint8_t *bytes) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4u; ++i)
        value |= (uint32_t)bytes[i] << (i * 8u);
    return value;
}

static uint64_t get64(const uint8_t *bytes) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8u; ++i)
        value |= (uint64_t)bytes[i] << (i * 8u);
    return value;
}

static void put16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8u);
}

static void put32(uint8_t *bytes, uint32_t value) {
    for (unsigned i = 0; i < 4u; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8u));
}

static void put64(uint8_t *bytes, uint64_t value) {
    for (unsigned i = 0; i < 8u; ++i)
        bytes[i] = (uint8_t)(value >> (i * 8u));
}

size_t fnr_transport_cobs_bound(size_t length) {
    const size_t overhead = length / 254u + 1u;
    if (length > SIZE_MAX - overhead) return 0;
    return length + overhead;
}

uint16_t fnr_transport_crc16(const uint8_t *bytes, size_t length) {
    uint32_t crc = UINT16_MAX;
    if (!bytes) return (uint16_t)crc;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint32_t)bytes[i] << 8u;
        for (unsigned bit = 0; bit < 8u; ++bit) {
            crc = (crc & 0x8000u) != 0u ?
                ((crc << 1u) ^ 0x1021u) & 0xffffu :
                (crc << 1u) & 0xffffu;
        }
    }
    return (uint16_t)crc;
}

fnr_transport_result fnr_transport_cobs_encode(const uint8_t *bytes, size_t length,
    uint8_t *out, size_t capacity, size_t *written) {
    if (!written) return FNR_TRANSPORT_INVALID_ARGUMENT;
    *written = 0;
    if (!out || (!bytes && length != 0)) return FNR_TRANSPORT_INVALID_ARGUMENT;
    /* Even the smallest encoding needs n+1 bytes. Check before reading input;
     * the bound check also handles SIZE_MAX without overflowing arithmetic. */
    if (fnr_transport_cobs_bound(length) == 0 || capacity <= length)
        return FNR_TRANSPORT_NO_SPACE;
    size_t code_index = 0;
    size_t write_index = 1;
    uint8_t code = 1;
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] == 0) {
            out[code_index] = code;
            if (write_index >= capacity) return FNR_TRANSPORT_NO_SPACE;
            code_index = write_index++;
            code = 1;
        } else {
            if (write_index >= capacity) return FNR_TRANSPORT_NO_SPACE;
            out[write_index++] = bytes[i];
            ++code;
            if (code == UINT8_MAX) {
                out[code_index] = code;
                if (write_index >= capacity) return FNR_TRANSPORT_NO_SPACE;
                code_index = write_index++;
                code = 1;
            }
        }
    }
    out[code_index] = code;
    *written = write_index;
    return FNR_TRANSPORT_OK;
}

fnr_transport_result fnr_transport_cobs_decode(const uint8_t *bytes, size_t length,
    uint8_t *out, size_t capacity, size_t *written) {
    if (!written) return FNR_TRANSPORT_INVALID_ARGUMENT;
    *written = 0;
    if (!out || (!bytes && length != 0)) return FNR_TRANSPORT_INVALID_ARGUMENT;
    if (length == 0) return FNR_TRANSPORT_MALFORMED_COBS;
    /* Reject an impossible encoded length before walking input. The bound may
     * be zero for enormous capacities; ordinary per-write checks still apply. */
    const size_t maximum = fnr_transport_cobs_bound(capacity);
    if (maximum != 0 && length > maximum) return FNR_TRANSPORT_NO_SPACE;
    size_t read_index = 0;
    size_t write_index = 0;
    while (read_index < length) {
        const uint8_t code = bytes[read_index++];
        if (code == 0) return FNR_TRANSPORT_MALFORMED_COBS;
        const size_t count = (size_t)code - 1u;
        if (count > length - read_index) return FNR_TRANSPORT_MALFORMED_COBS;
        if (count > capacity - write_index) return FNR_TRANSPORT_NO_SPACE;
        for (size_t i = 0; i < count; ++i) {
            const uint8_t byte = bytes[read_index++];
            if (byte == 0) return FNR_TRANSPORT_MALFORMED_COBS;
            out[write_index++] = byte;
        }
        if (code != UINT8_MAX && read_index < length) {
            if (write_index >= capacity) return FNR_TRANSPORT_NO_SPACE;
            out[write_index++] = 0;
        }
    }
    *written = write_index;
    return FNR_TRANSPORT_OK;
}

static bool status_values_valid(uint8_t flags, fnr_radio_state state,
    uint8_t channel) {
    if ((flags & (uint8_t)~FNR_TRANSPORT_STATUS_RESET) != 0 ||
        state < FNR_RADIO_UNCONFIGURED || state > FNR_RADIO_ERROR ||
        channel > 14u)
        return false;
    return state != FNR_RADIO_READY || channel >= 1u;
}

static fnr_transport_result validate_frame(const fnr_transport_frame *frame) {
    if (!frame) return FNR_TRANSPORT_INVALID_ARGUMENT;
    if (frame->version != FNR_TRANSPORT_VERSION)
        return FNR_TRANSPORT_UNSUPPORTED_VERSION;
    if (frame->type != FNR_TRANSPORT_RADIO_DATAGRAM &&
        frame->type != FNR_TRANSPORT_RADIO_STATUS)
        return FNR_TRANSPORT_UNKNOWN_TYPE;
    if (frame->payload_length > FNR_TRANSPORT_PAYLOAD_CAPACITY)
        return FNR_TRANSPORT_INVALID_LENGTH;
    if (frame->type == FNR_TRANSPORT_RADIO_DATAGRAM) {
        if (frame->payload_length < FNR_MAC_SIZE + 1u)
            return FNR_TRANSPORT_INVALID_LENGTH;
    } else {
        if (frame->payload_length != FNR_TRANSPORT_STATUS_SIZE)
            return FNR_TRANSPORT_INVALID_LENGTH;
        if (frame->payload[3] != 0 ||
            !status_values_valid(frame->payload[0],
                (fnr_radio_state)frame->payload[1], frame->payload[2]))
            return FNR_TRANSPORT_INVALID_PAYLOAD;
    }
    return FNR_TRANSPORT_OK;
}

fnr_transport_result fnr_transport_encode(const fnr_transport_frame *frame,
    uint8_t *out, size_t capacity, size_t *written) {
    if (!written) return FNR_TRANSPORT_INVALID_ARGUMENT;
    *written = 0;
    if (!out) return FNR_TRANSPORT_INVALID_ARGUMENT;
    const fnr_transport_result valid = validate_frame(frame);
    if (valid != FNR_TRANSPORT_OK) return valid;
    if (capacity == 0) return FNR_TRANSPORT_NO_SPACE;
    uint8_t raw[FNR_TRANSPORT_RAW_CAPACITY];
    const size_t covered = FNR_TRANSPORT_HEADER_SIZE + (size_t)frame->payload_length;
    raw[0] = frame->version;
    raw[1] = frame->type;
    put16(raw + 2, frame->payload_length);
    put32(raw + 4, frame->sequence);
    put64(raw + 8, frame->generation);
    memcpy(raw + FNR_TRANSPORT_HEADER_SIZE, frame->payload, frame->payload_length);
    put16(raw + covered, fnr_transport_crc16(raw, covered));
    size_t encoded_size = 0;
    const fnr_transport_result result = fnr_transport_cobs_encode(raw,
        covered + FNR_TRANSPORT_CRC_SIZE, out, capacity - 1u, &encoded_size);
    if (result != FNR_TRANSPORT_OK) return result;
    out[encoded_size] = 0;
    *written = encoded_size + 1u;
    return FNR_TRANSPORT_OK;
}

fnr_transport_result fnr_transport_decode(const uint8_t *encoded, size_t length,
    fnr_transport_frame *out) {
    if (!out || !encoded) return FNR_TRANSPORT_INVALID_ARGUMENT;
    if (length == 0 || length > FNR_TRANSPORT_ENCODED_CAPACITY)
        return FNR_TRANSPORT_INVALID_LENGTH;
    uint8_t raw[FNR_TRANSPORT_RAW_CAPACITY];
    size_t raw_size = 0;
    const fnr_transport_result decoded = fnr_transport_cobs_decode(encoded,
        length, raw, sizeof(raw), &raw_size);
    if (decoded == FNR_TRANSPORT_NO_SPACE) return FNR_TRANSPORT_INVALID_LENGTH;
    if (decoded != FNR_TRANSPORT_OK) return decoded;
    if (raw_size < FNR_TRANSPORT_HEADER_SIZE + FNR_TRANSPORT_CRC_SIZE)
        return FNR_TRANSPORT_INVALID_LENGTH;
    const uint16_t payload_length = get16(raw + 2);
    if (payload_length > FNR_TRANSPORT_PAYLOAD_CAPACITY ||
        raw_size != FNR_TRANSPORT_HEADER_SIZE + (size_t)payload_length +
            FNR_TRANSPORT_CRC_SIZE)
        return FNR_TRANSPORT_INVALID_LENGTH;
    if (raw[0] != FNR_TRANSPORT_VERSION) return FNR_TRANSPORT_UNSUPPORTED_VERSION;
    if (raw[1] != FNR_TRANSPORT_RADIO_DATAGRAM && raw[1] != FNR_TRANSPORT_RADIO_STATUS)
        return FNR_TRANSPORT_UNKNOWN_TYPE;
    const size_t covered = raw_size - FNR_TRANSPORT_CRC_SIZE;
    if (get16(raw + covered) != fnr_transport_crc16(raw, covered))
        return FNR_TRANSPORT_BAD_CRC;
    fnr_transport_frame frame = {0};
    frame.version = raw[0];
    frame.type = raw[1];
    frame.payload_length = payload_length;
    frame.sequence = get32(raw + 4);
    frame.generation = get64(raw + 8);
    memcpy(frame.payload, raw + FNR_TRANSPORT_HEADER_SIZE, payload_length);
    const fnr_transport_result valid = validate_frame(&frame);
    if (valid != FNR_TRANSPORT_OK) return valid;
    *out = frame;
    return FNR_TRANSPORT_OK;
}

fnr_transport_result fnr_transport_make_datagram(fnr_transport_frame *out,
    const fnr_datagram *datagram, uint32_t sequence, uint64_t generation) {
    if (!out || !datagram) return FNR_TRANSPORT_INVALID_ARGUMENT;
    if (datagram->size == 0 || datagram->size > FNR_DATAGRAM_CAPACITY)
        return FNR_TRANSPORT_INVALID_LENGTH;
    fnr_transport_frame frame = {0};
    frame.version = FNR_TRANSPORT_VERSION;
    frame.type = FNR_TRANSPORT_RADIO_DATAGRAM;
    frame.payload_length = (uint16_t)(FNR_MAC_SIZE + (size_t)datagram->size);
    frame.sequence = sequence;
    frame.generation = generation;
    memcpy(frame.payload, datagram->source_mac, FNR_MAC_SIZE);
    memcpy(frame.payload + FNR_MAC_SIZE, datagram->payload, datagram->size);
    *out = frame;
    return FNR_TRANSPORT_OK;
}

fnr_transport_result fnr_transport_unpack_datagram(const fnr_transport_frame *frame,
    fnr_datagram *out) {
    if (!out) return FNR_TRANSPORT_INVALID_ARGUMENT;
    const fnr_transport_result valid = validate_frame(frame);
    if (valid != FNR_TRANSPORT_OK) return valid;
    if (frame->type != FNR_TRANSPORT_RADIO_DATAGRAM) return FNR_TRANSPORT_UNKNOWN_TYPE;
    fnr_datagram datagram = {0};
    datagram.size = (uint16_t)(frame->payload_length - FNR_MAC_SIZE);
    memcpy(datagram.source_mac, frame->payload, FNR_MAC_SIZE);
    memcpy(datagram.payload, frame->payload + FNR_MAC_SIZE, datagram.size);
    *out = datagram;
    return FNR_TRANSPORT_OK;
}

fnr_transport_result fnr_transport_make_status(fnr_transport_frame *out,
    const fnr_transport_radio_status *status, uint32_t sequence, uint64_t generation) {
    if (!out || !status) return FNR_TRANSPORT_INVALID_ARGUMENT;
    if (!status_values_valid(status->flags, status->state, status->channel))
        return FNR_TRANSPORT_INVALID_PAYLOAD;
    fnr_transport_frame frame = {0};
    frame.version = FNR_TRANSPORT_VERSION;
    frame.type = FNR_TRANSPORT_RADIO_STATUS;
    frame.payload_length = FNR_TRANSPORT_STATUS_SIZE;
    frame.sequence = sequence;
    frame.generation = generation;
    frame.payload[0] = status->flags;
    frame.payload[1] = (uint8_t)status->state;
    frame.payload[2] = status->channel;
    put32(frame.payload + 4, status->radio.received);
    put32(frame.payload + 8, status->radio.accepted);
    put32(frame.payload + 12, status->radio.rejected_source);
    put32(frame.payload + 16, status->radio.invalid_length);
    put32(frame.payload + 20, status->radio.dropped);
    put32(frame.payload + 24, status->radio.consumed);
    put32(frame.payload + 28, status->radio.starts);
    put32(frame.payload + 32, status->radio.restarts);
    put32(frame.payload + 36, status->radio.errors);
    put32(frame.payload + 40, status->tx_replaced);
    put32(frame.payload + 44, status->tx_aborted);
    *out = frame;
    return FNR_TRANSPORT_OK;
}

fnr_transport_result fnr_transport_unpack_status(const fnr_transport_frame *frame,
    fnr_transport_radio_status *out) {
    if (!out) return FNR_TRANSPORT_INVALID_ARGUMENT;
    const fnr_transport_result valid = validate_frame(frame);
    if (valid != FNR_TRANSPORT_OK) return valid;
    if (frame->type != FNR_TRANSPORT_RADIO_STATUS) return FNR_TRANSPORT_UNKNOWN_TYPE;
    fnr_transport_radio_status status = {0};
    status.flags = frame->payload[0];
    status.state = (fnr_radio_state)frame->payload[1];
    status.channel = frame->payload[2];
    status.radio.received = get32(frame->payload + 4);
    status.radio.accepted = get32(frame->payload + 8);
    status.radio.rejected_source = get32(frame->payload + 12);
    status.radio.invalid_length = get32(frame->payload + 16);
    status.radio.dropped = get32(frame->payload + 20);
    status.radio.consumed = get32(frame->payload + 24);
    status.radio.starts = get32(frame->payload + 28);
    status.radio.restarts = get32(frame->payload + 32);
    status.radio.errors = get32(frame->payload + 36);
    status.tx_replaced = get32(frame->payload + 40);
    status.tx_aborted = get32(frame->payload + 44);
    *out = status;
    return FNR_TRANSPORT_OK;
}

uint32_t fnr_transport_sequence_next(uint32_t sequence) {
    return sequence + UINT32_C(1);
}

static void invalidate_pending(fnr_transport_rx *rx, uint32_t event) {
    if (rx->datagram_pending) bump(&rx->counters.datagrams_dropped);
    if (rx->status_pending) bump(&rx->counters.status_dropped);
    rx->datagram_pending = false;
    rx->status_pending = false;
    rx->events |= event;
}

void fnr_transport_rx_init(fnr_transport_rx *rx) {
    if (!rx) return;
    memset(rx, 0, sizeof(*rx));
    rx->discarding = true;
    rx->events = FNR_TRANSPORT_EVENT_RESET;
}

void fnr_transport_rx_reset(fnr_transport_rx *rx) {
    if (!rx) return;
    invalidate_pending(rx, FNR_TRANSPORT_EVENT_RESET);
    const fnr_transport_rx_counters counters = rx->counters;
    const uint32_t events = rx->events;
    memset(rx, 0, sizeof(*rx));
    rx->counters = counters;
    bump(&rx->counters.parser_resets);
    rx->events = events;
    rx->discarding = true;
}

static void discard_until_delimiter(fnr_transport_rx *rx, uint32_t event) {
    invalidate_pending(rx, event);
    rx->encoded_size = 0;
    rx->discarding = true;
    rx->have_sequence = false;
}

void fnr_transport_rx_discontinuity(fnr_transport_rx *rx) {
    if (!rx) return;
    bump(&rx->counters.link_discontinuities);
    discard_until_delimiter(rx, FNR_TRANSPORT_EVENT_LINK_ERROR);
}

void fnr_transport_rx_timeout(fnr_transport_rx *rx) {
    if (!rx) return;
    bump(&rx->counters.link_timeouts);
    discard_until_delimiter(rx, FNR_TRANSPORT_EVENT_TIMEOUT);
}

static void reject_segment(fnr_transport_rx *rx, fnr_transport_result result) {
    switch (result) {
    case FNR_TRANSPORT_MALFORMED_COBS: bump(&rx->counters.bad_cobs); break;
    case FNR_TRANSPORT_BAD_CRC: bump(&rx->counters.bad_crc); break;
    case FNR_TRANSPORT_UNSUPPORTED_VERSION: bump(&rx->counters.bad_version); break;
    case FNR_TRANSPORT_UNKNOWN_TYPE: bump(&rx->counters.bad_type); break;
    case FNR_TRANSPORT_INVALID_PAYLOAD: bump(&rx->counters.bad_payload); break;
    default: bump(&rx->counters.bad_length); break;
    }
    invalidate_pending(rx, FNR_TRANSPORT_EVENT_LINK_ERROR);
}

static void accept_frame(fnr_transport_rx *rx, const fnr_transport_frame *frame) {
    bump(&rx->counters.frames_valid);
    const bool generation_changed = rx->have_generation &&
        frame->generation != rx->generation;
    if (generation_changed) {
        bump(&rx->counters.generation_changes);
        invalidate_pending(rx, FNR_TRANSPORT_EVENT_GENERATION_CHANGE);
    }
    if (!generation_changed && rx->have_sequence &&
        frame->sequence != fnr_transport_sequence_next(rx->last_sequence)) {
        bump(&rx->counters.sequence_discontinuities);
        invalidate_pending(rx, FNR_TRANSPORT_EVENT_SEQUENCE_DISCONTINUITY);
    }
    rx->generation = frame->generation;
    rx->have_generation = true;
    rx->last_sequence = frame->sequence;
    rx->have_sequence = true;
    if (frame->type == FNR_TRANSPORT_RADIO_STATUS) {
        if ((frame->payload[0] & FNR_TRANSPORT_STATUS_RESET) != 0) {
            bump(&rx->counters.radio_resets);
            invalidate_pending(rx, FNR_TRANSPORT_EVENT_RADIO_RESET);
        }
        if (frame->payload[1] != FNR_RADIO_READY) {
            bump(&rx->counters.radio_unavailable);
            invalidate_pending(rx, FNR_TRANSPORT_EVENT_RADIO_UNAVAILABLE);
        }
        if (rx->status_pending) {
            bump(&rx->counters.status_replaced);
            bump(&rx->counters.status_dropped);
            rx->events |= FNR_TRANSPORT_EVENT_STATUS_REPLACED;
        }
        rx->status_slot = *frame;
        rx->status_pending = true;
        bump(&rx->counters.status_received);
    } else {
        if (rx->datagram_pending) {
            bump(&rx->counters.datagrams_replaced);
            bump(&rx->counters.datagrams_dropped);
            rx->events |= FNR_TRANSPORT_EVENT_DATAGRAM_REPLACED;
        }
        rx->datagram_slot = *frame;
        rx->datagram_pending = true;
        bump(&rx->counters.datagrams_received);
    }
}

size_t fnr_transport_rx_feed(fnr_transport_rx *rx, const uint8_t *bytes, size_t length) {
    if (!rx || (!bytes && length != 0)) return 0;
    size_t accepted = 0;
    for (size_t i = 0; i < length; ++i) {
        const uint8_t byte = bytes[i];
        bump(&rx->counters.bytes_received);
        if (rx->discarding) {
            if (byte == 0) {
                rx->discarding = false;
                rx->encoded_size = 0;
            } else {
                bump(&rx->counters.bytes_discarded);
            }
            continue;
        }
        if (byte == 0) {
            if (rx->encoded_size == 0) {
                bump(&rx->counters.empty_delimiters);
                continue;
            }
            fnr_transport_frame frame;
            const fnr_transport_result result = fnr_transport_decode(rx->encoded,
                rx->encoded_size, &frame);
            rx->encoded_size = 0;
            if (result == FNR_TRANSPORT_OK) {
                accept_frame(rx, &frame);
                ++accepted;
            } else {
                reject_segment(rx, result);
            }
        } else if (rx->encoded_size < sizeof(rx->encoded)) {
            rx->encoded[rx->encoded_size++] = byte;
        } else {
            bump(&rx->counters.oversized_frames);
            bump(&rx->counters.bytes_discarded);
            invalidate_pending(rx, FNR_TRANSPORT_EVENT_LINK_ERROR);
            rx->encoded_size = 0;
            rx->discarding = true;
        }
    }
    return accepted;
}

uint32_t fnr_transport_rx_take_events(fnr_transport_rx *rx) {
    if (!rx) return 0;
    const uint32_t events = rx->events;
    rx->events = 0;
    return events;
}

bool fnr_transport_rx_take_datagram(fnr_transport_rx *rx, fnr_transport_frame *out) {
    if (!rx || !out || rx->events != 0 || !rx->datagram_pending) return false;
    *out = rx->datagram_slot;
    rx->datagram_pending = false;
    bump(&rx->counters.datagrams_consumed);
    return true;
}

bool fnr_transport_rx_take_status(fnr_transport_rx *rx, fnr_transport_frame *out) {
    if (!rx || !out || rx->events != 0 || !rx->status_pending) return false;
    *out = rx->status_slot;
    rx->status_pending = false;
    bump(&rx->counters.status_consumed);
    return true;
}
