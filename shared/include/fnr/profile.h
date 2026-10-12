#ifndef FNR_PROFILE_H
#define FNR_PROFILE_H

#include "fnr/hid.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FNR_PROFILE_ID_RELATIVE_MOUSE = 1
};

typedef enum fnr_profile_neutral_reason {
    FNR_PROFILE_NEUTRAL_SESSION_CHANGE = 0,
    FNR_PROFILE_NEUTRAL_FRESHNESS_TIMEOUT,
    FNR_PROFILE_NEUTRAL_TRANSPORT,
    FNR_PROFILE_NEUTRAL_USB,
    FNR_PROFILE_NEUTRAL_RECONFIGURE
} fnr_profile_neutral_reason;

typedef enum fnr_profile_accept_result {
    FNR_PROFILE_ACCEPT_APPLIED = 0,
    FNR_PROFILE_ACCEPT_OUTPUT_DROPPED = 1
} fnr_profile_accept_result;

typedef struct fnr_profile_view {
    uint8_t message_type;
    uint8_t flags;
    const uint8_t *payload;
    uint16_t payload_size;
} fnr_profile_view;

typedef struct fnr_profile_ops {
    uint8_t profile_id;
    void *context;
    bool (*validate)(void *context, const fnr_profile_view *view);
    fnr_profile_accept_result (*accept)(void *context, const fnr_profile_view *view);
    void (*neutralize)(void *context, fnr_profile_neutral_reason reason);
    void (*usb_state)(void *context, fnr_hid_usb_state state);
} fnr_profile_ops;

#ifdef __cplusplus
}
#endif
#endif
