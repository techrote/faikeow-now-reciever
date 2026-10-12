#ifndef FNR_RECEIVER_H
#define FNR_RECEIVER_H

#include "fnr/envelope.h"
#include "fnr/profile.h"
#include "fnr/transport.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FNR_RECEIVER_DEFAULT_FRESHNESS_TIMEOUT_MS = 250,
    FNR_RECEIVER_RETIRED_SESSION_CAPACITY = 4
};

typedef struct fnr_receiver_config {
    bool peer_enabled;
    uint8_t peer_mac[FNR_MAC_SIZE];
    uint8_t platform_version;
    uint8_t profile_id;
    uint32_t freshness_timeout_ms;
} fnr_receiver_config;

typedef enum fnr_receiver_result {
    FNR_RECEIVER_ACCEPTED = 0,
    FNR_RECEIVER_ACCEPTED_OUTPUT_DROPPED,
    FNR_RECEIVER_UNAUTHORIZED_PEER,
    FNR_RECEIVER_MALFORMED_ENVELOPE,
    FNR_RECEIVER_WRONG_PROFILE,
    FNR_RECEIVER_INVALID_PROFILE_PAYLOAD,
    FNR_RECEIVER_DUPLICATE,
    FNR_RECEIVER_STALE,
    FNR_RECEIVER_AMBIGUOUS_SEQUENCE,
    FNR_RECEIVER_RETIRED_SESSION,
    FNR_RECEIVER_SESSION_CHANGE_DEFERRED,
    FNR_RECEIVER_INVALID_ARGUMENT
} fnr_receiver_result;

typedef struct fnr_receiver_counters {
    uint32_t datagrams_seen;
    uint32_t unauthorized_peer;
    uint32_t malformed_envelopes;
    uint32_t wrong_profile;
    uint32_t invalid_profile_payload;
    uint32_t accepted;
    uint32_t output_dropped;
    uint32_t duplicates;
    uint32_t stale;
    uint32_t ambiguous_sequence;
    uint32_t sequence_gap_events;
    uint32_t sequence_missing;
    uint32_t session_changes;
    uint32_t retired_session_rejects;
    uint32_t session_change_deferred;
    uint32_t freshness_timeouts;
    uint32_t transport_invalidations;
    uint32_t usb_invalidations;
} fnr_receiver_counters;

typedef struct fnr_receiver_core {
    fnr_receiver_config config;
    const fnr_profile_ops *profile;
    bool have_session;
    bool have_sequence;
    bool fresh;
    uint32_t session;
    uint32_t last_sequence;
    uint32_t last_accepted_ms;
    uint32_t retired_sessions[FNR_RECEIVER_RETIRED_SESSION_CAPACITY];
    uint8_t retired_count;
    uint8_t retired_next;
    fnr_hid_usb_state usb_state;
    fnr_receiver_counters counters;
} fnr_receiver_core;

void fnr_receiver_config_default(fnr_receiver_config *config);
bool fnr_receiver_config_valid(const fnr_receiver_config *config);
bool fnr_receiver_core_init(fnr_receiver_core *core,
                            const fnr_receiver_config *config,
                            const fnr_profile_ops *profile);
bool fnr_receiver_core_reconfigure(fnr_receiver_core *core,
                                   const fnr_receiver_config *config);
fnr_receiver_result fnr_receiver_core_accept_datagram(fnr_receiver_core *core,
                                                       const fnr_datagram *datagram,
                                                       uint32_t now_ms);
void fnr_receiver_core_transport_events(fnr_receiver_core *core, uint32_t events);
void fnr_receiver_core_tick(fnr_receiver_core *core, uint32_t now_ms);
void fnr_receiver_core_usb_state(fnr_receiver_core *core, fnr_hid_usb_state state);

#ifdef __cplusplus
}
#endif
#endif
