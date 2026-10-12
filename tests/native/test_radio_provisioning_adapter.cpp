#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
#include <espnow.h>
#include <user_interface.h>
}

#include "fnr/radio_ingress.h"
#include "fnr/radio_auth.h"
#include "../../firmware/esp8266/radio_adapter.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

struct FakeState {
    bool persistent{};
    bool auto_reconnect{};
    int wifi_mode{-1};
    uint8_t channel{};
    unsigned station_auto_calls{};
    unsigned station_disconnect_calls{};
    unsigned init_calls{};
    unsigned deinit_calls{};
    unsigned set_role_calls{};
    unsigned register_calls{};
    unsigned unregister_calls{};
    unsigned add_peer_calls{};
    int init_result{};
    int set_role_result{};
    int register_result{};
    int add_peer_result{};
    bool peer_exists{true};
    int peer_channel{6};
    uint8_t peer_role{};
    uint8_t peer_mac[FNR_MAC_SIZE]{};
    uint8_t peer_key[FNR_ESPNOW_KEY_SIZE]{};
    uint8_t peer_key_len{};
    esp_now_recv_cb_t recv_cb{};
    uint8_t station_mac[FNR_MAC_SIZE]{0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
} fake;

FnrFakeWiFi WiFi;
FnrFakeSerial Serial;
FnrFakeESP ESP;

void FnrFakeWiFi::persistent(bool enabled) { fake.persistent = enabled; }
void FnrFakeWiFi::setAutoReconnect(bool enabled) { fake.auto_reconnect = enabled; }
void FnrFakeWiFi::mode(int value) { fake.wifi_mode = value; }
void FnrFakeWiFi::macAddress(uint8_t *out) {
    if (out != nullptr) std::memcpy(out, fake.station_mac, FNR_MAC_SIZE);
}

void FnrFakeSerial::begin(unsigned long, int, int) {}
void FnrFakeSerial::setDebugOutput(bool) {}
int FnrFakeSerial::availableForWrite() { return 128; }
size_t FnrFakeSerial::write(const uint8_t *, size_t length) { return length; }
uint32_t FnrFakeESP::random() { return 1U; }
uint32_t millis() { return 0U; }
void noInterrupts() {}
void interrupts() {}

extern "C" void system_set_os_print(uint8_t) {}
extern "C" void wifi_station_set_auto_connect(uint8_t) { ++fake.station_auto_calls; }
extern "C" void wifi_station_disconnect(void) { ++fake.station_disconnect_calls; }
extern "C" bool wifi_set_channel(uint8_t channel) {
    fake.channel = channel;
    return true;
}
extern "C" uint8_t wifi_get_channel(void) { return fake.channel; }

extern "C" int esp_now_init(void) {
    ++fake.init_calls;
    return fake.init_result;
}
extern "C" int esp_now_deinit(void) {
    ++fake.deinit_calls;
    fake.recv_cb = nullptr;
    return 0;
}
extern "C" int esp_now_set_self_role(uint8_t role) {
    ++fake.set_role_calls;
    fake.peer_role = role;
    return fake.set_role_result;
}
extern "C" int esp_now_register_recv_cb(esp_now_recv_cb_t cb) {
    ++fake.register_calls;
    if (fake.register_result == 0) fake.recv_cb = cb;
    return fake.register_result;
}
extern "C" int esp_now_unregister_recv_cb(void) {
    ++fake.unregister_calls;
    fake.recv_cb = nullptr;
    return 0;
}
extern "C" int esp_now_add_peer(uint8_t *mac_addr, uint8_t role,
                                uint8_t channel, uint8_t *key,
                                uint8_t key_len) {
    ++fake.add_peer_calls;
    fake.peer_role = role;
    fake.peer_channel = channel;
    fake.peer_key_len = key_len;
    if (mac_addr != nullptr) std::memcpy(fake.peer_mac, mac_addr, FNR_MAC_SIZE);
    if (key != nullptr && key_len <= FNR_ESPNOW_KEY_SIZE)
        std::memcpy(fake.peer_key, key, key_len);
    return fake.add_peer_result;
}
extern "C" int esp_now_is_peer_exist(uint8_t *) { return fake.peer_exists ? 1 : 0; }
extern "C" int esp_now_get_peer_channel(uint8_t *) { return fake.peer_channel; }

/* Deterministic keyed provider seam; actual BearSSL has an RFC 4231 target
 * self-test before READY. This test exercises the production callback gate. */
bool fnr_auth_crypto_selftest(void) { return true; }
bool fnr_auth_crypto_hmac(void *key, const uint8_t *message, size_t size,
                          uint8_t digest[32]) {
    uint32_t hash = UINT32_C(2166136261);
    const auto *bytes = static_cast<const uint8_t *>(key);
    for (size_t i = 0; i < 16; ++i) hash = (hash ^ bytes[i]) * UINT32_C(16777619);
    for (size_t i = 0; i < size; ++i)
        hash = (hash ^ message[i]) * UINT32_C(16777619);
    for (size_t i = 0; i < 32; ++i)
        digest[i] = static_cast<uint8_t>(hash >> (8U * (i % 4)));
    return true;
}
#include "../../firmware/esp8266/radio_adapter.cpp"

static fnr_radio_provisioning valid_provisioning() {
    fnr_radio_provisioning p{};
    p.ingress.enabled = true;
    p.ingress.channel = 6U;
    const uint8_t peer[FNR_MAC_SIZE] = {2,3,4,5,6,7};
    std::memcpy(p.ingress.allowed_mac, peer, sizeof(peer));
    p.key_present = true;
    for (size_t i = 0U; i < FNR_ESPNOW_KEY_SIZE; ++i)
        p.key[i] = static_cast<uint8_t>(i + 1U);
    p.config_id = UINT64_C(0x123456789abcdef0);
    p.generation = 1;
    return p;
}

static void reset_fake() {
    fake = FakeState{};
    fake.peer_exists = true;
    fake.peer_channel = 6;
}

static void test_factory_is_off() {
    reset_fake();
    fnr_radio_apply_config(nullptr);
    fnr_radio_ingress s{};
    fnr_radio_status_foreground(&s);
    assert(s.state == FNR_RADIO_UNCONFIGURED);
    assert(fake.wifi_mode == WIFI_OFF);
    assert(fake.init_calls == 0U && fake.add_peer_calls == 0U);
}

static void test_encrypted_provisioning_and_receive() {
    reset_fake();
    const auto p = valid_provisioning();
    assert(fnr_radio_apply_provisioning(&p));
    assert(fake.wifi_mode == WIFI_STA);
    assert(fake.channel == 6U);
    assert(fake.init_calls == 1U && fake.set_role_calls == 1U);
    assert(fake.add_peer_calls == 1U);
    assert(fake.peer_role == ESP_NOW_ROLE_CONTROLLER);
    assert(fake.peer_key_len == FNR_ESPNOW_KEY_SIZE);
    assert(std::memcmp(fake.peer_mac, p.ingress.allowed_mac, FNR_MAC_SIZE) == 0);
    assert(std::memcmp(fake.peer_key, p.key, FNR_ESPNOW_KEY_SIZE) == 0);
    assert(fake.register_calls == 1U && fake.recv_cb != nullptr);

    fnr_radio_provisioning_status status{};
    fnr_radio_provisioning_status_foreground(&status);
    assert(status.configured && status.encryption_enabled);
    assert(status.state == FNR_RADIO_READY && status.channel == 6U);
    assert(status.config_id == p.config_id);

    uint8_t payload[3] = {1,2,3};
    uint8_t peer[FNR_MAC_SIZE];
    std::memcpy(peer, p.ingress.allowed_mac, sizeof(peer));
    fake.recv_cb(peer, payload, sizeof(payload));
    fnr_datagram d{};
    /* Physical regression: SDK callbacks can deliver plaintext from the
     * configured MAC even when that peer was registered with an ESP-NOW key.
     * A bare platform packet must never reach the production transport. */
    assert(!fnr_radio_take_foreground(&d));
    fnr_auth_context c{};
    c.config_id = p.config_id; c.generation = p.generation;
    std::memcpy(c.sender, peer, 6);
    std::memcpy(c.receiver, fake.station_mac, 6);
    uint8_t wire[250]; size_t wire_size = 0;
    auto key = p.key;
    uint8_t key_copy[16]; std::memcpy(key_copy, key, 16);
    assert(fnr_auth_wrap(&c, fnr_auth_crypto_hmac, key_copy, payload,
                         sizeof(payload), wire, sizeof(wire), &wire_size));
    fake.recv_cb(peer, wire, static_cast<uint8_t>(wire_size));
    assert(fnr_radio_take_foreground(&d) && d.size == sizeof(payload));
    assert(std::memcmp(d.payload, payload, sizeof(payload)) == 0);
    for (size_t i = 0; i < wire_size; ++i) {
        wire[i] ^= 1;
        fake.recv_cb(peer, wire, static_cast<uint8_t>(wire_size));
        assert(!fnr_radio_take_foreground(&d));
        wire[i] ^= 1;
    }
    fnr_radio_provisioning_status_foreground(&status);
    assert(status.authentication_enabled &&
           status.authentication_rejected == wire_size + 1);
}

static void test_missing_key_and_peer_failure_fail_closed() {
    reset_fake();
    auto p = valid_provisioning();
    p.key_present = false;
    assert(!fnr_radio_apply_provisioning(&p));
    assert(fake.init_calls == 0U && fake.add_peer_calls == 0U);
    fnr_radio_ingress s{};
    fnr_radio_status_foreground(&s);
    assert(s.state == FNR_RADIO_UNCONFIGURED);

    reset_fake();
    p = valid_provisioning();
    fake.add_peer_result = -1;
    assert(!fnr_radio_apply_provisioning(&p));
    fnr_radio_status_foreground(&s);
    assert(s.state == FNR_RADIO_ERROR);
    assert(fake.add_peer_calls == 1U);
    assert(fake.register_calls == 0U);
    assert(fake.deinit_calls >= 1U);
}

static void test_legacy_diagnostic_plaintext_is_explicit_only() {
    reset_fake();
    fnr_radio_config c{};
    c.enabled = true;
    c.channel = 6U;
    const uint8_t peer[FNR_MAC_SIZE] = {2,3,4,5,6,7};
    std::memcpy(c.allowed_mac, peer, sizeof(peer));
    fnr_radio_apply_config(&c);
    fnr_radio_ingress s{};
    fnr_radio_status_foreground(&s);
    assert(s.state == FNR_RADIO_READY);
    assert(fake.add_peer_calls == 0U);
    fnr_radio_provisioning_status ps{};
    fnr_radio_provisioning_status_foreground(&ps);
    assert(ps.configured && !ps.encryption_enabled && ps.config_id == 0U);
}

static void test_auth_bounds_and_identity_binding() {
    auto p = valid_provisioning();
    fnr_auth_context c{};
    c.config_id = p.config_id; c.generation = p.generation;
    std::memcpy(c.sender, p.ingress.allowed_mac, 6);
    std::memcpy(c.receiver, fake.station_mac, 6);
    uint8_t payload[229]{};
    uint8_t wire[251]{};
    for (size_t size = 1; size <= FNR_AUTH_INNER_MAX; ++size) {
        size_t written = 999;
        assert(fnr_auth_wrap(&c, fnr_auth_crypto_hmac, p.key, payload, size,
                             wire, sizeof(wire), &written));
        assert(written == size + FNR_AUTH_OVERHEAD && written <= 250);
        const uint8_t *inner = nullptr; size_t inner_size = 999;
        assert(fnr_auth_unwrap(&c, fnr_auth_crypto_hmac, p.key, wire, written,
                               &inner, &inner_size));
        assert(inner_size == size && std::memcmp(inner, payload, size) == 0);
        for (size_t truncated = 0; truncated < written; ++truncated) {
            assert(!fnr_auth_unwrap(&c, fnr_auth_crypto_hmac, p.key, wire,
                                    truncated, &inner, &inner_size));
            assert(inner == nullptr && inner_size == 0);
        }
    }
    size_t written = 999;
    assert(!fnr_auth_wrap(&c, fnr_auth_crypto_hmac, p.key, payload, 229,
                          wire, sizeof(wire), &written) && written == 0);
    assert(!fnr_auth_wrap(&c, fnr_auth_crypto_hmac, p.key, payload, 0,
                          wire, sizeof(wire), &written));
    assert(!fnr_auth_wrap(&c, fnr_auth_crypto_hmac, p.key, payload, 228,
                          wire, 249, &written));
    assert(fnr_auth_wrap(&c, fnr_auth_crypto_hmac, p.key, payload, 228,
                         wire, 250, &written));
    const uint8_t *inner; size_t inner_size;
    for (unsigned field = 0; field < 4; ++field) {
        auto wrong = c;
        if (field == 0) ++wrong.config_id;
        if (field == 1) ++wrong.generation;
        if (field == 2) wrong.sender[5] ^= 1;
        if (field == 3) wrong.receiver[5] ^= 1;
        assert(!fnr_auth_unwrap(&wrong, fnr_auth_crypto_hmac, p.key, wire,
                                written, &inner, &inner_size));
    }
    p.key[0] ^= 1;
    assert(!fnr_auth_unwrap(&c, fnr_auth_crypto_hmac, p.key, wire, written,
                            &inner, &inner_size));
    assert(!fnr_auth_unwrap(&c, nullptr, p.key, wire, written,
                            &inner, &inner_size));
    assert(!fnr_auth_unwrap(&c, fnr_auth_crypto_hmac, p.key, wire, 251,
                            &inner, &inner_size));
}

int main() {
    test_factory_is_off();
    test_encrypted_provisioning_and_receive();
    test_missing_key_and_peer_failure_fail_closed();
    test_legacy_diagnostic_plaintext_is_explicit_only();
    test_auth_bounds_and_identity_binding();
    std::puts("PASS: actual ESP radio adapter requires encrypted production peer and fails closed");
    return 0;
}
