#include "fnr/provisioning.h"
#include "fnr/profile.h"
#include "fnr/profile_relative_mouse.h"
#include "../../firmware/esp8266/radio_adapter.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

static std::array<uint8_t, FNR_PROVISIONING_RECORD_SIZE> public_record{};
static size_t fake_public_size;
static std::array<uint8_t, FNR_PROVISIONING_RADIO_KEY_SIZE> secret_key{};
static size_t fake_key_size;
static unsigned apply_calls;
static bool apply_ready;
static bool saw_null;
static fnr_radio_provisioning applied{};

extern "C" const uint8_t *fnr_provisioning_image_public(size_t *size) {
    if (size != nullptr) *size = fake_public_size;
    return fake_public_size == 0U ? nullptr : public_record.data();
}

extern "C" const uint8_t *fnr_provisioning_image_radio_key(size_t *size) {
    if (size != nullptr) *size = fake_key_size;
    return fake_key_size == 0U ? nullptr : secret_key.data();
}

extern "C" bool fnr_radio_apply_provisioning(
    const fnr_radio_provisioning *provisioning) {
    ++apply_calls;
    saw_null = provisioning == nullptr;
    if (provisioning != nullptr) applied = *provisioning;
    return provisioning != nullptr && apply_ready;
}

extern "C" void fnr_radio_station_mac(uint8_t out[FNR_MAC_SIZE]) {
    const uint8_t mac[FNR_MAC_SIZE] = {0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    if (out != nullptr) std::memcpy(out, mac, sizeof(mac));
}

#include "../../firmware/esp8266/provisioning_adapter.cpp"

static fnr_provisioning_config fixture() {
    fnr_provisioning_config c{};
    c.provisioned = true;
    c.encryption_required = true;
    c.channel = 11U;
    c.platform_version = FNR_PLATFORM_VERSION;
    c.profile_id = FNR_PROFILE_ID_RELATIVE_MOUSE;
    c.profile_schema_version = FNR_RELATIVE_MOUSE_SCHEMA_VERSION;
    const uint8_t peer[FNR_MAC_SIZE] = {2,3,4,5,6,7};
    std::memcpy(c.peer_mac, peer, sizeof(peer));
    c.freshness_timeout_ms = 250U;
    c.config_id = UINT64_C(0x123456789abcdef0);
    c.generation = 4U;
    return c;
}

static void reset_fake() {
    public_record.fill(0U);
    fake_public_size = 0U;
    secret_key.fill(0U);
    fake_key_size = 0U;
    apply_calls = 0U;
    apply_ready = true;
    saw_null = false;
    applied = fnr_radio_provisioning{};
}

static void test_factory_and_missing_key_revoke() {
    reset_fake();
    assert(fnr_esp_provisioning_init());
    assert(apply_calls == 1U && saw_null);
    fnr_esp_provisioning_status s{};
    fnr_esp_provisioning_snapshot(&s);
    assert(s.record_result == FNR_PROVISIONING_EMPTY && !s.key_present &&
           !s.radio_ready);

    reset_fake();
    auto c = fixture();
    assert(fnr_provisioning_encode(&c, public_record.data()) == FNR_PROVISIONING_OK);
    fake_public_size = public_record.size();
    assert(fnr_esp_provisioning_init());
    assert(apply_calls == 1U && saw_null);
    fnr_esp_provisioning_snapshot(&s);
    assert(s.record_result == FNR_PROVISIONING_INVALID_FIELDS);
    assert(!s.key_present && !s.radio_ready);
}

static void test_valid_key_reaches_only_radio_adapter() {
    reset_fake();
    auto c = fixture();
    assert(fnr_provisioning_encode(&c, public_record.data()) == FNR_PROVISIONING_OK);
    fake_public_size = public_record.size();
    for (size_t i = 0U; i < secret_key.size(); ++i)
        secret_key[i] = static_cast<uint8_t>(i + 1U);
    fake_key_size = secret_key.size();

    assert(fnr_esp_provisioning_init());
    assert(apply_calls == 1U && !saw_null);
    assert(applied.ingress.enabled && applied.ingress.channel == 11U);
    assert(std::memcmp(applied.ingress.allowed_mac, c.peer_mac, FNR_MAC_SIZE) == 0);
    assert(applied.key_present && applied.config_id == c.config_id);
    assert(applied.generation == c.generation);
    assert(std::memcmp(applied.key, secret_key.data(), secret_key.size()) == 0);

    fnr_esp_provisioning_status s{};
    fnr_esp_provisioning_snapshot(&s);
    assert(s.record_result == FNR_PROVISIONING_OK && s.key_present && s.radio_ready);
    assert(s.config.config_id == c.config_id);
    const uint8_t station[FNR_MAC_SIZE] = {0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    assert(std::memcmp(s.station_mac, station, sizeof(station)) == 0);
}

int main() {
    test_factory_and_missing_key_revoke();
    test_valid_key_reaches_only_radio_adapter();
    std::puts("PASS: ESP provisioning adapter requires secret and exposes only redacted state");
    return 0;
}
