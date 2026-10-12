#ifndef FNR_RP_PROVISIONING_ADAPTER_H
#define FNR_RP_PROVISIONING_ADAPTER_H

#include "fnr/provisioning.h"

#include <stdbool.h>

typedef struct fnr_rp_provisioning_status {
    fnr_provisioning_result record_result;
    fnr_provisioning_config config;
    bool staged;
    bool radio_acknowledged;
} fnr_rp_provisioning_status;

/* Factory/unreadable/corrupt records are safe outcomes: the receiver remains
 * unprovisioned. False means the receiver adapter could not be placed safely. */
bool fnr_rp_provisioning_init(void);
void fnr_rp_provisioning_snapshot(fnr_rp_provisioning_status *out);

#endif
