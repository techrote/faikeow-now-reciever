#include "fnr/provisioning.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)

static fnr_provisioning_config fixture() {
    fnr_provisioning_config config{};
    config.provisioned = true;
    config.encryption_required = true;
    config.channel = 6U;
    config.platform_version = FNR_PLATFORM_VERSION;
    config.profile_id = FNR_PROFILE_ID_RELATIVE_MOUSE;
    config.profile_schema_version = 1U;
    const uint8_t peer[FNR_MAC_SIZE] = {2, 3, 4, 5, 6, 7};
    std::memcpy(config.peer_mac, peer, sizeof(peer));
    config.freshness_timeout_ms = 250U;
    config.config_id = UINT64_C(0x0123456789abcdef);
    config.generation = 7U;
    return config;
}

static void test_crc_and_golden() {
    const uint8_t text[] = {'1','2','3','4','5','6','7','8','9'};
    CHECK(fnr_provisioning_crc32(text, sizeof(text)) == UINT32_C(0xcbf43926));

    const auto config = fixture();
    std::array<uint8_t, FNR_PROVISIONING_RECORD_SIZE> bytes{};
    CHECK(fnr_provisioning_encode(&config, bytes.data()) == FNR_PROVISIONING_OK);
    const std::array<uint8_t, FNR_PROVISIONING_RECORD_SIZE> golden{{
        0x46,0x4e,0x52,0x37,0x01,0x03,0x06,0x01,0x01,0x01,
        0x02,0x03,0x04,0x05,0x06,0x07,0xfa,0x00,0x00,0x00,
        0xef,0xcd,0xab,0x89,0x67,0x45,0x23,0x01,0x07,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x82,0xcf,0xc5,0x13
    }};
    CHECK(bytes == golden);

    fnr_provisioning_config decoded{};
    CHECK(fnr_provisioning_decode(bytes.data(), bytes.size(), &decoded) ==
          FNR_PROVISIONING_OK);
    CHECK(decoded.provisioned && decoded.encryption_required);
    CHECK(decoded.channel == 6U && decoded.platform_version == 1U);
    CHECK(decoded.profile_id == 1U && decoded.profile_schema_version == 1U);
    CHECK(decoded.freshness_timeout_ms == 250U);
    CHECK(decoded.config_id == UINT64_C(0x0123456789abcdef));
    CHECK(decoded.generation == 7U);
    CHECK(std::memcmp(decoded.peer_mac, config.peer_mac, FNR_MAC_SIZE) == 0);

    fnr_receiver_config receiver{};
    CHECK(fnr_provisioning_to_receiver_config(&decoded, &receiver));
    CHECK(receiver.peer_enabled && receiver.profile_enabled);
    CHECK(receiver.freshness_timeout_ms == 250U);
    CHECK(std::memcmp(receiver.peer_mac, config.peer_mac, FNR_MAC_SIZE) == 0);
}

static void test_corruption_and_fail_closed() {
    const auto config = fixture();
    std::array<uint8_t, FNR_PROVISIONING_RECORD_SIZE> bytes{};
    CHECK(fnr_provisioning_encode(&config, bytes.data()) == FNR_PROVISIONING_OK);

    for (std::size_t i = 0U; i < bytes.size(); ++i) {
        auto corrupt = bytes;
        corrupt[i] ^= 0x01U;
        fnr_provisioning_config out{};
        CHECK(fnr_provisioning_decode(corrupt.data(), corrupt.size(), &out) !=
              FNR_PROVISIONING_OK);
    }

    fnr_provisioning_config sentinel = fixture();
    sentinel.config_id = UINT64_C(0xaaaaaaaaaaaaaaaa);
    CHECK(fnr_provisioning_decode(nullptr, 0U, &sentinel) == FNR_PROVISIONING_EMPTY);
    CHECK(sentinel.config_id == UINT64_C(0xaaaaaaaaaaaaaaaa));
    CHECK(fnr_provisioning_decode(bytes.data(), bytes.size() - 1U, &sentinel) ==
          FNR_PROVISIONING_INVALID_LENGTH);
    CHECK(sentinel.config_id == UINT64_C(0xaaaaaaaaaaaaaaaa));

    auto invalid = config;
    invalid.encryption_required = false;
    std::array<uint8_t, FNR_PROVISIONING_RECORD_SIZE> untouched{};
    untouched.fill(0xa5U);
    CHECK(fnr_provisioning_encode(&invalid, untouched.data()) ==
          FNR_PROVISIONING_INVALID_FIELDS);
    for (uint8_t value : untouched) CHECK(value == 0xa5U);

    invalid = config; invalid.channel = 0U;
    CHECK(!fnr_provisioning_config_valid(&invalid));
    invalid = config; invalid.peer_mac[0] |= 1U;
    CHECK(!fnr_provisioning_config_valid(&invalid));
    invalid = config; invalid.config_id = 0U;
    CHECK(!fnr_provisioning_config_valid(&invalid));
    invalid = config; invalid.generation = 0U;
    CHECK(!fnr_provisioning_config_valid(&invalid));
}

static uint32_t rng_state = UINT32_C(0x7a5b3c1d);
static uint32_t rng32() {
    uint32_t x = rng_state;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    rng_state = x;
    return x;
}

static void test_20000_deterministic_codec_cases() {
    std::array<uint8_t, FNR_PROVISIONING_RECORD_SIZE> bytes{};
    for (unsigned iteration = 0U; iteration < 10000U; ++iteration) {
        auto config = fixture();
        config.channel = static_cast<uint8_t>(1U + rng32() % 14U);
        config.peer_mac[0] = static_cast<uint8_t>((rng32() & 0xfcU) | 0x02U);
        for (std::size_t i = 1U; i < FNR_MAC_SIZE; ++i)
            config.peer_mac[i] = static_cast<uint8_t>(rng32());
        config.freshness_timeout_ms = 1U + rng32() % UINT32_C(1000000);
        config.config_id = (static_cast<uint64_t>(rng32()) << 32U) | rng32();
        if (config.config_id == 0U) config.config_id = 1U;
        config.generation = rng32();
        if (config.generation == 0U) config.generation = 1U;

        CHECK(fnr_provisioning_encode(&config, bytes.data()) == FNR_PROVISIONING_OK);
        fnr_provisioning_config decoded{};
        CHECK(fnr_provisioning_decode(bytes.data(), bytes.size(), &decoded) ==
              FNR_PROVISIONING_OK);
        CHECK(decoded.channel == config.channel);
        CHECK(decoded.config_id == config.config_id);
        CHECK(decoded.generation == config.generation);
        CHECK(std::memcmp(decoded.peer_mac, config.peer_mac, FNR_MAC_SIZE) == 0);
    }

    for (unsigned iteration = 0U; iteration < 10000U; ++iteration) {
        for (uint8_t &value : bytes) value = static_cast<uint8_t>(rng32());
        fnr_provisioning_config decoded{};
        const auto result =
            fnr_provisioning_decode(bytes.data(), bytes.size(), &decoded);
        if (result == FNR_PROVISIONING_OK) {
            std::array<uint8_t, FNR_PROVISIONING_RECORD_SIZE> canonical{};
            CHECK(fnr_provisioning_encode(&decoded, canonical.data()) ==
                  FNR_PROVISIONING_OK);
            CHECK(canonical == bytes);
        }
    }
}

int main() {
    test_crc_and_golden();
    test_corruption_and_fail_closed();
    test_20000_deterministic_codec_cases();
    std::puts("PASS: FNR-007 provisioning record, corruption and 20000 deterministic codec cases");
    return 0;
}
