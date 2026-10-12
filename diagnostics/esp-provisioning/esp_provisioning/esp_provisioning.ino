#include <Arduino.h>

#include <provisioning_adapter.h>
#include <radio_adapter.h>

static void print_mac(const uint8_t mac[FNR_MAC_SIZE]) {
    for (size_t i = 0U; i < FNR_MAC_SIZE; ++i) {
        if (i != 0U) Serial.print(':');
        if (mac[i] < 16U) Serial.print('0');
        Serial.print(mac[i], HEX);
    }
}

static void print_status(void) {
    fnr_esp_provisioning_status provisioning = {};
    fnr_esp_provisioning_snapshot(&provisioning);
    fnr_radio_provisioning_status radio = {};
    fnr_radio_provisioning_status_foreground(&radio);

    Serial.print("{\"event\":\"provisioning_status\",\"record_result\":");
    Serial.print((unsigned)provisioning.record_result);
    Serial.print(",\"receiver_mac\":\"");
    print_mac(provisioning.station_mac);
    Serial.print("\",\"provisioned\":");
    Serial.print(provisioning.config.provisioned ? 1 : 0);
    Serial.print(",\"key_present\":");
    Serial.print(provisioning.key_present ? 1 : 0);
    Serial.print(",\"radio_ready\":");
    Serial.print(provisioning.radio_ready ? 1 : 0);
    Serial.print(",\"encryption_enabled\":");
    Serial.print(radio.encryption_enabled ? 1 : 0);
    Serial.print(",\"authentication_enabled\":");
    Serial.print(radio.authentication_enabled ? 1 : 0);
    Serial.print(",\"authentication_rejected\":");
    Serial.print((unsigned long)radio.authentication_rejected);
    Serial.print(",\"channel\":");
    Serial.print((unsigned)provisioning.config.channel);
    Serial.print(",\"peer\":\"");
    print_mac(provisioning.config.peer_mac);
    Serial.print("\",\"platform_version\":");
    Serial.print((unsigned)provisioning.config.platform_version);
    Serial.print(",\"profile_id\":");
    Serial.print((unsigned)provisioning.config.profile_id);
    Serial.print(",\"profile_schema_version\":");
    Serial.print((unsigned)provisioning.config.profile_schema_version);
    Serial.print(",\"freshness_timeout_ms\":");
    Serial.print((unsigned long)provisioning.config.freshness_timeout_ms);
    Serial.print(",\"config_id\":\"");
    char id[19];
    snprintf(id, sizeof(id), "0x%08lx%08lx",
             (unsigned long)(provisioning.config.config_id >> 32U),
             (unsigned long)provisioning.config.config_id);
    Serial.print(id);
    Serial.print("\",\"generation\":");
    Serial.print((unsigned long)provisioning.config.generation);
    Serial.print(",\"radio_state\":");
    Serial.print((unsigned)radio.state);
    Serial.println("}");
}

void setup() {
    Serial.begin(115200);
    Serial.setDebugOutput(false);
    (void)fnr_esp_provisioning_init();
    delay(20);
    print_status();
}

void loop() {
    static char line[16];
    static size_t used = 0U;
    for (unsigned i = 0U; i < 16U && Serial.available(); ++i) {
        const int ch = Serial.read();
        if (ch == '\n') {
            line[used] = '\0';
            if (strcmp(line, "INFO") == 0 || strcmp(line, "STATS") == 0)
                print_status();
            used = 0U;
        } else if (ch != '\r' && used + 1U < sizeof(line)) {
            line[used++] = (char)ch;
        }
    }
    delay(1);
}
