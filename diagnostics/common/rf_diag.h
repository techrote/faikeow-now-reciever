/* Isolated RF test payload, not the platform envelope or production framing. */
#ifndef FNR_RF_DIAG_H
#define FNR_RF_DIAG_H
#include "link_diag.h"
static inline bool rf_diag_make(uint8_t *data, unsigned n, uint32_t seq) {
    if (!data || (n != 1 && (n < 8 || n > 250))) return false;
    if (n == 1) { data[0] = (uint8_t)seq; return true; }
    diag_put32(data, seq);
    diag_payload(data + 4, n - 8, seq, true);
    diag_put32(data + n - 4, diag_crc(data, n - 4));
    return true;
}
static inline bool rf_diag_check(const uint8_t *data, unsigned n,
                                 uint32_t expected, uint32_t *seq) {
    if (!data || !seq || (n != 1 && (n < 8 || n > 250))) return false;
    if (n == 1) {
        *seq = (expected & ~255u) | data[0];
        if (*seq < expected) *seq += 256u;
        return true; /* One-byte case has no room for an application CRC. */
    }
    *seq = diag_get32(data);
    uint8_t pattern[242];
    diag_payload(pattern, n - 8, *seq, true);
    return diag_get32(data + n - 4) == diag_crc(data, n - 4)
        && memcmp(data + 4, pattern, n - 8) == 0;
}
#endif
