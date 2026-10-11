#include "tusb.h"
#include <stddef.h>
_Static_assert(TUSB_VERSION_MAJOR==0 && TUSB_VERSION_MINOR==18 &&
               TUSB_VERSION_REVISION==0, "Requires SDK-pinned TinyUSB 0.18.0");
/* Keep a real device-stack symbol link-visible without starting USB. */
bool (* volatile fnr_usb_api_anchor)(void) = tud_mounted;
uint8_t const *tud_descriptor_device_cb(void) { return NULL; }
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) { (void)index; return NULL; }
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)index; (void)langid; return NULL;
}
/* Placeholder callbacks are NOT descriptors. main never initializes TinyUSB. */
