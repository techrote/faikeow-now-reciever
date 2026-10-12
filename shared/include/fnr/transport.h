#ifndef FNR_TRANSPORT_H
#define FNR_TRANSPORT_H

#include "fnr/radio_ingress.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Board-local wire v1. All multibyte integers, including the CRC, are little
 * endian. CRC-16/CCITT-FALSE: poly 0x1021, init 0xffff, no reflection/xorout.
 * CRC covers the complete decoded header and payload, excluding the CRC itself.
 * Header offsets: version:u8@0, type:u8@1, length:u16@2, sequence:u32@4,
 * generation:u64@8. Generation identifies the LOCAL radio transport lifecycle;
 * it is neither a sender session nor authentication. C structures are not ABI.
 */
enum {
    FNR_TRANSPORT_VERSION = 1,
    FNR_TRANSPORT_HEADER_SIZE = 16,
    FNR_TRANSPORT_CRC_SIZE = 2,
    FNR_TRANSPORT_STATUS_SIZE = 48,
    FNR_TRANSPORT_PAYLOAD_CAPACITY = FNR_MAC_SIZE + FNR_DATAGRAM_CAPACITY,
    FNR_TRANSPORT_RAW_CAPACITY = FNR_TRANSPORT_HEADER_SIZE +
        FNR_TRANSPORT_PAYLOAD_CAPACITY + FNR_TRANSPORT_CRC_SIZE,
    /* Canonical COBS bound for n bytes: n + floor(n / 254) + 1. */
    FNR_TRANSPORT_ENCODED_CAPACITY = FNR_TRANSPORT_RAW_CAPACITY +
        FNR_TRANSPORT_RAW_CAPACITY / 254 + 1,
    FNR_TRANSPORT_WIRE_CAPACITY = FNR_TRANSPORT_ENCODED_CAPACITY + 1
};

typedef enum fnr_transport_type {
    FNR_TRANSPORT_RADIO_DATAGRAM = 1,
    FNR_TRANSPORT_RADIO_STATUS = 2
} fnr_transport_type;

typedef enum fnr_transport_result {
    FNR_TRANSPORT_OK = 0,
    FNR_TRANSPORT_INVALID_ARGUMENT,
    FNR_TRANSPORT_NO_SPACE,
    FNR_TRANSPORT_MALFORMED_COBS,
    FNR_TRANSPORT_INVALID_LENGTH,
    FNR_TRANSPORT_UNSUPPORTED_VERSION,
    FNR_TRANSPORT_UNKNOWN_TYPE,
    FNR_TRANSPORT_BAD_CRC,
    FNR_TRANSPORT_INVALID_PAYLOAD
} fnr_transport_result;

typedef struct fnr_transport_frame {
    uint8_t version;
    uint8_t type;
    uint16_t payload_length;
    uint32_t sequence;
    uint64_t generation;
    uint8_t payload[FNR_TRANSPORT_PAYLOAD_CAPACITY];
} fnr_transport_frame;

enum { FNR_TRANSPORT_STATUS_RESET = 1u };
/* Status payload: flags@0, radio state@1, channel@2, reserved-zero@3;
 * nine radio counters in fnr_radio_counters declaration order at 4..39;
 * tx_replaced:u32@40, tx_aborted:u32@44. READY requires channel 1..14;
 * other states permit 0..14. Only the RESET flag is defined. */
typedef struct fnr_transport_radio_status {
    uint8_t flags;
    fnr_radio_state state;
    uint8_t channel;
    fnr_radio_counters radio;
    uint32_t tx_replaced;
    uint32_t tx_aborted;
} fnr_transport_radio_status;

/* All buffers must be valid for their lengths and must not overlap. Null input
 * is permitted only for zero length. A failing encoder/COBS utility sets
 * *written=0 and may modify its byte output; decoder frame outputs and the
 * make/unpack helpers remain unchanged on failure. No utility allocates.
 * cobs_bound returns 0 if n's worst-case expansion would overflow size_t.
 */
size_t fnr_transport_cobs_bound(size_t length);
uint16_t fnr_transport_crc16(const uint8_t *bytes, size_t length);
fnr_transport_result fnr_transport_cobs_encode(const uint8_t *bytes, size_t length,
    uint8_t *out, size_t capacity, size_t *written);
fnr_transport_result fnr_transport_cobs_decode(const uint8_t *bytes, size_t length,
    uint8_t *out, size_t capacity, size_t *written);
/* encode writes one COBS segment AND its terminating zero. decode accepts one
 * complete COBS segment WITHOUT its delimiter and publishes only on validation.
 * Canonical encoding retains the terminal empty block after 254 nonzero bytes;
 * decoding also accepts the valid shorter representation of that block.
 */
fnr_transport_result fnr_transport_encode(const fnr_transport_frame *frame,
    uint8_t *out, size_t capacity, size_t *written);
fnr_transport_result fnr_transport_decode(const uint8_t *encoded, size_t length,
    fnr_transport_frame *out);
fnr_transport_result fnr_transport_make_datagram(fnr_transport_frame *out,
    const fnr_datagram *datagram, uint32_t sequence, uint64_t generation);
fnr_transport_result fnr_transport_unpack_datagram(const fnr_transport_frame *frame,
    fnr_datagram *out);
fnr_transport_result fnr_transport_make_status(fnr_transport_frame *out,
    const fnr_transport_radio_status *status, uint32_t sequence, uint64_t generation);
fnr_transport_result fnr_transport_unpack_status(const fnr_transport_frame *frame,
    fnr_transport_radio_status *out);
uint32_t fnr_transport_sequence_next(uint32_t sequence);

enum {
    FNR_TRANSPORT_EVENT_RESET = 1u << 0,
    FNR_TRANSPORT_EVENT_LINK_ERROR = 1u << 1,
    FNR_TRANSPORT_EVENT_GENERATION_CHANGE = 1u << 2,
    FNR_TRANSPORT_EVENT_SEQUENCE_DISCONTINUITY = 1u << 3,
    FNR_TRANSPORT_EVENT_RADIO_RESET = 1u << 4,
    FNR_TRANSPORT_EVENT_DATAGRAM_REPLACED = 1u << 5,
    FNR_TRANSPORT_EVENT_STATUS_REPLACED = 1u << 6,
    FNR_TRANSPORT_EVENT_RADIO_UNAVAILABLE = 1u << 7,
    FNR_TRANSPORT_EVENT_TIMEOUT = 1u << 8
};

typedef struct fnr_transport_rx_counters {
    uint32_t bytes_received, bytes_discarded, frames_valid;
    uint32_t datagrams_received, status_received;
    uint32_t datagrams_consumed, status_consumed;
    uint32_t datagrams_replaced, status_replaced;
    uint32_t datagrams_dropped, status_dropped;
    uint32_t bad_cobs, bad_crc, bad_length, bad_version, bad_type, bad_payload;
    uint32_t oversized_frames, empty_delimiters;
    uint32_t parser_resets, link_discontinuities, link_timeouts;
    uint32_t generation_changes, sequence_discontinuities;
    uint32_t radio_resets, radio_unavailable;
} fnr_transport_rx_counters;

/* All calls on a receiver must be serialized, including event/take calls.
 * There is NO implicit IRQ safety. Each frame slot owns all of its payload.
 * One newest-wins slot per frame type; ordinary replacement retains the other
 * type's slot and increments both that type's replaced and total-dropped counts.
 * Counters saturate at UINT32_MAX. Rejection/link loss, lifecycle change,
 * sequence discontinuity and non-READY status
 * invalidate older pending slots. Sequence mismatch is diagnostic: the newly
 * validated frame becomes the next baseline, including duplicates/backward
 * values. This layer does not implement sender order/freshness acceptance.
 *
 * Events are sticky and are a mandatory barrier: both takes return false until
 * take_events has reported/cleared the accumulated bits. The future core must
 * process that event snapshot before using a subsequent frame. Events survive
 * status-slot replacement. Take outputs must be caller-owned storage outside
 * the receiver. No consumer callback is invoked by feed.
 */
typedef struct fnr_transport_rx {
    uint8_t encoded[FNR_TRANSPORT_ENCODED_CAPACITY];
    size_t encoded_size;
    bool discarding;
    bool have_generation;
    bool have_sequence;
    uint64_t generation;
    uint32_t last_sequence;
    uint32_t events;
    fnr_transport_frame datagram_slot;
    fnr_transport_frame status_slot;
    bool datagram_pending;
    bool status_pending;
    fnr_transport_rx_counters counters;
} fnr_transport_rx;

/* init accepts uninitialized storage, clears counters and raises RESET. Both
 * init/reset discard through the NEXT delimiter; a leading zero is required
 * to preserve the first frame after startup. reset requires initialized storage,
 * retains counters, drops pending frames and forgets generation/sequence.
 * discontinuity handles UART error/overflow/interruption: drops pending/current
 * data through the next delimiter, forgets sequence, retains known generation.
 * timeout performs the same invalidation and raises a distinct TIMEOUT event;
 * the caller owns its clock/timeout policy, outside the codec.
 * Feed accepts arbitrary chunks and null only when length is zero; invalid
 * input is ignored. A complete corrupt segment is rejected at its delimiter,
 * which immediately resynchronizes the parser for the next segment. Feed
 * returns the count of valid frames in this call, independent of saturating
 * counters; target link-liveness logic may use it without consuming slots.
 */
void fnr_transport_rx_init(fnr_transport_rx *rx);
void fnr_transport_rx_reset(fnr_transport_rx *rx);
void fnr_transport_rx_discontinuity(fnr_transport_rx *rx);
void fnr_transport_rx_timeout(fnr_transport_rx *rx);
size_t fnr_transport_rx_feed(fnr_transport_rx *rx, const uint8_t *bytes, size_t length);
uint32_t fnr_transport_rx_take_events(fnr_transport_rx *rx);
bool fnr_transport_rx_take_datagram(fnr_transport_rx *rx, fnr_transport_frame *out);
bool fnr_transport_rx_take_status(fnr_transport_rx *rx, fnr_transport_frame *out);

#ifdef __cplusplus
}
#endif
#endif
