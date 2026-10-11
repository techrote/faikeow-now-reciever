/* Test fixture label, not a hardware identity. Host guards still require the
 * independently mapped USB topology and MCU identity. No flash/GPIO access.
 * --wrap keeps the pinned SDK unchanged and avoids its shared SRAM "EE" ID.
 */
#include <stddef.h>
#include <string.h>
#ifndef FNR_DIAG_BOARD_LABEL
#error Explicit diagnostic fixture label required
#endif
void __wrap_pico_get_unique_board_id_string(char *out, unsigned length) {
    const char label[] = "FNR002" FNR_DIAG_BOARD_LABEL;
    if (!length) return;
    size_t n = sizeof(label) - 1;
    if (n >= length) n = length - 1;
    memcpy(out, label, n);
    out[n] = 0;
}
