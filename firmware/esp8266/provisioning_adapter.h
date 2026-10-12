#ifndef FNR_ESP_PROVISIONING_ADAPTER_H
#define FNR_ESP_PROVISIONING_ADAPTER_H

#include "fnr/provisioning.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fnr_esp_provisioning_status {
    fnr_provisioning_result record_result;
    fnr_provisioning_config config;
    bool key_present;
    bool radio_ready;
    uint8_t station_mac[FNR_MAC_SIZE];
} fnr_esp_provisioning_status;

/* Invalid/factory configuration is a safe initialized state with radio
 * admission revoked. Return false only for an internal adapter failure. */
bool fnr_esp_provisioning_init(void);
void fnr_esp_provisioning_snapshot(fnr_esp_provisioning_status *out);

#ifdef __cplusplus
}
#endif
#endif
