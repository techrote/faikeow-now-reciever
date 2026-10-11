// FNR-002 isolated RF test. Runtime-only peers/channel; no credentials or IP use.
// Receiver uses the accepted FNR-004 adapter unchanged, including its real SDK
// callback and bounded newest-datagram foreground handoff.
#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
#include <espnow.h>
}
#include <radio_adapter.h>
#include <rf_diag.h>
static bool tx_active, rx_active, sending, finishing;
static volatile bool outstanding;
static volatile uint32_t tx_ok, tx_callback_fail;
static uint32_t attempted, queued, enqueue_fail, next_send, finish_at, total, interval;
static uint32_t checked, integrity_errors, gaps, duplicates, expected_seq, pause_until;
static uint32_t uart_overruns, uart_rx_errors;
static unsigned payload_length;
// Requested SDK ceiling, in quarter dBm; this is not measured radiated power.
static unsigned power_limit_qdbm = 82;
static uint8_t peer[6], last_source[6];
static fnr_radio_config receive_config;
static fnr_radio_counters baseline;

static void tx_done(uint8_t *, uint8_t status) {
    if (status == 0) tx_ok++; else tx_callback_fail++;
    outstanding = false;
}
static void stop_radio() {
    if (tx_active) { esp_now_unregister_send_cb(); esp_now_deinit(); }
    tx_active = rx_active = sending = finishing = outstanding = false;
    fnr_radio_apply_config(nullptr);
    WiFi.mode(WIFI_OFF);
}
static bool mac_parse(const char *text, uint8_t *out) {
    unsigned m[6]; int end = 0;
    if (sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x%n", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], &end) != 6
        || end != 17 || text[end]) return false;
    unsigned any = 0;
    for (unsigned i = 0; i < 6; i++) { if (m[i] > 255) return false; out[i] = (uint8_t)m[i]; any |= m[i]; }
    return any && !(out[0] & 1);
}
static void mac_print(const uint8_t *mac) {
    Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
static void status(const char *event) {
    fnr_radio_ingress snapshot = {};
    fnr_radio_status_foreground(&snapshot);
    const auto &c = snapshot.counters;
    uint8_t mac[6]; fnr_radio_station_mac(mac);
    Serial.printf("{\"event\":\"%s\",\"identity\":\"FNR-002 RF diagnostic v1\",\"mac\":\"", event);
    mac_print(mac); Serial.print("\",\"peer\":\""); mac_print(peer);
    Serial.print("\",\"last_source\":\""); mac_print(last_source);
    Serial.printf("\",\"channel\":%u,\"rx_active\":%u,\"tx_active\":%u,\"state\":%u,"
        "\"payload_length\":%u,\"attempted\":%lu,\"queued\":%lu,\"enqueue_fail\":%lu,"
        "\"tx_ok\":%lu,\"tx_callback_fail\":%lu,\"outstanding\":%u,"
        "\"received\":%lu,\"accepted\":%lu,\"rejected_source\":%lu,\"invalid_length\":%lu,"
        "\"dropped\":%lu,\"consumed\":%lu,\"checked\":%lu,\"integrity_errors\":%lu,"
        "\"gaps\":%lu,\"duplicates\":%lu,\"uart_overrun\":%lu,\"uart_rx_error\":%lu,"
        "\"power_limit_qdbm\":%u,\"opmode\":%u,\"station_status\":%u,"
        "\"auto_connect\":%u,\"auto_reconnect\":%u,\"sleep_type\":%u}\n",
        (unsigned)wifi_get_channel(), rx_active, tx_active, (unsigned)snapshot.state,
        payload_length, (unsigned long)attempted, (unsigned long)queued, (unsigned long)enqueue_fail,
        (unsigned long)tx_ok, (unsigned long)tx_callback_fail, outstanding,
        (unsigned long)(c.received - baseline.received), (unsigned long)(c.accepted - baseline.accepted),
        (unsigned long)(c.rejected_source - baseline.rejected_source), (unsigned long)(c.invalid_length - baseline.invalid_length),
        (unsigned long)(c.dropped - baseline.dropped), (unsigned long)(c.consumed - baseline.consumed),
        (unsigned long)checked, (unsigned long)integrity_errors, (unsigned long)gaps, (unsigned long)duplicates,
        (unsigned long)uart_overruns, (unsigned long)uart_rx_errors,
        power_limit_qdbm, (unsigned)wifi_get_opmode(), (unsigned)wifi_station_get_connect_status(),
        (unsigned)wifi_station_get_auto_connect(), (unsigned)wifi_station_get_reconnect_policy(),
        (unsigned)wifi_get_sleep_type());
}
static void reset_case() {
    attempted = queued = enqueue_fail = tx_ok = tx_callback_fail = 0;
    checked = integrity_errors = gaps = duplicates = expected_seq = pause_until = 0;
    uart_overruns = uart_rx_errors = 0;
    memset(last_source, 0, sizeof(last_source));
    fnr_radio_ingress snapshot = {}; fnr_radio_status_foreground(&snapshot);
    baseline = snapshot.counters;
}
static void command(const char *line) {
    unsigned channel, length, count, spacing, pause, power; char mac_text[18]; int end = 0;
    if (!strcmp(line, "STATS") || !strcmp(line, "INFO")) { status("status"); return; }
    if (!strcmp(line, "OFF")) { stop_radio(); status("off"); return; }
    if (!strcmp(line, "RESET")) { Serial.flush(); ESP.restart(); return; }
    if (!strcmp(line, "RESTART") && rx_active) {
        fnr_radio_restart(); reset_case(); status("restarted"); return;
    }
    if (sscanf(line, "POWER %u%n", &power, &end) == 1 && !line[end] && power <= 82
        && !sending && !outstanding && !finishing) {
        power_limit_qdbm = power;
        system_phy_set_max_tpw((uint8_t)power_limit_qdbm);
        status("power_set"); return;
    }
    end = 0;
    if (sscanf(line, "PAUSE %u%n", &pause, &end) == 1 && !line[end] && pause <= 10000 && rx_active) {
        pause_until = millis() + pause; status("paused"); return;
    }
    end = 0;
    if (sscanf(line, "RX %u %17s %u%n", &channel, mac_text, &length, &end) == 3 && !line[end]
        && channel >= 1 && channel <= 14 && (length == 1 || (length >= 8 && length <= 250)) && mac_parse(mac_text, peer)) {
        stop_radio(); payload_length = length; receive_config = {};
        receive_config.enabled = true; receive_config.channel = (uint8_t)channel;
        memcpy(receive_config.allowed_mac, peer, 6);
        fnr_radio_apply_config(&receive_config);
        system_phy_set_max_tpw((uint8_t)power_limit_qdbm);
        rx_active = true; reset_case(); status("rx_ready"); return;
    }
    end = 0;
    if (sscanf(line, "TX %u %17s %u %u %u%n", &channel, mac_text, &length, &count, &spacing, &end) == 5 && !line[end]
        && channel >= 1 && channel <= 14 && (length == 1 || (length >= 8 && length <= 250))
        && count >= 1 && count <= 20000 && spacing <= 1000 && mac_parse(mac_text, peer)) {
        stop_radio(); payload_length = length; total = count; interval = spacing;
        WiFi.persistent(false); WiFi.setAutoReconnect(false); WiFi.mode(WIFI_STA);
        wifi_station_set_auto_connect(0); wifi_station_disconnect();
        if (!wifi_set_channel((uint8_t)channel) || wifi_get_channel() != channel || esp_now_init() != 0) {
            stop_radio(); Serial.println("{\"event\":\"error\",\"stage\":\"tx_init\"}"); return;
        }
        tx_active = true;
        if (esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER) != 0 || esp_now_register_send_cb(tx_done) != 0
            || esp_now_add_peer(peer, ESP_NOW_ROLE_SLAVE, (uint8_t)channel, nullptr, 0) != 0) {
            stop_radio(); Serial.println("{\"event\":\"error\",\"stage\":\"tx_peer\"}"); return;
        }
        system_phy_set_max_tpw((uint8_t)power_limit_qdbm);
        reset_case(); sending = true; next_send = millis() + 100; status("tx_started"); return;
    }
    Serial.println("{\"event\":\"error\",\"stage\":\"command\"}");
}
void setup() {
    WiFi.persistent(false); WiFi.setAutoReconnect(false); WiFi.mode(WIFI_OFF);
    Serial.setRxBufferSize(1024); Serial.begin(115200); Serial.setDebugOutput(false);
    fnr_radio_start(); delay(20); status("boot");
}
void loop() {
    if (Serial.hasOverrun()) uart_overruns++;
    if (Serial.hasRxError()) uart_rx_errors++;
    static char line[96]; static unsigned used; static bool discard;
    for (unsigned i = 0; i < 96 && Serial.available(); i++) {
        int ch = Serial.read();
        if (ch == '\n') { if (!discard) { line[used] = 0; command(line); } else Serial.println("{\"event\":\"error\",\"stage\":\"line_overflow\"}"); used = 0; discard = false; }
        else if (ch != '\r') { if (used + 1 < sizeof(line)) line[used++] = (char)ch; else discard = true; }
    }
    uint32_t now = millis();
    if (rx_active && (!pause_until || (int32_t)(now - pause_until) >= 0)) {
        pause_until = 0; fnr_datagram data = {};
        if (fnr_radio_take_foreground(&data)) {
            checked++; memcpy(last_source, data.source_mac, 6); uint32_t seq = 0;
            if (data.size != payload_length || !rf_diag_check(data.payload, data.size, expected_seq, &seq)) integrity_errors++;
            else if (seq < expected_seq) duplicates++;
            else { gaps += seq - expected_seq; expected_seq = seq + 1; }
        }
    }
    if (sending && !outstanding && (int32_t)(now - next_send) >= 0) {
        uint8_t data[250]; rf_diag_make(data, payload_length, attempted);
        outstanding = true;
        if (esp_now_send(peer, data, (uint8_t)payload_length) == 0) queued++;
        else { enqueue_fail++; outstanding = false; }
        attempted++; next_send = now + interval;
        if (attempted == total) { sending = false; finishing = true; finish_at = now + 1000; }
    }
    if (finishing && (int32_t)(now - finish_at) >= 0) { finishing = false; status("tx_end"); }
    yield();
}
