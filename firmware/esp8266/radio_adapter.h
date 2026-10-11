#ifndef FNR_RADIO_ADAPTER_H
#define FNR_RADIO_ADAPTER_H
#include "fnr/radio_ingress.h"
#ifdef __cplusplus
extern "C" {
#endif
/* No implicit provisioning: build defaults to disabled receiver. */
void fnr_radio_start(void);
void fnr_radio_restart(void);
/* Foreground-only, copied configuration; null revokes admission. */
void fnr_radio_apply_config(const fnr_radio_config *config);
bool fnr_radio_take_foreground(fnr_datagram *out);
void fnr_radio_status_foreground(fnr_radio_ingress *out);
void fnr_radio_station_mac(uint8_t out[FNR_MAC_SIZE]);
#ifdef __cplusplus
}
#endif
#endif
