#include "provisioning_adapter.h"

#include "fnr/provisioning_image.h"
#include "provisioning_image.h"
#include "radio_adapter.h"

#include <cstring>

static fnr_esp_provisioning_status status = {};

static void zero_secret(fnr_radio_provisioning *provisioning) {
    if (provisioning == nullptr) return;
    volatile uint8_t *bytes = provisioning->key;
    for (size_t i = 0U; i < FNR_ESPNOW_KEY_SIZE; ++i) bytes[i] = 0U;
}

static bool key_valid(const uint8_t *key, size_t size) {
    if (key == nullptr || size != FNR_PROVISIONING_RADIO_KEY_SIZE) return false;
    uint8_t any = 0U;
    for (size_t i = 0U; i < size; ++i) any |= key[i];
    return any != 0U;
}

bool fnr_esp_provisioning_init(void) {
    std::memset(&status, 0, sizeof(status));

    size_t public_size = 0U;
    const uint8_t *public_bytes = fnr_provisioning_image_public(&public_size);
    status.record_result =
        fnr_provisioning_decode(public_bytes, public_size, &status.config);

    size_t key_size = 0U;
    const uint8_t *key = fnr_provisioning_image_radio_key(&key_size);
    status.key_present = key_valid(key, key_size);

    if (status.record_result != FNR_PROVISIONING_OK || !status.key_present) {
        if (status.record_result == FNR_PROVISIONING_OK)
            status.record_result = FNR_PROVISIONING_INVALID_FIELDS;
        status.radio_ready = fnr_radio_apply_provisioning(nullptr);
        fnr_radio_station_mac(status.station_mac);
        return !status.radio_ready;
    }

    fnr_radio_provisioning provisioning = {};
    provisioning.ingress.enabled = true;
    provisioning.ingress.channel = status.config.channel;
    std::memcpy(provisioning.ingress.allowed_mac,
                status.config.peer_mac, FNR_MAC_SIZE);
    provisioning.key_present = true;
    std::memcpy(provisioning.key, key, FNR_ESPNOW_KEY_SIZE);
    provisioning.config_id = status.config.config_id;

    status.radio_ready = fnr_radio_apply_provisioning(&provisioning);
    zero_secret(&provisioning);
    fnr_radio_station_mac(status.station_mac);
    return true;
}

void fnr_esp_provisioning_snapshot(fnr_esp_provisioning_status *out) {
    if (out != nullptr) *out = status;
}
