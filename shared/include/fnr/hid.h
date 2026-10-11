#ifndef FNR_HID_H
#define FNR_HID_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The device/profile state is single-owner and not internally synchronized; callers
 * must serialize logical-input, lifecycle and service operations. Fixed capacities
 * keep profile registration and report ownership bounded. */
enum {
    FNR_HID_MAX_PROFILES = 4,
    FNR_HID_MAX_INTERFACES = 4,
    FNR_HID_MAX_REPORT_SIZE = 64
};

typedef enum fnr_hid_status {
    FNR_HID_STATUS_OK = 0,
    FNR_HID_STATUS_INVALID_ARGUMENT = 1,
    FNR_HID_STATUS_CAPACITY = 2,
    FNR_HID_STATUS_DUPLICATE_PROFILE = 3,
    FNR_HID_STATUS_PROFILE_NOT_FOUND = 4,
    FNR_HID_STATUS_PROFILE_NOT_SELECTED = 5,
    FNR_HID_STATUS_BUSY = 6,
    FNR_HID_STATUS_DESCRIPTOR_INVALID = 7,
    FNR_HID_STATUS_REPORT_INVALID = 8,
    FNR_HID_STATUS_NOT_READY = 9,
    FNR_HID_STATUS_SEND_FAILED = 10,
    FNR_HID_STATUS_REPLACED_PENDING = 11,
    FNR_HID_STATUS_CLAMPED = 12,
    FNR_HID_STATUS_NEUTRAL_PENDING = 13
} fnr_hid_status;

typedef enum fnr_hid_usb_event {
    FNR_HID_USB_RESET = 0,
    FNR_HID_USB_MOUNT = 1,
    FNR_HID_USB_UNMOUNT = 2,
    FNR_HID_USB_SUSPEND = 3,
    FNR_HID_USB_RESUME = 4
} fnr_hid_usb_event;

typedef enum fnr_hid_usb_state {
    FNR_HID_USB_STATE_DETACHED = 0,
    FNR_HID_USB_STATE_MOUNTED = 1,
    FNR_HID_USB_STATE_SUSPENDED = 2
} fnr_hid_usb_state;

/* Descriptor byte arrays are backend-owned immutable storage. They must remain
 * valid for the full lifetime of a registered profile/USB adapter. This type
 * deliberately contains no TinyUSB types. reports[N] maps to HID instance N. */
typedef struct fnr_hid_report_descriptor {
    const uint8_t *bytes;
    uint16_t size;
} fnr_hid_report_descriptor;

typedef struct fnr_hid_descriptor_set {
    const uint8_t *device;
    uint16_t device_size;
    const uint8_t *configuration;
    uint16_t configuration_size;
    const fnr_hid_report_descriptor *reports;
    uint8_t report_count;
    const char *const *strings; /* String index 1 maps to strings[0]. */
    uint8_t string_count;
} fnr_hid_descriptor_set;

/* Reports are copied by value into the generic device's single pending slot;
 * backend scratch storage is never borrowed by the transport. */
typedef struct fnr_hid_report {
    uint8_t instance;
    uint8_t report_id;
    uint16_t size;
    uint8_t data[FNR_HID_MAX_REPORT_SIZE];
} fnr_hid_report;

/* A backend owns HID descriptors and logical-output semantics. build_report()
 * snapshots current output. neutralize() must synchronously establish the
 * profile's safe state. build_control_report() is optional and must not consume
 * event-like output. on_report_submitted() is called once the target transport
 * accepted a transfer; relative/event state may be consumed there.
 * on_usb_event() receives real USB lifecycle events only. */
typedef struct fnr_hid_backend_ops {
    const fnr_hid_descriptor_set *(*descriptors)(void *context);
    fnr_hid_status (*build_report)(void *context, fnr_hid_report *out);
    fnr_hid_status (*build_control_report)(void *context, uint8_t instance,
                                           uint8_t report_id, fnr_hid_report *out);
    void (*neutralize)(void *context);
    void (*on_report_submitted)(void *context, const fnr_hid_report *report);
    void (*on_usb_event)(void *context, fnr_hid_usb_event event);
} fnr_hid_backend_ops;

/* Registration borrows profile/context/ops pointers: they must outlive device.
 * name is a local configuration key only. FNR-006 owns wire profile IDs and
 * schemas; this API deliberately does not freeze or infer them. */
typedef struct fnr_hid_profile {
    const char *name;
    void *context;
    const fnr_hid_backend_ops *ops;
} fnr_hid_profile;

typedef struct fnr_hid_counters {
    uint32_t reports_queued;
    uint32_t reports_replaced;
    uint32_t reports_submitted;
    uint32_t backpressure_events;
    uint32_t send_failures;
    uint32_t transport_failures;
    uint32_t invalidations;
    uint32_t neutral_reports_queued;
    uint32_t reports_blocked_by_neutral;
} fnr_hid_counters;

typedef struct fnr_hid_device {
    const fnr_hid_profile *profiles[FNR_HID_MAX_PROFILES];
    uint8_t profile_count;
    const fnr_hid_profile *selected;
    fnr_hid_usb_state usb_state;
    bool pending;
    bool pending_is_neutral;
    fnr_hid_report pending_report;
    fnr_hid_counters counters;
    fnr_hid_status last_status;
} fnr_hid_device;

/* Target adapter callbacks. A successful send means the target accepted the
 * transfer for submission, not that a physical host has proven receipt. */
typedef bool (*fnr_hid_ready_fn)(void *context, uint8_t instance);
typedef bool (*fnr_hid_send_fn)(void *context, uint8_t instance,
                                uint8_t report_id, const uint8_t *data,
                                uint16_t size);

void fnr_hid_device_init(fnr_hid_device *device);
fnr_hid_status fnr_hid_device_register_profile(fnr_hid_device *device,
                                               const fnr_hid_profile *profile);
/* Selection is allowed only while detached because descriptors define USB
 * identity. A later runtime profile change must force controlled re-enumeration. */
fnr_hid_status fnr_hid_device_select_profile(fnr_hid_device *device,
                                             const char *name);
const fnr_hid_profile *fnr_hid_device_selected_profile(const fnr_hid_device *device);
const fnr_hid_descriptor_set *fnr_hid_device_descriptors(const fnr_hid_device *device);
fnr_hid_status fnr_hid_device_usb_event(fnr_hid_device *device,
                                        fnr_hid_usb_event event);
/* Publish uses one fixed pending slot. A newer snapshot replaces an older
 * unsent non-neutral snapshot and returns REPLACED_PENDING. Recovery-neutral
 * reports are barriers: normal publish is blocked/neutralized until the barrier
 * is submitted and returns NEUTRAL_PENDING; there is never a backlog. */
fnr_hid_status fnr_hid_device_publish_current(fnr_hid_device *device);
/* Generic receiver/transport invalidation neutralizes without fabricating a
 * USB event. If mounted, one neutral report is queued. */
fnr_hid_status fnr_hid_device_invalidate_output(fnr_hid_device *device);
fnr_hid_status fnr_hid_device_note_transport_error(fnr_hid_device *device);
fnr_hid_status fnr_hid_device_service(fnr_hid_device *device,
                                      fnr_hid_ready_fn ready,
                                      fnr_hid_send_fn send,
                                      void *transport_context);
fnr_hid_status fnr_hid_device_control_report(const fnr_hid_device *device,
                                             uint8_t instance,
                                             uint8_t report_id,
                                             fnr_hid_report *out);

#ifdef __cplusplus
}
#endif

#endif
