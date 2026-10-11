#include "pico/stdlib.h"

#include "fnr/hid.h"
#include "fnr/hid_relative_mouse.h"
#include "usb_adapter.h"

static fnr_hid_device fnr_usb_device;
static fnr_relative_mouse_state fnr_mouse_state;
static fnr_hid_profile fnr_mouse_profile;

static bool fnr_usb_profile_init(void) {
    fnr_hid_device_init(&fnr_usb_device);
    fnr_relative_mouse_init(&fnr_mouse_state);
    fnr_relative_mouse_make_profile(&fnr_mouse_state, &fnr_mouse_profile);

    if (fnr_hid_device_register_profile(&fnr_usb_device, &fnr_mouse_profile) !=
        FNR_HID_STATUS_OK) {
        return false;
    }
    if (fnr_hid_device_select_profile(&fnr_usb_device, "relative_mouse") !=
        FNR_HID_STATUS_OK) {
        return false;
    }
    return fnr_usb_adapter_init(&fnr_usb_device);
}

int main(void) {
    if (!fnr_usb_profile_init()) {
        for (;;) {
            tight_loop_contents();
        }
    }

    /* Production firmware services USB only. FNR-003 deliberately has no
     * synthetic mouse activity; FNR-006 will feed authorized logical input. */
    for (;;) {
        fnr_usb_adapter_task();
        tight_loop_contents();
    }
}
