#ifndef FNR_RP_RECEIVER_ADAPTER_H
#define FNR_RP_RECEIVER_ADAPTER_H

#include "fnr/hid.h"
#include "fnr/hid_relative_mouse.h"
#include "fnr/receiver.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct fnr_receiver_provisioning_status {
    bool staged;
    bool radio_acknowledged;
    uint8_t expected_channel;
} fnr_receiver_provisioning_status;

bool fnr_receiver_adapter_init(fnr_hid_device *hid,
                               fnr_relative_mouse_state *mouse);

/* Direct configuration remains a test/integration seam. Production FNR-007
 * uses stage_provisioning so peer/profile admission remains disabled until a
 * matching READY radio status has crossed the accepted FNR-005 link. */
bool fnr_receiver_adapter_configure(const fnr_receiver_config *config);
bool fnr_receiver_adapter_stage_provisioning(const fnr_receiver_config *config,
                                             uint8_t radio_channel);
bool fnr_receiver_adapter_revoke_provisioning(void);
void fnr_receiver_adapter_provisioning_snapshot(
    fnr_receiver_provisioning_status *out);
void fnr_receiver_adapter_snapshot(fnr_receiver_core *out);

#endif
