#ifndef FNR_PROFILE_RELATIVE_MOUSE_H
#define FNR_PROFILE_RELATIVE_MOUSE_H

#include "fnr/hid_relative_mouse.h"
#include "fnr/profile.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FNR_RELATIVE_MOUSE_SCHEMA_VERSION = 1,
    FNR_RELATIVE_MOUSE_PAYLOAD_SIZE = 4,
    FNR_RELATIVE_MOUSE_WIRE_BUTTON_LEFT = 1U << 0,
    FNR_RELATIVE_MOUSE_WIRE_BUTTON_RIGHT = 1U << 1,
    FNR_RELATIVE_MOUSE_WIRE_BUTTON_MASK = 3U
};

typedef enum fnr_relative_mouse_payload_result {
    FNR_RELATIVE_MOUSE_PAYLOAD_OK = 0,
    FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_ARGUMENT,
    FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_LENGTH,
    FNR_RELATIVE_MOUSE_PAYLOAD_UNSUPPORTED_VERSION,
    FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_BUTTONS,
    FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_AXIS
} fnr_relative_mouse_payload_result;

typedef struct fnr_relative_mouse_payload {
    int8_t x;
    int8_t y;
    uint8_t buttons;
} fnr_relative_mouse_payload;

typedef struct fnr_relative_mouse_profile {
    fnr_relative_mouse_state *mouse;
    fnr_hid_device *hid;
    uint32_t accepted;
    uint32_t output_dropped;
    uint32_t neutralizations;
} fnr_relative_mouse_profile;

fnr_relative_mouse_payload_result fnr_relative_mouse_payload_encode(
    int16_t x, int16_t y, uint8_t buttons, uint8_t *out, size_t capacity);
fnr_relative_mouse_payload_result fnr_relative_mouse_payload_decode(
    const uint8_t *bytes, size_t length, fnr_relative_mouse_payload *out);
void fnr_relative_mouse_profile_init(fnr_relative_mouse_profile *profile,
                                     fnr_relative_mouse_state *mouse,
                                     fnr_hid_device *hid);
void fnr_relative_mouse_profile_make_ops(fnr_relative_mouse_profile *profile,
                                         fnr_profile_ops *ops);

#ifdef __cplusplus
}
#endif
#endif
