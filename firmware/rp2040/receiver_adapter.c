#include "receiver_adapter.h"

#include "fnr/profile_relative_mouse.h"
#include "pico/stdlib.h"
#include "transport_adapter.h"

static fnr_hid_device *active_hid;
static fnr_relative_mouse_profile relative_profile;
static fnr_profile_ops relative_ops;
static fnr_receiver_core receiver_core;
static bool initialized;

bool fnr_receiver_adapter_init(fnr_hid_device *hid,
                               fnr_relative_mouse_state *mouse) {
    initialized = false;
    active_hid = NULL;
    if (hid == NULL || mouse == NULL) return false;
    const fnr_hid_profile *selected = fnr_hid_device_selected_profile(hid);
    if (selected == NULL || selected->context != mouse) return false;
    fnr_relative_mouse_profile_init(&relative_profile, mouse, hid);
    fnr_relative_mouse_profile_make_ops(&relative_profile, &relative_ops);
    fnr_receiver_config config;
    fnr_receiver_config_default(&config);
    if (!fnr_receiver_core_init(&receiver_core, &config, &relative_ops))
        return false;
    active_hid = hid;
    initialized = true;
    return true;
}

bool fnr_receiver_adapter_configure(const fnr_receiver_config *config) {
    return initialized && fnr_receiver_core_reconfigure(&receiver_core, config);
}

void fnr_receiver_adapter_snapshot(fnr_receiver_core *out) {
    if (out != NULL && initialized) *out = receiver_core;
}

void fnr_transport_foreground_consume(const fnr_transport_consumer_api *api) {
    if (!initialized || active_hid == NULL || api == NULL ||
        api->take_events == NULL || api->take_datagram == NULL ||
        api->take_status == NULL) return;

    const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    fnr_receiver_core_usb_state(&receiver_core, active_hid->usb_state);
    fnr_receiver_core_transport_events(&receiver_core, api->take_events());
    fnr_receiver_core_tick(&receiver_core, now_ms);

    fnr_transport_frame status;
    while (api->take_status(&status)) {
        /* FNR-005 already validated status and raised lifecycle events. */
    }

    fnr_transport_frame frame;
    if (api->take_datagram(&frame)) {
        fnr_datagram datagram;
        if (fnr_transport_unpack_datagram(&frame, &datagram) == FNR_TRANSPORT_OK)
            (void)fnr_receiver_core_accept_datagram(&receiver_core, &datagram,
                                                     now_ms);
    }
}
