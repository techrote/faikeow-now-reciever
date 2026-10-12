#include "provisioning_adapter.h"

#include "fnr/provisioning_image.h"
#include "receiver_adapter.h"

#include <string.h>

static fnr_rp_provisioning_status status;

bool fnr_rp_provisioning_init(void) {
    memset(&status, 0, sizeof(status));
    size_t size = 0U;
    const uint8_t *bytes = fnr_provisioning_image_public(&size);
    status.record_result = fnr_provisioning_decode(bytes, size, &status.config);

    if (status.record_result != FNR_PROVISIONING_OK) {
        status.staged = false;
        return fnr_receiver_adapter_revoke_provisioning();
    }

    fnr_receiver_config receiver;
    fnr_receiver_config_default(&receiver);
    receiver.peer_enabled = true;
    memcpy(receiver.peer_mac, status.config.peer_mac, FNR_MAC_SIZE);
    receiver.platform_version = status.config.platform_version;
    receiver.profile_enabled = true;
    receiver.profile_id = status.config.profile_id;
    receiver.freshness_timeout_ms = status.config.freshness_timeout_ms;
    if (!fnr_receiver_config_valid(&receiver)) {
        status.record_result = FNR_PROVISIONING_INVALID_FIELDS;
        status.staged = false;
        return fnr_receiver_adapter_revoke_provisioning();
    }

    status.staged = fnr_receiver_adapter_stage_provisioning(
        &receiver, status.config.channel);
    if (!status.staged) {
        status.record_result = FNR_PROVISIONING_INVALID_FIELDS;
        (void)fnr_receiver_adapter_revoke_provisioning();
    }
    return status.staged;
}

void fnr_rp_provisioning_snapshot(fnr_rp_provisioning_status *out) {
    if (out == NULL) return;
    *out = status;
    fnr_receiver_provisioning_status receiver_status;
    fnr_receiver_adapter_provisioning_snapshot(&receiver_status);
    out->radio_acknowledged = receiver_status.radio_acknowledged;
}
