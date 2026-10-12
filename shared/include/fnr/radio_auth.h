#ifndef FNR_RADIO_AUTH_H
#define FNR_RADIO_AUTH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Radio-only wrapper. The enclosed platform packet and FNR-005 are unchanged.
 * No secrets are stored here. The platform supplies a keyed HMAC-SHA-256. */
enum {
    FNR_AUTH_HEADER_SIZE = 6, FNR_AUTH_TAG_SIZE = 16,
    FNR_AUTH_OVERHEAD = 22, FNR_AUTH_INNER_MAX = 228,
    FNR_AUTH_DIGEST_SIZE = 32, FNR_AUTH_MESSAGE_MAX = 275
};
typedef struct fnr_auth_context {
    uint64_t config_id;
    uint32_t generation;
    uint8_t sender[6];
    uint8_t receiver[6];
} fnr_auth_context;
typedef bool (*fnr_auth_hmac)(void *key_context, const uint8_t *message,
                              size_t length, uint8_t digest[32]);
/* Output arguments are cleared on failure. No allocation, no secret logging.
 * Provider failure is fail-closed. Tag comparison examines all 16 bytes. */
bool fnr_auth_wrap(const fnr_auth_context *context, fnr_auth_hmac hmac,
                   void *key_context, const uint8_t *inner, size_t inner_size,
                   uint8_t *wire, size_t capacity, size_t *wire_size);
bool fnr_auth_unwrap(const fnr_auth_context *context, fnr_auth_hmac hmac,
                     void *key_context, const uint8_t *wire, size_t wire_size,
                     const uint8_t **inner, size_t *inner_size);
#ifdef __cplusplus
}
#endif
#endif
