#ifndef FNR_RADIO_ADAPTER_H
#define FNR_RADIO_ADAPTER_H

#include "fnr/radio_ingress.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { FNR_ESPNOW_KEY_SIZE = 16 };

typedef struct fnr_radio_provisioning {
    fnr_radio_config ingress;
    uint8_t key[FNR_ESPNOW_KEY_SIZE];
    bool key_present;
    uint64_t config_id;
} fnr_radio_provisioning;

typedef struct fnr_radio_provisioning_status {
    bool configured;
    bool encryption_enabled;
    uint8_t channel;
    uint8_t peer_mac[FNR_MAC_SIZE];
    uint64_t config_id;
    fnr_radio_state state;
} fnr_radio_provisioning_status;

/* No implicit provisioning: build defaults to disabled receiver. */
void fnr_radio_start(void);
void fnr_radio_restart(void);

/* Legacy explicit plaintext seam retained only for isolated FNR-002/FNR-004
 * diagnostics. Production main never calls this after FNR-007. */
void fnr_radio_apply_config(const fnr_radio_config *config);

/* Production provisioning is fail-closed and requires a 16-byte per-device
 * ESP-NOW key. Invalid/null input revokes radio admission; there is no
 * production plaintext fallback. Return true only when the SDK is READY. */
bool fnr_radio_apply_provisioning(const fnr_radio_provisioning *provisioning);
void fnr_radio_provisioning_status_foreground(
    fnr_radio_provisioning_status *out);

bool fnr_radio_take_foreground(fnr_datagram *out);
void fnr_radio_status_foreground(fnr_radio_ingress *out);

/* Foreground lifecycle token, modulo 2^32; increments for EVERY start attempt,
 * including disabled/error configurations. Not a saturating diagnostic or
 * sender session. Callers must observe between 2^32 lifecycle operations. */
uint32_t fnr_radio_generation_foreground(void);
void fnr_radio_station_mac(uint8_t out[FNR_MAC_SIZE]);

#ifdef __cplusplus
}
#endif
#endif
