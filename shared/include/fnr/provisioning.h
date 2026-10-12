#ifndef FNR_PROVISIONING_H
#define FNR_PROVISIONING_H

#include "fnr/receiver.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    FNR_PROVISIONING_VERSION = 1,
    FNR_PROVISIONING_RECORD_SIZE = 40,
    FNR_PROVISIONING_RADIO_KEY_SIZE = 16,
    FNR_PROVISIONING_FLAG_PROVISIONED = 1u << 0,
    FNR_PROVISIONING_FLAG_ENCRYPTION_REQUIRED = 1u << 1
};

typedef enum fnr_provisioning_result {
    FNR_PROVISIONING_OK = 0,
    FNR_PROVISIONING_EMPTY,
    FNR_PROVISIONING_INVALID_ARGUMENT,
    FNR_PROVISIONING_INVALID_LENGTH,
    FNR_PROVISIONING_BAD_MAGIC,
    FNR_PROVISIONING_UNSUPPORTED_VERSION,
    FNR_PROVISIONING_BAD_FLAGS,
    FNR_PROVISIONING_BAD_CRC,
    FNR_PROVISIONING_INVALID_FIELDS
} fnr_provisioning_result;

typedef struct fnr_provisioning_config {
    bool provisioned;
    bool encryption_required;
    uint8_t channel;
    uint8_t platform_version;
    uint8_t profile_id;
    uint8_t profile_schema_version;
    uint8_t peer_mac[FNR_MAC_SIZE];
    uint32_t freshness_timeout_ms;
    uint64_t config_id;
    uint32_t generation;
} fnr_provisioning_config;

uint32_t fnr_provisioning_crc32(const uint8_t *bytes, size_t length);
bool fnr_provisioning_config_valid(const fnr_provisioning_config *config);
fnr_provisioning_result fnr_provisioning_encode(
    const fnr_provisioning_config *config,
    uint8_t out[FNR_PROVISIONING_RECORD_SIZE]);
fnr_provisioning_result fnr_provisioning_decode(
    const uint8_t *bytes, size_t length, fnr_provisioning_config *out);
bool fnr_provisioning_to_receiver_config(
    const fnr_provisioning_config *provisioning,
    fnr_receiver_config *receiver);

#ifdef __cplusplus
}
#endif
#endif
