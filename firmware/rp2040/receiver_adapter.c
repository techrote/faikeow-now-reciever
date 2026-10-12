#include "receiver_adapter.h"

#include "fnr/profile_relative_mouse.h"
#include "pico/stdlib.h"
#include "transport_adapter.h"

static fnr_hid_device *active_hid;
static fnr_relative_mouse_profile relative_profile;
static fnr_profile_ops relative_ops;
static fnr_receiver_core receiver_core;
static bool initialized;

static bool provisioning_staged;
static bool radio_acknowledged;
static uint8_t expected_channel;
static fnr_receiver_config desired_config;
static fnr_receiver_config safe_config;

static uint32_t invalidating_transport_events(void) {
    return FNR_TRANSPORT_EVENT_RESET |
           FNR_TRANSPORT_EVENT_LINK_ERROR |
           FNR_TRANSPORT_EVENT_GENERATION_CHANGE |
           FNR_TRANSPORT_EVENT_SEQUENCE_DISCONTINUITY |
           FNR_TRANSPORT_EVENT_RADIO_RESET |
           FNR_TRANSPORT_EVENT_RADIO_UNAVAILABLE |
           FNR_TRANSPORT_EVENT_TIMEOUT;
}

static bool set_radio_acknowledged(bool acknowledged) {
    if (!provisioning_staged) return !acknowledged;
    if (acknowledged == radio_acknowledged) return true;
    const fnr_receiver_config *next = acknowledged ? &desired_config : &safe_config;
    /* Temporary radio admission is not a new sender configuration. Preserve
     * session, last sequence and retired sessions across both transitions.
     * The existing transport invalidation supplies a neutral HID barrier and
     * revokes freshness, including when the trigger is a status-only change. */
    fnr_receiver_core_transport_events(&receiver_core,
                                       FNR_TRANSPORT_EVENT_RADIO_UNAVAILABLE);
    receiver_core.config = *next;
    radio_acknowledged = acknowledged;
    return true;
}

bool fnr_receiver_adapter_profile_supported(uint8_t profile_id,
                                             uint8_t schema_version) {
    return initialized && profile_id == relative_ops.profile_id &&
           schema_version == FNR_RELATIVE_MOUSE_SCHEMA_VERSION;
}

bool fnr_receiver_adapter_init(fnr_hid_device *hid,
                               fnr_relative_mouse_state *mouse) {
    initialized = false;
    active_hid = NULL;
    provisioning_staged = false;
    radio_acknowledged = false;
    expected_channel = 0U;
    if (hid == NULL || mouse == NULL) return false;
    const fnr_hid_profile *selected = fnr_hid_device_selected_profile(hid);
    if (selected == NULL || selected->context != mouse) return false;
    fnr_relative_mouse_profile_init(&relative_profile, mouse, hid);
    fnr_relative_mouse_profile_make_ops(&relative_profile, &relative_ops);
    fnr_receiver_config config;
    fnr_receiver_config_default(&config);
    if (!fnr_receiver_core_init(&receiver_core, &config, &relative_ops))
        return false;
    desired_config = config;
    safe_config = config;
    active_hid = hid;
    initialized = true;
    return true;
}

bool fnr_receiver_adapter_configure(const fnr_receiver_config *config) {
    if (!initialized || config == NULL) return false;
    provisioning_staged = false;
    radio_acknowledged = false;
    expected_channel = 0U;
    return fnr_receiver_core_reconfigure(&receiver_core, config);
}

bool fnr_receiver_adapter_stage_provisioning(const fnr_receiver_config *config,
                                             uint8_t radio_channel) {
    if (!initialized || config == NULL || !fnr_receiver_config_valid(config) ||
        !config->peer_enabled || !config->profile_enabled ||
        radio_channel < 1U || radio_channel > 14U) {
        return false;
    }

    fnr_receiver_config candidate_safe = *config;
    candidate_safe.peer_enabled = false;
    candidate_safe.profile_enabled = false;
    if (!fnr_receiver_core_reconfigure(&receiver_core, &candidate_safe))
        return false;

    desired_config = *config;
    safe_config = candidate_safe;
    provisioning_staged = true;
    radio_acknowledged = false;
    expected_channel = radio_channel;
    return true;
}

bool fnr_receiver_adapter_revoke_provisioning(void) {
    if (!initialized) return false;
    fnr_receiver_config config;
    fnr_receiver_config_default(&config);
    desired_config = config;
    safe_config = config;
    provisioning_staged = false;
    radio_acknowledged = false;
    expected_channel = 0U;
    return fnr_receiver_core_reconfigure(&receiver_core, &config);
}

void fnr_receiver_adapter_provisioning_snapshot(
    fnr_receiver_provisioning_status *out) {
    if (out == NULL) return;
    out->staged = provisioning_staged;
    out->radio_acknowledged = radio_acknowledged;
    out->expected_channel = expected_channel;
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

    const uint32_t events = api->take_events();
    fnr_receiver_core_transport_events(&receiver_core, events);
    if (provisioning_staged &&
        (events & invalidating_transport_events()) != 0U) {
        (void)set_radio_acknowledged(false);
    }
    fnr_receiver_core_tick(&receiver_core, now_ms);

    fnr_transport_frame status_frame;
    while (api->take_status(&status_frame)) {
        fnr_transport_radio_status status;
        if (fnr_transport_unpack_status(&status_frame, &status) != FNR_TRANSPORT_OK)
            continue;
        if (provisioning_staged) {
            const bool ready = status.state == FNR_RADIO_READY &&
                               status.channel == expected_channel;
            (void)set_radio_acknowledged(ready);
        }
    }

    fnr_transport_frame frame;
    if (api->take_datagram(&frame)) {
        fnr_datagram datagram;
        if (fnr_transport_unpack_datagram(&frame, &datagram) == FNR_TRANSPORT_OK)
            (void)fnr_receiver_core_accept_datagram(&receiver_core, &datagram,
                                                     now_ms);
    }
}
