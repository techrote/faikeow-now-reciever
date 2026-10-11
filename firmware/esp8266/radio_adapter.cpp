#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
#include <espnow.h>
}
#include <type_traits>
#include "radio_adapter.h"
using expected_rx = void (*)(uint8_t *, uint8_t *, uint8_t);
static_assert(std::is_same<esp_now_recv_cb_t, expected_rx>::value,
              "Pinned ESP8266 NONOS SDK receive callback ABI changed");
/* ESP8266 NONOS invokes Wi-Fi callbacks from its cooperative SDK context.
 * The foreground never yields inside a critical copy; interrupt masking
 * additionally excludes ISR interruption. The SDK does not formally promise
 * that callbacks cannot be reentered; this is a conservative single-core
 * serialization assumption, requiring hardware stress confirmation. */
static fnr_radio_ingress ingress = {};
static bool now_active = false;
static fnr_radio_config runtime_config = {};
static volatile bool runtime_config_present = false;
static void rx(uint8_t *mac, uint8_t *bytes, uint8_t length) {
    noInterrupts();
    fnr_radio_receive(&ingress, mac, bytes, length);
    interrupts();
}
static fnr_radio_config configured() {
    if (runtime_config_present) return runtime_config;
    fnr_radio_config c = {};
#if defined(FNR_RADIO_ENABLE) && FNR_RADIO_ENABLE == 1 && \
    defined(FNR_PEER_0) && defined(FNR_PEER_1) && defined(FNR_PEER_2) && \
    defined(FNR_PEER_3) && defined(FNR_PEER_4) && defined(FNR_PEER_5) && \
    defined(FNR_CHANNEL)
    c.enabled = true;
    c.channel = FNR_CHANNEL;
    const uint8_t mac[FNR_MAC_SIZE] = {
        FNR_PEER_0,FNR_PEER_1,FNR_PEER_2,FNR_PEER_3,FNR_PEER_4,FNR_PEER_5
    };
    for (unsigned i=0;i<FNR_MAC_SIZE;++i) c.allowed_mac[i]=mac[i];
#endif
    return c;
}
void fnr_radio_start(void) {
    if (now_active) {
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        now_active = false;
    }
    const fnr_radio_config cfg = configured();
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
    if (!wifi_set_channel(cfg.channel) || wifi_get_channel()!=cfg.channel ||
        esp_now_init()!=0) {
        noInterrupts();
        fnr_radio_transition(&ingress,FNR_RADIO_ERROR);
        interrupts();
        return;
    }
    now_active = true;
    if (esp_now_set_self_role(ESP_NOW_ROLE_SLAVE)!=0 ||
        esp_now_register_recv_cb(rx)!=0) {
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        now_active = false;
        noInterrupts();
        fnr_radio_transition(&ingress,FNR_RADIO_ERROR);
        interrupts();
        return;
    }
    noInterrupts();
    fnr_radio_transition(&ingress,FNR_RADIO_READY);
    interrupts();
}
void fnr_radio_restart(void) { fnr_radio_start(); }
void fnr_radio_apply_config(const fnr_radio_config *config) {
    /* Called by foreground only. The restart retires the prior callback. */
    runtime_config_present = false;
    runtime_config = config ? *config : fnr_radio_config{};
    runtime_config_present = (config != nullptr);
    fnr_radio_restart();
}
bool fnr_radio_take_foreground(fnr_datagram *out) {
    noInterrupts();
    const bool ok=fnr_radio_take(&ingress,out);
    interrupts();
    return ok;
}
void fnr_radio_status_foreground(fnr_radio_ingress *out) {
    noInterrupts();
    fnr_radio_snapshot(&ingress,out);
    interrupts();
}
void fnr_radio_station_mac(uint8_t out[FNR_MAC_SIZE]) {
    if (out) WiFi.macAddress(out);
}
