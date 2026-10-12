#include "fnr/provisioning.h"
#include "fnr/profile_relative_mouse.h"

#include <string.h>

enum {
    MAGIC_0 = 'F', MAGIC_1 = 'N', MAGIC_2 = 'R', MAGIC_3 = '7',
    FLAGS_OFFSET = 5,
    CHANNEL_OFFSET = 6,
    PLATFORM_OFFSET = 7,
    PROFILE_OFFSET = 8,
    PROFILE_SCHEMA_OFFSET = 9,
    PEER_OFFSET = 10,
    FRESHNESS_OFFSET = 16,
    CONFIG_ID_OFFSET = 20,
    GENERATION_OFFSET = 28,
    RESERVED_OFFSET = 32,
    CRC_OFFSET = 36
};

_Static_assert(FNR_PROVISIONING_RECORD_SIZE == 40,
               "FNR-007 provisioning record size is frozen");
_Static_assert(FNR_PROVISIONING_RECORD_SIZE == CRC_OFFSET + 4,
               "FNR-007 provisioning offsets drifted");

static uint32_t read_u32_le(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64_le(const uint8_t *bytes) {
    uint64_t value = 0U;
    for (unsigned i = 0U; i < 8U; ++i)
        value |= (uint64_t)bytes[i] << (8U * i);
    return value;
}

static void write_u32_le(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static void write_u64_le(uint8_t *bytes, uint64_t value) {
    for (unsigned i = 0U; i < 8U; ++i)
        bytes[i] = (uint8_t)(value >> (8U * i));
}

static bool unicast_nonzero_mac(const uint8_t mac[FNR_MAC_SIZE]) {
    uint8_t any = 0U;
    for (size_t i = 0U; i < FNR_MAC_SIZE; ++i)
        any |= mac[i];
    return any != 0U && (mac[0] & 1U) == 0U;
}

uint32_t fnr_provisioning_crc32(const uint8_t *bytes, size_t length) {
    if (bytes == NULL && length != 0U) return 0U;
    uint32_t crc = UINT32_C(0xffffffff);
    for (size_t i = 0U; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0U; bit < 8U; ++bit)
            crc = (crc >> 1U) ^ ((crc & 1U) != 0U ? UINT32_C(0xedb88320) : 0U);
    }
    return crc ^ UINT32_C(0xffffffff);
}

bool fnr_provisioning_config_valid(const fnr_provisioning_config *config) {
    if (config == NULL || !config->provisioned ||
        !config->encryption_required || config->channel < 1U ||
        config->channel > 14U || !unicast_nonzero_mac(config->peer_mac) ||
        config->platform_version != FNR_PLATFORM_VERSION ||
        config->profile_id == 0U || config->profile_schema_version == 0U ||
        config->freshness_timeout_ms == 0U ||
        config->freshness_timeout_ms >= UINT32_C(0x80000000) ||
        config->config_id == 0U || config->generation == 0U) {
        return false;
    }
    return true;
}

fnr_provisioning_result fnr_provisioning_encode(
    const fnr_provisioning_config *config,
    uint8_t out[FNR_PROVISIONING_RECORD_SIZE]) {
    if (config == NULL || out == NULL) return FNR_PROVISIONING_INVALID_ARGUMENT;
    if (!fnr_provisioning_config_valid(config))
        return FNR_PROVISIONING_INVALID_FIELDS;

    memset(out, 0, FNR_PROVISIONING_RECORD_SIZE);
    out[0] = MAGIC_0;
    out[1] = MAGIC_1;
    out[2] = MAGIC_2;
    out[3] = MAGIC_3;
    out[4] = FNR_PROVISIONING_VERSION;
    out[FLAGS_OFFSET] = FNR_PROVISIONING_FLAG_PROVISIONED |
                        FNR_PROVISIONING_FLAG_ENCRYPTION_REQUIRED;
    out[CHANNEL_OFFSET] = config->channel;
    out[PLATFORM_OFFSET] = config->platform_version;
    out[PROFILE_OFFSET] = config->profile_id;
    out[PROFILE_SCHEMA_OFFSET] = config->profile_schema_version;
    memcpy(out + PEER_OFFSET, config->peer_mac, FNR_MAC_SIZE);
    write_u32_le(out + FRESHNESS_OFFSET, config->freshness_timeout_ms);
    write_u64_le(out + CONFIG_ID_OFFSET, config->config_id);
    write_u32_le(out + GENERATION_OFFSET, config->generation);
    write_u32_le(out + CRC_OFFSET,
                 fnr_provisioning_crc32(out, CRC_OFFSET));
    return FNR_PROVISIONING_OK;
}

fnr_provisioning_result fnr_provisioning_decode(
    const uint8_t *bytes, size_t length, fnr_provisioning_config *out) {
    if (out == NULL) return FNR_PROVISIONING_INVALID_ARGUMENT;
    if (bytes == NULL || length == 0U) return FNR_PROVISIONING_EMPTY;
    if (length != FNR_PROVISIONING_RECORD_SIZE)
        return FNR_PROVISIONING_INVALID_LENGTH;
    if (bytes[0] != MAGIC_0 || bytes[1] != MAGIC_1 ||
        bytes[2] != MAGIC_2 || bytes[3] != MAGIC_3)
        return FNR_PROVISIONING_BAD_MAGIC;
    if (bytes[4] != FNR_PROVISIONING_VERSION)
        return FNR_PROVISIONING_UNSUPPORTED_VERSION;
    const uint8_t expected_flags = FNR_PROVISIONING_FLAG_PROVISIONED |
                                   FNR_PROVISIONING_FLAG_ENCRYPTION_REQUIRED;
    if (bytes[FLAGS_OFFSET] != expected_flags)
        return FNR_PROVISIONING_BAD_FLAGS;
    for (size_t i = RESERVED_OFFSET; i < CRC_OFFSET; ++i)
        if (bytes[i] != 0U) return FNR_PROVISIONING_INVALID_FIELDS;
    if (read_u32_le(bytes + CRC_OFFSET) !=
        fnr_provisioning_crc32(bytes, CRC_OFFSET))
        return FNR_PROVISIONING_BAD_CRC;

    fnr_provisioning_config candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.provisioned = true;
    candidate.encryption_required = true;
    candidate.channel = bytes[CHANNEL_OFFSET];
    candidate.platform_version = bytes[PLATFORM_OFFSET];
    candidate.profile_id = bytes[PROFILE_OFFSET];
    candidate.profile_schema_version = bytes[PROFILE_SCHEMA_OFFSET];
    memcpy(candidate.peer_mac, bytes + PEER_OFFSET, FNR_MAC_SIZE);
    candidate.freshness_timeout_ms = read_u32_le(bytes + FRESHNESS_OFFSET);
    candidate.config_id = read_u64_le(bytes + CONFIG_ID_OFFSET);
    candidate.generation = read_u32_le(bytes + GENERATION_OFFSET);
    if (!fnr_provisioning_config_valid(&candidate))
        return FNR_PROVISIONING_INVALID_FIELDS;
    *out = candidate;
    return FNR_PROVISIONING_OK;
}
