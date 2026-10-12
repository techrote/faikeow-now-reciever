#ifndef FNR_ESP_TRANSPORT_ADAPTER_H
#define FNR_ESP_TRANSPORT_ADAPTER_H

#include "fnr/transport_tx.h"

#ifdef __cplusplus
extern "C" {
#endif
void fnr_esp_transport_init(void);
void fnr_esp_transport_task(void);
void fnr_esp_transport_snapshot(fnr_transport_tx_counters *out);
#ifdef __cplusplus
}
#endif
#endif
