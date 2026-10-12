#ifndef FNR_ENVELOPE_H
#define FNR_ENVELOPE_H

#include "fnr/platform.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FNR_PLATFORM_VERSION = 1,
    FNR_PLATFORM_MESSAGE_PROFILE_DATA = 1,
    FNR_PLATFORM_ENVELOPE_HEADER_SIZE = 18,
    FNR_PLATFORM_PAYLOAD_CAPACITY = FNR_DATAGRAM_CAPACITY - FNR_PLATFORM_ENVELOPE_HEADER_SIZE
};

typedef enum fnr_envelope_result {
    FNR_ENVELOPE_OK = 0,
    FNR_ENVELOPE_INVALID_ARGUMENT,
    FNR_ENVELOPE_INVALID_LENGTH,
    FNR_ENVELOPE_UNSUPPORTED_VERSION,
    FNR_ENVELOPE_INVALID_PROFILE,
    FNR_ENVELOPE_UNKNOWN_MESSAGE,
    FNR_ENVELOPE_INVALID_FLAGS,
    FNR_ENVELOPE_INVALID_SESSION
} fnr_envelope_result;

typedef struct fnr_envelope {
    uint8_t version;
    uint8_t profile_id;
    uint8_t message_type;
    uint8_t flags;
    uint64_t session;
    uint32_t sequence;
    uint16_t payload_length;
    uint8_t payload[FNR_PLATFORM_PAYLOAD_CAPACITY];
} fnr_envelope;

typedef enum fnr_sequence_relation {
    FNR_SEQUENCE_NEWER = 0,
    FNR_SEQUENCE_DUPLICATE,
    FNR_SEQUENCE_STALE,
    FNR_SEQUENCE_AMBIGUOUS
} fnr_sequence_relation;

fnr_envelope_result fnr_envelope_encode(const fnr_envelope *envelope,
                                         uint8_t *out, size_t capacity,
                                         size_t *written);
fnr_envelope_result fnr_envelope_decode(const uint8_t *bytes, size_t length,
                                         fnr_envelope *out);
fnr_sequence_relation fnr_sequence_compare(uint32_t previous, uint32_t candidate);

#ifdef __cplusplus
}
#endif
#endif
