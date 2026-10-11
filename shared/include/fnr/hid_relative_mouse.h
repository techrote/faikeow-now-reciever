#ifndef FNR_HID_RELATIVE_MOUSE_H
#define FNR_HID_RELATIVE_MOUSE_H

#include "fnr/hid.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FNR_RELATIVE_MOUSE_BUTTON_LEFT = 1U << 0,
    FNR_RELATIVE_MOUSE_BUTTON_RIGHT = 1U << 1,
    FNR_RELATIVE_MOUSE_REPORT_SIZE = 3
};

typedef struct fnr_relative_mouse_state {
    int8_t x;
    int8_t y;
    uint8_t buttons;
    uint32_t accepted_inputs;
    uint32_t clamped_axes;
} fnr_relative_mouse_state;

void fnr_relative_mouse_init(fnr_relative_mouse_state *state);
void fnr_relative_mouse_make_profile(fnr_relative_mouse_state *state,
                                     fnr_hid_profile *profile);
fnr_hid_status fnr_relative_mouse_apply_input(fnr_relative_mouse_state *state,
                                              int16_t x, int16_t y,
                                              bool left, bool right);
void fnr_relative_mouse_release_all(fnr_relative_mouse_state *state);
const fnr_hid_descriptor_set *fnr_relative_mouse_descriptors(void);

#ifdef __cplusplus
}
#endif

#endif
