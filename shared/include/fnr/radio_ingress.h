#ifndef FNR_RADIO_INGRESS_H
#define FNR_RADIO_INGRESS_H
#include "fnr/platform.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Locking: every ingress operation must execute under the same platform
 * critical section. No internal volatile or implicit thread-safety claim.
 * Callback and foreground must not overlap or reenter these functions. */
typedef enum fnr_radio_state {
    FNR_RADIO_UNCONFIGURED = 0, FNR_RADIO_STARTING = 1,
    FNR_RADIO_READY = 2, FNR_RADIO_ERROR = 3
} fnr_radio_state;
typedef struct fnr_radio_config {
    uint8_t channel;
    uint8_t allowed_mac[FNR_MAC_SIZE];
    bool enabled;
} fnr_radio_config;
typedef struct fnr_radio_counters {
    uint32_t received, accepted, rejected_source, invalid_length;
    uint32_t dropped, consumed, starts, restarts, errors;
} fnr_radio_counters;
typedef struct fnr_radio_ingress {
    fnr_radio_config config;
    fnr_radio_state state;
    fnr_radio_counters counters;
    fnr_datagram slot;
    bool pending;
} fnr_radio_ingress;
bool fnr_radio_config_valid(const fnr_radio_config *config);
void fnr_radio_reset(fnr_radio_ingress *ctx, const fnr_radio_config *config);
void fnr_radio_transition(fnr_radio_ingress *ctx, fnr_radio_state state);
void fnr_radio_receive(fnr_radio_ingress *ctx, const uint8_t *mac,
                       const uint8_t *payload, size_t length);
bool fnr_radio_take(fnr_radio_ingress *ctx, fnr_datagram *out);
void fnr_radio_snapshot(const fnr_radio_ingress *ctx, fnr_radio_ingress *out);
#ifdef __cplusplus
}
#endif
#endif
