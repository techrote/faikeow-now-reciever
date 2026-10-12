#include "fnr/receiver.h"

#include <limits.h>
#include <string.h>

static void bump(uint32_t *value) {
    if (*value != UINT32_MAX) ++*value;
}

static void add_sat(uint32_t *value, uint32_t amount) {
    if (UINT32_MAX - *value < amount) *value = UINT32_MAX;
    else *value += amount;
}

static bool profile_valid(const fnr_profile_ops *profile) {
    return profile != NULL && profile->profile_id != 0U && profile->validate != NULL &&
           profile->accept != NULL && profile->neutralize != NULL;
}

void fnr_receiver_config_default(fnr_receiver_config *config) {
    if (config == NULL) return;
    memset(config, 0, sizeof(*config));
    config->peer_enabled = false;
    config->platform_version = FNR_PLATFORM_VERSION;
    config->profile_id = FNR_PROFILE_ID_RELATIVE_MOUSE;
    config->freshness_timeout_ms = FNR_RECEIVER_DEFAULT_FRESHNESS_TIMEOUT_MS;
}

bool fnr_receiver_config_valid(const fnr_receiver_config *config) {
    if (config == NULL || config->platform_version != FNR_PLATFORM_VERSION ||
        config->profile_id == 0U || config->freshness_timeout_ms == 0U ||
        config->freshness_timeout_ms >= UINT32_C(0x80000000)) return false;
    if (config->peer_enabled) {
        bool nonzero = false;
        for (size_t i = 0U; i < FNR_MAC_SIZE; ++i)
            nonzero = nonzero || config->peer_mac[i] != 0U;
        if (!nonzero) return false;
    }
    return true;
}

static void neutralize(fnr_receiver_core *core, fnr_profile_neutral_reason reason) {
    if (core->profile != NULL)
        core->profile->neutralize(core->profile->context, reason);
}

bool fnr_receiver_core_init(fnr_receiver_core *core,
                            const fnr_receiver_config *config,
                            const fnr_profile_ops *profile) {
    if (core == NULL || !fnr_receiver_config_valid(config) || !profile_valid(profile) ||
        profile->profile_id != config->profile_id) return false;
    memset(core, 0, sizeof(*core));
    core->config = *config;
    core->profile = profile;
    core->usb_state = FNR_HID_USB_STATE_DETACHED;
    return true;
}

bool fnr_receiver_core_reconfigure(fnr_receiver_core *core,
                                   const fnr_receiver_config *config) {
    if (core == NULL || !fnr_receiver_config_valid(config) || core->profile == NULL ||
        core->profile->profile_id != config->profile_id) return false;
    neutralize(core, FNR_PROFILE_NEUTRAL_RECONFIGURE);
    core->config = *config;
    core->have_session = false;
    core->have_sequence = false;
    core->fresh = false;
    core->session = 0U;
    core->last_sequence = 0U;
    core->last_accepted_ms = 0U;
    memset(core->retired_sessions, 0, sizeof(core->retired_sessions));
    core->retired_count = 0U;
    core->retired_next = 0U;
    return true;
}

static bool peer_matches(const fnr_receiver_config *config, const uint8_t *mac) {
    return config->peer_enabled && memcmp(config->peer_mac, mac, FNR_MAC_SIZE) == 0;
}

static bool session_retired(const fnr_receiver_core *core, uint32_t session) {
    for (uint8_t i = 0U; i < core->retired_count; ++i)
        if (core->retired_sessions[i] == session) return true;
    return false;
}

static void retire_session(fnr_receiver_core *core, uint32_t session) {
    if (session == 0U || session_retired(core, session)) return;
    core->retired_sessions[core->retired_next] = session;
    core->retired_next = (uint8_t)((core->retired_next + 1U) %
                                    FNR_RECEIVER_RETIRED_SESSION_CAPACITY);
    if (core->retired_count < FNR_RECEIVER_RETIRED_SESSION_CAPACITY)
        ++core->retired_count;
}

static fnr_receiver_result classify_sequence(fnr_receiver_core *core,
                                             uint32_t sequence) {
    if (!core->have_sequence) return FNR_RECEIVER_ACCEPTED;
    const fnr_sequence_relation relation =
        fnr_sequence_compare(core->last_sequence, sequence);
    if (relation == FNR_SEQUENCE_DUPLICATE) {
        bump(&core->counters.duplicates);
        return FNR_RECEIVER_DUPLICATE;
    }
    if (relation == FNR_SEQUENCE_STALE) {
        bump(&core->counters.stale);
        return FNR_RECEIVER_STALE;
    }
    if (relation == FNR_SEQUENCE_AMBIGUOUS) {
        bump(&core->counters.ambiguous_sequence);
        return FNR_RECEIVER_AMBIGUOUS_SEQUENCE;
    }
    const uint32_t delta = sequence - core->last_sequence;
    if (delta > 1U) {
        bump(&core->counters.sequence_gap_events);
        add_sat(&core->counters.sequence_missing, delta - 1U);
    }
    return FNR_RECEIVER_ACCEPTED;
}

fnr_receiver_result fnr_receiver_core_accept_datagram(fnr_receiver_core *core,
                                                       const fnr_datagram *datagram,
                                                       uint32_t now_ms) {
    if (core == NULL || datagram == NULL || core->profile == NULL)
        return FNR_RECEIVER_INVALID_ARGUMENT;
    bump(&core->counters.datagrams_seen);
    if (!peer_matches(&core->config, datagram->source_mac)) {
        bump(&core->counters.unauthorized_peer);
        return FNR_RECEIVER_UNAUTHORIZED_PEER;
    }
    fnr_envelope envelope;
    const fnr_envelope_result decoded =
        fnr_envelope_decode(datagram->payload, datagram->size, &envelope);
    if (decoded != FNR_ENVELOPE_OK ||
        envelope.version != core->config.platform_version) {
        bump(&core->counters.malformed_envelopes);
        return FNR_RECEIVER_MALFORMED_ENVELOPE;
    }
    if (envelope.profile_id != core->config.profile_id ||
        envelope.profile_id != core->profile->profile_id) {
        bump(&core->counters.wrong_profile);
        return FNR_RECEIVER_WRONG_PROFILE;
    }
    const fnr_profile_view view = {
        envelope.message_type, envelope.flags, envelope.payload,
        envelope.payload_length
    };
    if (!core->profile->validate(core->profile->context, &view)) {
        bump(&core->counters.invalid_profile_payload);
        return FNR_RECEIVER_INVALID_PROFILE_PAYLOAD;
    }

    if (!core->have_session) {
        core->session = envelope.session;
        core->have_session = true;
        core->have_sequence = false;
    } else if (envelope.session != core->session) {
        if (session_retired(core, envelope.session)) {
            bump(&core->counters.retired_session_rejects);
            return FNR_RECEIVER_RETIRED_SESSION;
        }
        if (core->fresh && envelope.sequence != 0U) {
            bump(&core->counters.session_change_deferred);
            return FNR_RECEIVER_SESSION_CHANGE_DEFERRED;
        }
        retire_session(core, core->session);
        neutralize(core, FNR_PROFILE_NEUTRAL_SESSION_CHANGE);
        core->session = envelope.session;
        core->have_sequence = false;
        core->fresh = false;
        bump(&core->counters.session_changes);
    }

    const fnr_receiver_result ordering =
        classify_sequence(core, envelope.sequence);
    if (ordering != FNR_RECEIVER_ACCEPTED) return ordering;

    core->last_sequence = envelope.sequence;
    core->have_sequence = true;
    core->last_accepted_ms = now_ms;
    core->fresh = true;
    const fnr_profile_accept_result applied =
        core->profile->accept(core->profile->context, &view);
    bump(&core->counters.accepted);
    if (applied == FNR_PROFILE_ACCEPT_OUTPUT_DROPPED) {
        bump(&core->counters.output_dropped);
        return FNR_RECEIVER_ACCEPTED_OUTPUT_DROPPED;
    }
    return FNR_RECEIVER_ACCEPTED;
}

void fnr_receiver_core_transport_events(fnr_receiver_core *core,
                                        uint32_t events) {
    if (core == NULL || events == 0U) return;
    const uint32_t invalidating = FNR_TRANSPORT_EVENT_RESET |
        FNR_TRANSPORT_EVENT_LINK_ERROR |
        FNR_TRANSPORT_EVENT_GENERATION_CHANGE |
        FNR_TRANSPORT_EVENT_SEQUENCE_DISCONTINUITY |
        FNR_TRANSPORT_EVENT_RADIO_RESET |
        FNR_TRANSPORT_EVENT_RADIO_UNAVAILABLE |
        FNR_TRANSPORT_EVENT_TIMEOUT;
    if ((events & invalidating) == 0U) return;
    core->fresh = false;
    core->have_sequence = false;
    neutralize(core, FNR_PROFILE_NEUTRAL_TRANSPORT);
    bump(&core->counters.transport_invalidations);
}

void fnr_receiver_core_tick(fnr_receiver_core *core, uint32_t now_ms) {
    if (core == NULL || !core->fresh) return;
    if ((uint32_t)(now_ms - core->last_accepted_ms) <
        core->config.freshness_timeout_ms) return;
    core->fresh = false;
    neutralize(core, FNR_PROFILE_NEUTRAL_FRESHNESS_TIMEOUT);
    bump(&core->counters.freshness_timeouts);
}

void fnr_receiver_core_usb_state(fnr_receiver_core *core,
                                 fnr_hid_usb_state state) {
    if (core == NULL || state > FNR_HID_USB_STATE_SUSPENDED ||
        state == core->usb_state) return;
    core->usb_state = state;
    core->fresh = false;
    if (core->profile != NULL && core->profile->usb_state != NULL)
        core->profile->usb_state(core->profile->context, state);
    if (state != FNR_HID_USB_STATE_MOUNTED) {
        neutralize(core, FNR_PROFILE_NEUTRAL_USB);
        bump(&core->counters.usb_invalidations);
    }
}
