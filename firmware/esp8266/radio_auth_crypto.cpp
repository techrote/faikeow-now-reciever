#include "radio_auth_crypto.h"
#include "fnr/radio_auth.h"
#include <bearssl/bearssl.h>
#include <cstring>

static void wipe(uint8_t *bytes, size_t size) {
    volatile uint8_t *p = bytes;
    for (size_t i = 0; i < size; ++i) p[i] = 0;
}
static void hmac(const uint8_t *key, size_t key_size,
                 const uint8_t *message, size_t size, uint8_t out[32]) {
    br_hmac_key_context kc;
    br_hmac_context hc;
    br_hmac_key_init(&kc, &br_sha256_vtable, key, key_size);
    br_hmac_init(&hc, &kc, 0);
    br_hmac_update(&hc, message, size);
    br_hmac_out(&hc, out);
    wipe(reinterpret_cast<uint8_t *>(&kc), sizeof(kc));
    wipe(reinterpret_cast<uint8_t *>(&hc), sizeof(hc));
}
bool fnr_auth_crypto_selftest(void) {
    /* RFC 4231 test case 1; actual pinned target crypto is tested before READY. */
    uint8_t key[20]; std::memset(key, 0x0b, sizeof(key));
    const uint8_t message[] = "Hi There";
    const uint8_t expected[32] = {
        0xb0,0x34,0x4c,0x61,0xd8,0xdb,0x38,0x53,
        0x5c,0xa8,0xaf,0xce,0xaf,0x0b,0xf1,0x2b,
        0x88,0x1d,0xc2,0x00,0xc9,0x83,0x3d,0xa7,
        0x26,0xe9,0x37,0x6c,0x2e,0x32,0xcf,0xf7};
    uint8_t out[32];
    hmac(key, sizeof(key), message, sizeof(message) - 1U, out);
    if (std::memcmp(out, expected, sizeof(out)) != 0) return false;
    /* Independent Python hashlib/hmac wrapper vector. Public test values,
     * unrelated to any deployment key. Checks KDF and serialized binding. */
    uint8_t peer_key[16];
    for (size_t i = 0; i < sizeof(peer_key); ++i)
        peer_key[i] = static_cast<uint8_t>(i + 1U);
    fnr_auth_context binding = {
        UINT64_C(0x123456789abcdef0), 1,
        {2,3,4,5,6,7}, {10,11,12,13,14,15}};
    const uint8_t body[3] = {1,2,3};
    const uint8_t expected_tag[16] = {
        0x0e,0xe8,0x09,0x1a,0x2e,0x32,0x99,0x19,
        0x4c,0xee,0x54,0x59,0xc9,0xc8,0xb0,0x18};
    uint8_t wire[25]; size_t wire_size = 0;
    return fnr_auth_wrap(&binding, fnr_auth_crypto_hmac, peer_key, body,
                         sizeof(body), wire, sizeof(wire), &wire_size) &&
           wire_size == sizeof(wire) &&
           std::memcmp(wire + 9, expected_tag, sizeof(expected_tag)) == 0;
}
bool fnr_auth_crypto_hmac(void *peer_key, const uint8_t *message,
                          size_t size, uint8_t out[32]) {
    if (!peer_key || !message || !out) return false;
    const uint8_t domain[] = "FNR-radio-auth-key-v1";
    uint8_t derived[32];
    hmac(static_cast<const uint8_t *>(peer_key), 16, domain,
         sizeof(domain) - 1U, derived);
    hmac(derived, sizeof(derived), message, size, out);
    wipe(derived, sizeof(derived));
    return true;
}
