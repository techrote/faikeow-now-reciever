#include "usb_adapter.h"

#include <stddef.h>
#include <string.h>

#include "device/dcd.h"
#include "tusb.h"

_Static_assert(TUSB_VERSION_MAJOR == 0 && TUSB_VERSION_MINOR == 18 &&
                   TUSB_VERSION_REVISION == 0,
               "Requires SDK-pinned TinyUSB 0.18.0");

static fnr_hid_device *fnr_usb_device;
static volatile bool fnr_bus_reset_pending;
static volatile bool fnr_transfer_failure_pending;
static uint16_t fnr_string_descriptor[64];

static bool fnr_tinyusb_ready(void *context, uint8_t instance) {
    (void)context;
    return tud_hid_n_ready(instance);
}

static bool fnr_tinyusb_send(void *context, uint8_t instance,
                             uint8_t report_id, const uint8_t *data,
                             uint16_t size) {
    (void)context;
    return tud_hid_n_report(instance, report_id, data, size);
}

static const fnr_hid_descriptor_set *fnr_active_descriptors(void) {
    return fnr_hid_device_descriptors(fnr_usb_device);
}

bool fnr_usb_adapter_init(fnr_hid_device *device) {
    if (device == NULL || fnr_hid_device_selected_profile(device) == NULL ||
        fnr_hid_device_descriptors(device) == NULL) {
        return false;
    }
    fnr_usb_device = device;
    fnr_bus_reset_pending = false;
    fnr_transfer_failure_pending = false;
    if (!tud_init(0U)) {
        fnr_usb_device = NULL;
        return false;
    }
    return true;
}

void fnr_usb_adapter_task(void) {
    if (fnr_usb_device == NULL) {
        return;
    }

    tud_task_ext(0U, false);

    /* The TinyUSB event hook may run in interrupt context. It records only a
     * flag; all portable state transitions occur here after tud_task(). */
    if (fnr_bus_reset_pending) {
        fnr_bus_reset_pending = false;
        (void)fnr_hid_device_usb_event(fnr_usb_device, FNR_HID_USB_RESET);
    }
    if (fnr_transfer_failure_pending) {
        fnr_transfer_failure_pending = false;
        (void)fnr_hid_device_note_transport_error(fnr_usb_device);
    }

    (void)fnr_hid_device_service(fnr_usb_device, fnr_tinyusb_ready,
                                 fnr_tinyusb_send, NULL);
}

uint8_t const *tud_descriptor_device_cb(void) {
    const fnr_hid_descriptor_set *descriptors = fnr_active_descriptors();
    return descriptors == NULL ? NULL : descriptors->device;
}

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    const fnr_hid_descriptor_set *descriptors = fnr_active_descriptors();
    if (descriptors == NULL || index != 0U) {
        return NULL;
    }
    return descriptors->configuration;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    const fnr_hid_descriptor_set *descriptors = fnr_active_descriptors();
    if (descriptors == NULL || instance >= descriptors->report_count) {
        return NULL;
    }
    return descriptors->reports[instance].bytes;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    const fnr_hid_descriptor_set *descriptors = fnr_active_descriptors();
    (void)langid;
    if (descriptors == NULL) {
        return NULL;
    }

    size_t count = 0U;
    if (index == 0U) {
        fnr_string_descriptor[1] = 0x0409U; /* English (United States). */
        count = 1U;
    } else {
        const uint8_t string_index = (uint8_t)(index - 1U);
        if (string_index >= descriptors->string_count ||
            descriptors->strings[string_index] == NULL) {
            return NULL;
        }
        const char *string = descriptors->strings[string_index];
        const size_t capacity = (sizeof(fnr_string_descriptor) /
                                 sizeof(fnr_string_descriptor[0])) - 1U;
        while (string[count] != '\0' && count < capacity) {
            fnr_string_descriptor[1U + count] = (uint8_t)string[count];
            ++count;
        }
    }

    fnr_string_descriptor[0] =
        (uint16_t)((TUSB_DESC_STRING << 8U) | (uint16_t)(2U * count + 2U));
    return fnr_string_descriptor;
}

void tud_mount_cb(void) {
    fnr_bus_reset_pending = false;
    fnr_transfer_failure_pending = false;
    if (fnr_usb_device != NULL) {
        (void)fnr_hid_device_usb_event(fnr_usb_device, FNR_HID_USB_MOUNT);
    }
}

void tud_umount_cb(void) {
    fnr_bus_reset_pending = false;
    fnr_transfer_failure_pending = false;
    if (fnr_usb_device != NULL) {
        (void)fnr_hid_device_usb_event(fnr_usb_device, FNR_HID_USB_UNMOUNT);
    }
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    if (fnr_usb_device != NULL) {
        (void)fnr_hid_device_usb_event(fnr_usb_device, FNR_HID_USB_SUSPEND);
    }
}

void tud_resume_cb(void) {
    if (fnr_usb_device != NULL) {
        (void)fnr_hid_device_usb_event(fnr_usb_device, FNR_HID_USB_RESUME);
    }
}

void tud_event_hook_cb(uint8_t rhport, uint32_t eventid, bool in_isr) {
    (void)rhport;
    (void)in_isr;
    if (eventid == (uint32_t)DCD_EVENT_BUS_RESET) {
        fnr_bus_reset_pending = true;
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
    if (fnr_usb_device == NULL || report_type != HID_REPORT_TYPE_INPUT ||
        buffer == NULL || reqlen == 0U) {
        return 0U;
    }

    fnr_hid_report report;
    const fnr_hid_status status = fnr_hid_device_control_report(
        fnr_usb_device, instance, report_id, &report);
    if (status != FNR_HID_STATUS_OK || report.instance != instance ||
        report.report_id != report_id) {
        return 0U;
    }

    const uint16_t size = report.size < reqlen ? report.size : reqlen;
    memcpy(buffer, report.data, size);
    return size;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
    /* v0.1 relative_mouse has no host-to-device HID output report. */
}

void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol) {
    (void)instance;
    (void)protocol;
    /* The v0.1 three-byte report is deliberately boot-mouse compatible. */
}

bool tud_hid_set_idle_cb(uint8_t instance, uint8_t idle_rate) {
    (void)instance;
    /* No periodic synthetic reports are generated. A non-zero HID idle rate
     * cannot be honored yet, so stall it rather than claiming support. */
    return idle_rate == 0U;
}

void tud_hid_report_failed_cb(uint8_t instance, hid_report_type_t report_type,
                              uint8_t const *report,
                              uint16_t xferred_bytes) {
    (void)instance;
    (void)report_type;
    (void)report;
    (void)xferred_bytes;
    /* Never retry a relative movement report after an asynchronous transfer
     * failure. The service loop neutralizes instead. */
    fnr_transfer_failure_pending = true;
}
