#include "fnr/envelope.h"

#include <string.h>

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8U));
}

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) |
           ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static void write_u16_le(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value & UINT16_C(0xff));
    p[1] = (uint8_t)(value >> 8U);
}

static void write_u32_le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value & UINT32_C(0xff));
    p[1] = (uint8_t)((value >> 8U) & UINT32_C(0xff));
    p[2] = (uint8_t)((value >> 16U) & UINT32_C(0xff));
    p[3] = (uint8_t)(value >> 24U);
}

static fnr_envelope_result validate_header(const fnr_envelope *envelope) {
    if (envelope->version != FNR_PLATFORM_VERSION) return FNR_ENVELOPE_UNSUPPORTED_VERSION;
    if (envelope->profile_id == 0U) return FNR_ENVELOPE_INVALID_PROFILE;
    if (envelope->message_type != FNR_PLATFORM_MESSAGE_PROFILE_DATA)
        return FNR_ENVELOPE_UNKNOWN_MESSAGE;
    if (envelope->flags != 0U) return FNR_ENVELOPE_INVALID_FLAGS;
    if (envelope->session == 0U) return FNR_ENVELOPE_INVALID_SESSION;
    if (envelope->payload_length > FNR_PLATFORM_PAYLOAD_CAPACITY)
        return FNR_ENVELOPE_INVALID_LENGTH;
    return FNR_ENVELOPE_OK;
}

fnr_envelope_result fnr_envelope_encode(const fnr_envelope *envelope,
                                         uint8_t *out, size_t capacity,
                                         size_t *written) {
    if (written != NULL) *written = 0U;
    if (envelope == NULL || out == NULL || written == NULL)
        return FNR_ENVELOPE_INVALID_ARGUMENT;
    const fnr_envelope_result valid = validate_header(envelope);
    if (valid != FNR_ENVELOPE_OK) return valid;
    const size_t total = FNR_PLATFORM_ENVELOPE_HEADER_SIZE + envelope->payload_length;
    if (capacity < total) return FNR_ENVELOPE_INVALID_LENGTH;
    out[0] = envelope->version;
    out[1] = envelope->profile_id;
    out[2] = envelope->message_type;
    out[3] = envelope->flags;
    write_u32_le(out + 4U, envelope->session);
    write_u32_le(out + 8U, envelope->sequence);
    write_u16_le(out + 12U, envelope->payload_length);
    if (envelope->payload_length != 0U)
        memcpy(out + FNR_PLATFORM_ENVELOPE_HEADER_SIZE, envelope->payload,
               envelope->payload_length);
    *written = total;
    return FNR_ENVELOPE_OK;
}

fnr_envelope_result fnr_envelope_decode(const uint8_t *bytes, size_t length,
                                         fnr_envelope *out) {
    if (bytes == NULL || out == NULL) return FNR_ENVELOPE_INVALID_ARGUMENT;
    if (length < FNR_PLATFORM_ENVELOPE_HEADER_SIZE || length > FNR_DATAGRAM_CAPACITY)
        return FNR_ENVELOPE_INVALID_LENGTH;
    fnr_envelope candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.version = bytes[0];
    candidate.profile_id = bytes[1];
    candidate.message_type = bytes[2];
    candidate.flags = bytes[3];
    candidate.session = read_u32_le(bytes + 4U);
    candidate.sequence = read_u32_le(bytes + 8U);
    candidate.payload_length = read_u16_le(bytes + 12U);
    const fnr_envelope_result valid = validate_header(&candidate);
    if (valid != FNR_ENVELOPE_OK) return valid;
    if ((size_t)candidate.payload_length != length - FNR_PLATFORM_ENVELOPE_HEADER_SIZE)
        return FNR_ENVELOPE_INVALID_LENGTH;
    if (candidate.payload_length != 0U)
        memcpy(candidate.payload, bytes + FNR_PLATFORM_ENVELOPE_HEADER_SIZE,
               candidate.payload_length);
    *out = candidate;
    return FNR_ENVELOPE_OK;
}

fnr_sequence_relation fnr_sequence_compare(uint32_t previous, uint32_t candidate) {
    const uint32_t delta = candidate - previous;
    if (delta == 0U) return FNR_SEQUENCE_DUPLICATE;
    if (delta == UINT32_C(0x80000000)) return FNR_SEQUENCE_AMBIGUOUS;
    return delta < UINT32_C(0x80000000) ? FNR_SEQUENCE_NEWER : FNR_SEQUENCE_STALE;
}
