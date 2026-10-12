#include "fnr/profile_relative_mouse.h"
#include "fnr/envelope.h"

#include <limits.h>
#include <string.h>

static void bump(uint32_t *value) {
    if (*value != UINT32_MAX) ++*value;
}

fnr_relative_mouse_payload_result fnr_relative_mouse_payload_encode(
    int16_t x, int16_t y, uint8_t buttons, uint8_t *out, size_t capacity) {
    if (out == NULL) return FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_ARGUMENT;
    if (capacity < FNR_RELATIVE_MOUSE_PAYLOAD_SIZE)
        return FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_LENGTH;
    if (x < -127 || x > 127 || y < -127 || y > 127)
        return FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_AXIS;
    if ((buttons & (uint8_t)~FNR_RELATIVE_MOUSE_WIRE_BUTTON_MASK) != 0U)
        return FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_BUTTONS;
    out[0] = FNR_RELATIVE_MOUSE_SCHEMA_VERSION;
    out[1] = buttons;
    out[2] = (uint8_t)(int8_t)x;
    out[3] = (uint8_t)(int8_t)y;
    return FNR_RELATIVE_MOUSE_PAYLOAD_OK;
}

fnr_relative_mouse_payload_result fnr_relative_mouse_payload_decode(
    const uint8_t *bytes, size_t length, fnr_relative_mouse_payload *out) {
    if (bytes == NULL || out == NULL)
        return FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_ARGUMENT;
    if (length != FNR_RELATIVE_MOUSE_PAYLOAD_SIZE)
        return FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_LENGTH;
    if (bytes[0] != FNR_RELATIVE_MOUSE_SCHEMA_VERSION)
        return FNR_RELATIVE_MOUSE_PAYLOAD_UNSUPPORTED_VERSION;
    if ((bytes[1] & (uint8_t)~FNR_RELATIVE_MOUSE_WIRE_BUTTON_MASK) != 0U)
        return FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_BUTTONS;
    if (bytes[2] == UINT8_C(0x80) || bytes[3] == UINT8_C(0x80))
        return FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_AXIS;
    fnr_relative_mouse_payload candidate;
    candidate.buttons = bytes[1];
    candidate.x = (int8_t)bytes[2];
    candidate.y = (int8_t)bytes[3];
    *out = candidate;
    return FNR_RELATIVE_MOUSE_PAYLOAD_OK;
}

static bool profile_validate(void *context, const fnr_profile_view *view) {
    (void)context;
    fnr_relative_mouse_payload payload;
    if (view == NULL || view->message_type != FNR_PLATFORM_MESSAGE_PROFILE_DATA ||
        view->flags != 0U) return false;
    return fnr_relative_mouse_payload_decode(view->payload, view->payload_size,
                                             &payload) == FNR_RELATIVE_MOUSE_PAYLOAD_OK;
}

static fnr_profile_accept_result profile_accept(void *context,
                                                 const fnr_profile_view *view) {
    fnr_relative_mouse_profile *profile = (fnr_relative_mouse_profile *)context;
    fnr_relative_mouse_payload payload;
    if (profile == NULL || profile->mouse == NULL || profile->hid == NULL ||
        view == NULL || fnr_relative_mouse_payload_decode(view->payload,
        view->payload_size, &payload) != FNR_RELATIVE_MOUSE_PAYLOAD_OK) {
        return FNR_PROFILE_ACCEPT_OUTPUT_DROPPED;
    }
    bump(&profile->accepted);
    if (profile->hid->usb_state != FNR_HID_USB_STATE_MOUNTED) {
        fnr_relative_mouse_release_all(profile->mouse);
        bump(&profile->output_dropped);
        return FNR_PROFILE_ACCEPT_OUTPUT_DROPPED;
    }
    (void)fnr_relative_mouse_apply_input(
        profile->mouse, payload.x, payload.y,
        (payload.buttons & FNR_RELATIVE_MOUSE_WIRE_BUTTON_LEFT) != 0U,
        (payload.buttons & FNR_RELATIVE_MOUSE_WIRE_BUTTON_RIGHT) != 0U);
    const fnr_hid_status status = fnr_hid_device_publish_current(profile->hid);
    if (status == FNR_HID_STATUS_OK || status == FNR_HID_STATUS_REPLACED_PENDING)
        return FNR_PROFILE_ACCEPT_APPLIED;
    fnr_relative_mouse_release_all(profile->mouse);
    bump(&profile->output_dropped);
    return FNR_PROFILE_ACCEPT_OUTPUT_DROPPED;
}

static void profile_neutralize(void *context, fnr_profile_neutral_reason reason) {
    fnr_relative_mouse_profile *profile = (fnr_relative_mouse_profile *)context;
    (void)reason;
    if (profile == NULL || profile->mouse == NULL) return;
    bump(&profile->neutralizations);
    if (profile->hid != NULL && fnr_hid_device_selected_profile(profile->hid) != NULL) {
        (void)fnr_hid_device_invalidate_output(profile->hid);
    } else {
        fnr_relative_mouse_release_all(profile->mouse);
    }
}

static void profile_usb_state(void *context, fnr_hid_usb_state state) {
    fnr_relative_mouse_profile *profile = (fnr_relative_mouse_profile *)context;
    if (profile == NULL || profile->mouse == NULL) return;
    if (state != FNR_HID_USB_STATE_MOUNTED)
        fnr_relative_mouse_release_all(profile->mouse);
}

void fnr_relative_mouse_profile_init(fnr_relative_mouse_profile *profile,
                                     fnr_relative_mouse_state *mouse,
                                     fnr_hid_device *hid) {
    if (profile == NULL) return;
    memset(profile, 0, sizeof(*profile));
    profile->mouse = mouse;
    profile->hid = hid;
}

void fnr_relative_mouse_profile_make_ops(fnr_relative_mouse_profile *profile,
                                         fnr_profile_ops *ops) {
    if (ops == NULL) return;
    ops->profile_id = FNR_PROFILE_ID_RELATIVE_MOUSE;
    ops->context = profile;
    ops->validate = profile_validate;
    ops->accept = profile_accept;
    ops->neutralize = profile_neutralize;
    ops->usb_state = profile_usb_state;
}
