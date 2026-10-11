#include "rf_diag.h"
#include <cassert>
#include <cstdio>
int main() {
    uint8_t bytes[250]; uint32_t sequence;
    const unsigned sizes[] = {8u, 32u, 128u, 250u};
    for (unsigned n : sizes) {
        assert(rf_diag_make(bytes, n, 0x12345678));
        assert(rf_diag_check(bytes, n, 0, &sequence) && sequence == 0x12345678);
        for (unsigned i = 0; i < n; ++i) {
            for (unsigned bit = 0; bit < 8; ++bit) {
                bytes[i] ^= (uint8_t)(1u << bit);
                assert(!rf_diag_check(bytes, n, 0, &sequence));
                bytes[i] ^= (uint8_t)(1u << bit);
            }
        }
        assert(!rf_diag_check(bytes, n - 1, 0, &sequence));
    }
    assert(rf_diag_make(bytes, 1, 255));
    assert(rf_diag_check(bytes, 1, 255, &sequence) && sequence == 255);
    assert(rf_diag_make(bytes, 1, 256));
    assert(rf_diag_check(bytes, 1, 256, &sequence) && sequence == 256);
    assert(rf_diag_make(bytes, 1, 260));
    assert(rf_diag_check(bytes, 1, 258, &sequence) && sequence == 260);
    assert(!rf_diag_make(bytes, 251, 0));
    assert(!rf_diag_make(bytes, 2, 0));
    assert(!rf_diag_check(nullptr, 250, 0, &sequence));
    std::puts("PASS: RF payload bounds, sequence and single-bit corruption rejection");
}
