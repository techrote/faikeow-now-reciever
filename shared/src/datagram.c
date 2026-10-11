#include "fnr/platform.h"
fnr_copy_result fnr_datagram_copy(fnr_datagram *out, const uint8_t *mac,
                                 const uint8_t *bytes, size_t size) {
    if (out == NULL || mac == NULL || bytes == NULL || size == 0 ||
        size > FNR_DATAGRAM_CAPACITY) return FNR_COPY_INVALID;
    for (size_t i = 0; i < FNR_MAC_SIZE; ++i) out->source_mac[i] = mac[i];
    for (size_t i = 0; i < FNR_DATAGRAM_CAPACITY; ++i)
        out->payload[i] = i < size ? bytes[i] : 0;
    out->size = (uint16_t)size; /* Narrow ONLY after validation. */
    return FNR_COPY_OK;
}
