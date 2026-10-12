#ifndef FNR_RADIO_AUTH_CRYPTO_H
#define FNR_RADIO_AUTH_CRYPTO_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
/* Uses BearSSL from the pinned Arduino core; never exposes a key in state.
 * HMAC key = HMAC-SHA-256(peer key, ASCII "FNR-radio-auth-key-v1"). */
bool fnr_auth_crypto_selftest(void);
bool fnr_auth_crypto_hmac(void *peer_key, const uint8_t *message,
                          size_t length, uint8_t digest[32]);
#endif
