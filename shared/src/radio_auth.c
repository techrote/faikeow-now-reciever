#include "fnr/radio_auth.h"
#include <string.h>

static bool context_valid(const fnr_auth_context *c) {
    if (!c || !c->config_id || !c->generation) return false;
    uint8_t sender = 0, receiver = 0;
    for (size_t i = 0; i < 6; ++i) {
        sender |= c->sender[i]; receiver |= c->receiver[i];
    }
    return sender && receiver && !(c->sender[0] & 1U) &&
           !(c->receiver[0] & 1U);
}

static bool digest(const fnr_auth_context *c, fnr_auth_hmac hmac, void *key,
                   const uint8_t *packet, size_t size, uint8_t out[32]) {
    /* Domain (17), LE config ID (8), LE generation (4), sender (6),
     * receiver (6), then the complete wrapper header and enclosed packet. */
    uint8_t message[FNR_AUTH_MESSAGE_MAX];
    static const uint8_t domain[17] = "FNR-radio-auth-v1";
    if (!context_valid(c) || !hmac || size > 234U) return false;
    memcpy(message, domain, sizeof(domain));
    for (size_t i = 0; i < 8; ++i)
        message[17 + i] = (uint8_t)(c->config_id >> (8U * i));
    for (size_t i = 0; i < 4; ++i)
        message[25 + i] = (uint8_t)(c->generation >> (8U * i));
    memcpy(message + 29, c->sender, 6);
    memcpy(message + 35, c->receiver, 6);
    memcpy(message + 41, packet, size);
    return hmac(key, message, 41U + size, out);
}

bool fnr_auth_wrap(const fnr_auth_context *c, fnr_auth_hmac hmac, void *key,
                   const uint8_t *inner, size_t size, uint8_t *wire,
                   size_t capacity, size_t *wire_size) {
    if (wire_size) *wire_size = 0;
    if (!wire_size || !inner || !wire || !size || size > FNR_AUTH_INNER_MAX ||
        capacity < size + FNR_AUTH_OVERHEAD) return false;
    /* Input/output must not overlap. Caller owns both bounded buffers. */
    memcpy(wire, "FNA1", 4);
    wire[4] = (uint8_t)size; wire[5] = (uint8_t)(size >> 8);
    memcpy(wire + FNR_AUTH_HEADER_SIZE, inner, size);
    uint8_t tag[32];
    if (!digest(c, hmac, key, wire, size + FNR_AUTH_HEADER_SIZE, tag))
        return false;
    memcpy(wire + FNR_AUTH_HEADER_SIZE + size, tag, FNR_AUTH_TAG_SIZE);
    *wire_size = size + FNR_AUTH_OVERHEAD;
    return true;
}

bool fnr_auth_unwrap(const fnr_auth_context *c, fnr_auth_hmac hmac, void *key,
                     const uint8_t *wire, size_t size,
                     const uint8_t **inner, size_t *inner_size) {
    if (inner) *inner = NULL;
    if (inner_size) *inner_size = 0;
    if (!inner || !inner_size || !wire || size <= FNR_AUTH_OVERHEAD ||
        size > 250U || memcmp(wire, "FNA1", 4)) return false;
    const size_t length = (size_t)wire[4] | ((size_t)wire[5] << 8);
    if (!length || length > FNR_AUTH_INNER_MAX ||
        size != length + FNR_AUTH_OVERHEAD) return false;
    uint8_t tag[32];
    if (!digest(c, hmac, key, wire, length + FNR_AUTH_HEADER_SIZE, tag))
        return false;
    uint8_t difference = 0;
    for (size_t i = 0; i < FNR_AUTH_TAG_SIZE; ++i)
        difference |= (uint8_t)(tag[i] ^ wire[FNR_AUTH_HEADER_SIZE + length + i]);
    if (difference) return false;
    *inner = wire + FNR_AUTH_HEADER_SIZE;
    *inner_size = length;
    return true;
}
