#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
#include <espnow.h>
}

#include <cstring>
#include <type_traits>

#include "radio_adapter.h"

using expected_rx = void (*)(uint8_t *, uint8_t *, uint8_t);
static_assert(std::is_same<esp_now_recv_cb_t, expected_rx>::value,
              "Pinned ESP8266 NONOS SDK receive callback ABI changed");

/* ESP8266 NONOS invokes Wi-Fi callbacks from its cooperative SDK context.
 * The foreground never yields inside a critical copy; interrupt masking
 * additionally excludes ISR interruption. */
static fnr_radio_ingress ingress = {};
static bool now_active = false;
static fnr_radio_config runtime_config = {};
static bool runtime_config_present = false;
static bool runtime_encrypted = false;
static uint8_t runtime_key[FNR_ESPNOW_KEY_SIZE] = {};
static uint64_t runtime_config_id = 0U;
static uint32_t lifecycle_generation = 0; /* foreground owner only */

static void rx(uint8_t *mac, uint8_t *bytes, uint8_t length) {
    noInterrupts();
    fnr_radio_receive(&ingress, mac, bytes, length);
    interrupts();
}

static void clear_runtime_secret(void) {
    volatile uint8_t *key = runtime_key;
    for (size_t i = 0U; i < sizeof(runtime_key); ++i) key[i] = 0U;
}

static bool provisioning_valid(const fnr_radio_provisioning *provisioning) {
    if (provisioning == nullptr ||
        !fnr_radio_config_valid(&provisioning->ingress) ||
        !provisioning->key_present || provisioning->config_id == 0U) {
        return false;
    }
    uint8_t any = 0U;
    for (size_t i = 0U; i < FNR_ESPNOW_KEY_SIZE; ++i)
        any |= provisioning->key[i];
    return any != 0U;
}

static void transition_error(void) {
    if (now_active) {
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        now_active = false;
    }
    noInterrupts();
    fnr_radio_transition(&ingress, FNR_RADIO_ERROR);
    interrupts();
}

void fnr_radio_start(void) {
    ++lifecycle_generation;
    if (now_active) {
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        now_active = false;
    }

    const fnr_radio_config cfg =
        runtime_config_present ? runtime_config : fnr_radio_config{};
    noInterrupts();
    fnr_radio_reset(&ingress, &cfg);
    interrupts();

    if (!fnr_radio_config_valid(&cfg)) {
        WiFi.mode(WIFI_OFF);
        return;
    }

    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.mode(WIFI_STA);
    wifi_station_set_auto_connect(0);
    wifi_station_disconnect();
    if (!wifi_set_channel(cfg.channel) || wifi_get_channel() != cfg.channel ||
        esp_now_init() != 0) {
        transition_error();
        return;
    }
    now_active = true;

    if (esp_now_set_self_role(ESP_NOW_ROLE_SLAVE) != 0) {
        transition_error();
        return;
    }

    if (runtime_encrypted) {
        if (esp_now_add_peer(runtime_config.allowed_mac,
                             ESP_NOW_ROLE_CONTROLLER,
                             runtime_config.channel,
                             runtime_key,
                             FNR_ESPNOW_KEY_SIZE) != 0 ||
            esp_now_is_peer_exist(runtime_config.allowed_mac) == 0 ||
            esp_now_get_peer_channel(runtime_config.allowed_mac) !=
                runtime_config.channel) {
            transition_error();
            return;
        }
    }

    if (esp_now_register_recv_cb(rx) != 0) {
        transition_error();
        return;
    }

    noInterrupts();
    fnr_radio_transition(&ingress, FNR_RADIO_READY);
    interrupts();
}

void fnr_radio_restart(void) { fnr_radio_start(); }

void fnr_radio_apply_config(const fnr_radio_config *config) {
    /* Explicit legacy diagnostic configuration. Production provisioning uses
     * fnr_radio_apply_provisioning and never falls back here. */
    runtime_config_present = false;
    runtime_encrypted = false;
    runtime_config_id = 0U;
    clear_runtime_secret();
    runtime_config = config != nullptr ? *config : fnr_radio_config{};
    runtime_config_present = config != nullptr;
    fnr_radio_restart();
}

bool fnr_radio_apply_provisioning(const fnr_radio_provisioning *provisioning) {
    runtime_config_present = false;
    runtime_encrypted = false;
    runtime_config_id = 0U;
    clear_runtime_secret();
    runtime_config = fnr_radio_config{};

    if (provisioning_valid(provisioning)) {
        runtime_config = provisioning->ingress;
        std::memcpy(runtime_key, provisioning->key, sizeof(runtime_key));
        runtime_config_present = true;
        runtime_encrypted = true;
        runtime_config_id = provisioning->config_id;
    }

    fnr_radio_restart();
    fnr_radio_ingress snapshot = {};
    fnr_radio_status_foreground(&snapshot);
    return runtime_config_present && runtime_encrypted &&
           snapshot.state == FNR_RADIO_READY;
}

void fnr_radio_provisioning_status_foreground(
    fnr_radio_provisioning_status *out) {
    if (out == nullptr) return;
    fnr_radio_ingress snapshot = {};
    fnr_radio_status_foreground(&snapshot);
    std::memset(out, 0, sizeof(*out));
    out->configured = runtime_config_present;
    out->encryption_enabled = runtime_config_present && runtime_encrypted;
    out->channel = runtime_config.channel;
    std::memcpy(out->peer_mac, runtime_config.allowed_mac, FNR_MAC_SIZE);
    out->config_id = runtime_config_id;
    out->state = snapshot.state;
}

bool fnr_radio_take_foreground(fnr_datagram *out) {
    noInterrupts();
    const bool ok = fnr_radio_take(&ingress, out);
    interrupts();
    return ok;
}

void fnr_radio_status_foreground(fnr_radio_ingress *out) {
    noInterrupts();
    fnr_radio_snapshot(&ingress, out);
    interrupts();
}

uint32_t fnr_radio_generation_foreground(void) { return lifecycle_generation; }

void fnr_radio_station_mac(uint8_t out[FNR_MAC_SIZE]) {
    if (out != nullptr) WiFi.macAddress(out);
}
