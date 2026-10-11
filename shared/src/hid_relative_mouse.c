#include "fnr/hid_relative_mouse.h"

#include <string.h>

/* Development-only identity. 0xCAFE is TinyUSB's example/test VID; the
 * project-local PID is not an assigned production identity. FNR-009 must use
 * a properly allocated VID/PID before a distributable production release. */
enum {
    FNR_USB_VID_LO = 0xFE,
    FNR_USB_VID_HI = 0xCA,
    FNR_USB_PID_LO = 0x03,
    FNR_USB_PID_HI = 0xF0
};

static const uint8_t fnr_relative_mouse_report_descriptor[] = {
    0x05, 0x01,       /* Usage Page (Generic Desktop) */
    0x09, 0x02,       /* Usage (Mouse) */
    0xA1, 0x01,       /* Collection (Application) */
    0x09, 0x01,       /*   Usage (Pointer) */
    0xA1, 0x00,       /*   Collection (Physical) */
    0x05, 0x09,       /*     Usage Page (Button) */
    0x19, 0x01,       /*     Usage Minimum (1) */
    0x29, 0x02,       /*     Usage Maximum (2) */
    0x15, 0x00,       /*     Logical Minimum (0) */
    0x25, 0x01,       /*     Logical Maximum (1) */
    0x95, 0x02,       /*     Report Count (2) */
    0x75, 0x01,       /*     Report Size (1) */
    0x81, 0x02,       /*     Input (Data, Variable, Absolute) */
    0x95, 0x01,       /*     Report Count (1) */
    0x75, 0x06,       /*     Report Size (6) */
    0x81, 0x03,       /*     Input (Constant, Variable, Absolute) */
    0x05, 0x01,       /*     Usage Page (Generic Desktop) */
    0x09, 0x30,       /*     Usage (X) */
    0x09, 0x31,       /*     Usage (Y) */
    0x15, 0x81,       /*     Logical Minimum (-127) */
    0x25, 0x7F,       /*     Logical Maximum (127) */
    0x75, 0x08,       /*     Report Size (8) */
    0x95, 0x02,       /*     Report Count (2) */
    0x81, 0x06,       /*     Input (Data, Variable, Relative) */
    0xC0,             /*   End Collection */
    0xC0              /* End Collection */
};

static const uint8_t fnr_relative_mouse_device_descriptor[] = {
    18, 0x01,         /* bLength, DEVICE */
    0x00, 0x02,       /* bcdUSB 2.00 */
    0x00, 0x00, 0x00, /* class/subclass/protocol: interface-defined */
    64,               /* EP0 max packet */
    FNR_USB_VID_LO, FNR_USB_VID_HI,
    FNR_USB_PID_LO, FNR_USB_PID_HI,
    0x00, 0x01,       /* bcdDevice 1.00 */
    0x01,             /* manufacturer string */
    0x02,             /* product string */
    0x00,             /* no serial in software-only v0.1 backend */
    0x01              /* one configuration */
};

enum {
    FNR_MOUSE_CONFIG_TOTAL_LENGTH = 34,
    FNR_MOUSE_REPORT_DESCRIPTOR_LENGTH = sizeof(fnr_relative_mouse_report_descriptor)
};

static const uint8_t fnr_relative_mouse_configuration_descriptor[] = {
    9, 0x02,                                  /* configuration descriptor */
    FNR_MOUSE_CONFIG_TOTAL_LENGTH, 0x00,
    0x01, 0x01, 0x00, 0x80, 50,              /* one interface, 100 mA */
    9, 0x04,                                  /* interface descriptor */
    0x00, 0x00, 0x01,                         /* interface 0, alt 0, one EP */
    0x03, 0x01, 0x02, 0x00,                   /* HID, boot subclass, mouse */
    9, 0x21,                                  /* HID descriptor */
    0x11, 0x01, 0x00, 0x01, 0x22,
    FNR_MOUSE_REPORT_DESCRIPTOR_LENGTH, 0x00,
    7, 0x05,                                  /* endpoint descriptor */
    0x81, 0x03,                               /* IN EP1, interrupt */
    0x08, 0x00,                               /* max packet 8 */
    0x01                                      /* 1 ms interval */
};

_Static_assert(sizeof(fnr_relative_mouse_device_descriptor) == 18U,
               "USB device descriptor must be 18 bytes");
_Static_assert(sizeof(fnr_relative_mouse_report_descriptor) == 50U,
               "relative_mouse report descriptor size drifted");
_Static_assert(sizeof(fnr_relative_mouse_configuration_descriptor) ==
                   FNR_MOUSE_CONFIG_TOTAL_LENGTH,
               "USB configuration descriptor length mismatch");

static const fnr_hid_report_descriptor fnr_relative_mouse_reports[] = {
    {fnr_relative_mouse_report_descriptor,
     (uint16_t)sizeof(fnr_relative_mouse_report_descriptor)}
};

static const char *const fnr_relative_mouse_strings[] = {
    "FNR Project",
    "FNR Relative Mouse (Development)"
};

static const fnr_hid_descriptor_set fnr_relative_mouse_descriptor_set = {
    fnr_relative_mouse_device_descriptor,
    (uint16_t)sizeof(fnr_relative_mouse_device_descriptor),
    fnr_relative_mouse_configuration_descriptor,
    (uint16_t)sizeof(fnr_relative_mouse_configuration_descriptor),
    fnr_relative_mouse_reports,
    (uint8_t)(sizeof(fnr_relative_mouse_reports) / sizeof(fnr_relative_mouse_reports[0])),
    fnr_relative_mouse_strings,
    (uint8_t)(sizeof(fnr_relative_mouse_strings) / sizeof(fnr_relative_mouse_strings[0]))
};

static int8_t fnr_relative_mouse_clamp_axis(int16_t value, bool *clamped) {
    if (value < -127) {
        *clamped = true;
        return (int8_t)-127;
    }
    if (value > 127) {
        *clamped = true;
        return (int8_t)127;
    }
    return (int8_t)value;
}

static const fnr_hid_descriptor_set *fnr_relative_mouse_backend_descriptors(void *context) {
    (void)context;
    return &fnr_relative_mouse_descriptor_set;
}

static fnr_hid_status fnr_relative_mouse_build_report(void *context, fnr_hid_report *out) {
    fnr_relative_mouse_state *state = (fnr_relative_mouse_state *)context;
    if (state == NULL || out == NULL) {
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    out->instance = 0U;
    out->report_id = 0U;
    out->size = FNR_RELATIVE_MOUSE_REPORT_SIZE;
    out->data[0] = (uint8_t)(state->buttons &
                             (FNR_RELATIVE_MOUSE_BUTTON_LEFT |
                              FNR_RELATIVE_MOUSE_BUTTON_RIGHT));
    out->data[1] = (uint8_t)state->x;
    out->data[2] = (uint8_t)state->y;
    return FNR_HID_STATUS_OK;
}

static fnr_hid_status fnr_relative_mouse_build_control_report(void *context,
                                                              uint8_t instance,
                                                              uint8_t report_id,
                                                              fnr_hid_report *out) {
    fnr_relative_mouse_state *state = (fnr_relative_mouse_state *)context;
    if (state == NULL || out == NULL || instance != 0U || report_id != 0U) {
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    out->instance = 0U;
    out->report_id = 0U;
    out->size = FNR_RELATIVE_MOUSE_REPORT_SIZE;
    out->data[0] = (uint8_t)(state->buttons &
                             (FNR_RELATIVE_MOUSE_BUTTON_LEFT |
                              FNR_RELATIVE_MOUSE_BUTTON_RIGHT));
    /* Relative motion is never echoed through GET_REPORT, which prevents a
     * control transfer from duplicating a pending movement event. */
    out->data[1] = 0U;
    out->data[2] = 0U;
    return FNR_HID_STATUS_OK;
}

static void fnr_relative_mouse_backend_neutralize(void *context) {
    fnr_relative_mouse_release_all((fnr_relative_mouse_state *)context);
}

static void fnr_relative_mouse_report_submitted(void *context,
                                                const fnr_hid_report *report) {
    fnr_relative_mouse_state *state = (fnr_relative_mouse_state *)context;
    (void)report;
    if (state == NULL) {
        return;
    }
    /* Buttons are complete current state and remain held; relative movement is
     * event-like and is consumed exactly once when the transfer is accepted. */
    state->x = 0;
    state->y = 0;
}

static void fnr_relative_mouse_usb_event(void *context, fnr_hid_usb_event event) {
    (void)context;
    (void)event;
}

static const fnr_hid_backend_ops fnr_relative_mouse_ops = {
    fnr_relative_mouse_backend_descriptors,
    fnr_relative_mouse_build_report,
    fnr_relative_mouse_build_control_report,
    fnr_relative_mouse_backend_neutralize,
    fnr_relative_mouse_report_submitted,
    fnr_relative_mouse_usb_event
};

void fnr_relative_mouse_init(fnr_relative_mouse_state *state) {
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

void fnr_relative_mouse_make_profile(fnr_relative_mouse_state *state,
                                     fnr_hid_profile *profile) {
    if (profile == NULL) {
        return;
    }
    profile->name = "relative_mouse";
    profile->context = state;
    profile->ops = &fnr_relative_mouse_ops;
}

fnr_hid_status fnr_relative_mouse_apply_input(fnr_relative_mouse_state *state,
                                              int16_t x, int16_t y,
                                              bool left, bool right) {
    if (state == NULL) {
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    bool x_clamped = false;
    bool y_clamped = false;
    state->x = fnr_relative_mouse_clamp_axis(x, &x_clamped);
    state->y = fnr_relative_mouse_clamp_axis(y, &y_clamped);
    state->buttons = 0U;
    if (left) {
        state->buttons |= FNR_RELATIVE_MOUSE_BUTTON_LEFT;
    }
    if (right) {
        state->buttons |= FNR_RELATIVE_MOUSE_BUTTON_RIGHT;
    }
    ++state->accepted_inputs;
    if (x_clamped) {
        ++state->clamped_axes;
    }
    if (y_clamped) {
        ++state->clamped_axes;
    }
    return (x_clamped || y_clamped) ? FNR_HID_STATUS_CLAMPED : FNR_HID_STATUS_OK;
}

void fnr_relative_mouse_release_all(fnr_relative_mouse_state *state) {
    if (state == NULL) {
        return;
    }
    state->x = 0;
    state->y = 0;
    state->buttons = 0U;
}

const fnr_hid_descriptor_set *fnr_relative_mouse_descriptors(void) {
    return &fnr_relative_mouse_descriptor_set;
}
