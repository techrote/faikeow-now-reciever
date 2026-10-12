/* Qualification only: includes the production implementations verbatim so
 * observation can inspect finite ownership without exporting test APIs. The
 * only UART configuration difference is enabling RX for bounded host commands.
 * Ordinary records still traverse rx -> real foreground take -> real TX task.
 * Raw damage stimuli deliberately bypass TX framing, never the RP parser. */
#include "pattern.h"
#include <ESP8266WiFi.h> /* Explicit dependency for makeEspArduino's source scanner. */
#include "radio_adapter.cpp"
#include "radio_auth_crypto.cpp"
#include "transport_adapter.cpp"
#ifndef FNR_SOURCE_ID
#error "Qualification image requires exact source identity"
#endif
#define Q_STRING_INNER(x) #x
#define Q_STRING(x) Q_STRING_INNER(x)
static_assert(sizeof(Q_STRING(FNR_SOURCE_ID)) == 41, "Exact 40-character source SHA required");
static uint32_t generated, next_id, remaining, interval_us, next_due;
static uint32_t sent_data, sent_status, raw_written, command_errors;
static uint64_t sent_hash = UINT64_C(14695981039346656037);
static unsigned application_length;
static bool frozen = true, finishing, muted;
static uint32_t finish_at;
static uint32_t phase_started_us, phase_ended_us;
static uint8_t damage[600];
static size_t damage_size, damage_offset;
static char command[96];
static size_t command_size;
static bool command_discard;

static void generate() {
    uint8_t bytes[FNR_DATAGRAM_CAPACITY];
    for (size_t i = 0; i < application_length; ++i) bytes[i] = qualification_byte(next_id, i);
    uint8_t source[6]; memcpy(source, qualification_source, sizeof(source));
    /* Calls the actual production callback body, from controlled foreground.
     * This establishes ingress ownership, not reception over RF. */
    rx(source, bytes, (uint8_t)application_length);
    ++generated; ++next_id;
}
static void telemetry() {
    fnr_transport_tx_counters counters;
    fnr_esp_transport_snapshot(&counters);
    fnr_datagram record = {};
    memcpy(record.source_mac, qualification_telemetry_source, 6);
    record.size = 176;
    memcpy(record.payload, "Q005", 4);
    uint32_t fields[] = {generated, sent_data, sent_status, raw_written, command_errors,
        ingress.counters.received, ingress.counters.accepted, ingress.counters.dropped,
        ingress.counters.consumed, counters.datagrams_offered,
        counters.datagrams_replaced, counters.invalid_offers,
        counters.statuses_replaced, counters.records_aborted,
        counters.restarts, counters.frames_started,
        counters.frames_completed, counters.bytes_written,
        (uint32_t)sent_hash, (uint32_t)(sent_hash >> 32u), next_id,
        (uint32_t)Serial.hasOverrun(), (uint32_t)Serial.hasRxError(),
        (uint32_t)transmitter.generation, (uint32_t)(transmitter.generation >> 32u)};
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
        qualification_put32(record.payload + 4 + i * 4, fields[i]);
    uint8_t mac[6]; fnr_radio_station_mac(mac); memcpy(record.payload + 104, mac, 6);
    memcpy(record.payload + 112, Q_STRING(FNR_SOURCE_ID), 40);
    qualification_put32(record.payload + 152, phase_started_us);
    qualification_put32(record.payload + 156, phase_ended_us);
    (void)fnr_transport_tx_offer_datagram(&transmitter, &record);
}
static void make_damage(unsigned kind) {
    fnr_datagram datagram = {};
    memcpy(datagram.source_mac, qualification_source, 6); datagram.size = 32;
    for (size_t i = 0; i < 32; ++i) datagram.payload[i] = qualification_byte(0xdeadbeefu, i);
    fnr_transport_frame frame;
    (void)fnr_transport_make_datagram(&frame, &datagram, transmitter.next_sequence,
                                    transmitter.generation);
    size_t size = 0;
    (void)fnr_transport_encode(&frame, damage, sizeof(damage), &size);
    uint8_t raw[FNR_TRANSPORT_RAW_CAPACITY]; size_t raw_size = 0;
    (void)fnr_transport_cobs_decode(damage, size - 1, raw, sizeof(raw), &raw_size);
    switch (kind) {
    case 0: memset(damage, 0, 8); size = 8; break;
    case 1: size /= 2; damage[size++] = 0; break;
    case 2: memmove(damage + 11, damage + 10, size - 10); damage[10] = 0x55; ++size; break;
    case 3: memmove(damage + 10, damage + 11, size - 11); --size; break;
    case 4: damage[20] ^= 0x40; break;
    case 5: raw[raw_size - 1] ^= 1; goto encode_raw;
    case 6: damage[0] = 254; break;
    case 7: raw[0] = 2; goto repair_crc;
    case 8: raw[1] = 255; goto repair_crc;
    case 9: raw[2] = 255; raw[3] = 255; goto repair_crc;
    case 10: memset(damage, 1, FNR_TRANSPORT_ENCODED_CAPACITY); size = FNR_TRANSPORT_ENCODED_CAPACITY; damage[size++] = 0; break;
    case 11: memset(damage, 1, FNR_TRANSPORT_ENCODED_CAPACITY + 1); size = FNR_TRANSPORT_ENCODED_CAPACITY + 1; damage[size++] = 0; break;
    case 12: for (size_t i = 0; i < 93; ++i) damage[i] = (uint8_t)(1u + i * 17u % 254u); size = 93; damage[size++] = 0; break;
    case 13: size /= 2; break; /* Deliberately undelimited partial. */
    default: ++command_errors; return;
    }
    damage_size = size; damage_offset = 0; return;
repair_crc: {
        uint16_t crc = fnr_transport_crc16(raw, raw_size - 2);
        raw[raw_size - 2] = (uint8_t)crc; raw[raw_size - 1] = (uint8_t)(crc >> 8u);
    }
encode_raw:
    (void)fnr_transport_cobs_encode(raw, raw_size, damage, sizeof(damage) - 1, &size);
    damage[size++] = 0; damage_size = size; damage_offset = 0;
}
static void execute() {
    unsigned count, length, rate, first, kind; int end = 0;
    if (sscanf(command, "RUN %u %u %u %u%n", &count, &length, &rate, &first, &end) == 4 &&
        command[end] == 0 && count > 0 && count <= 30000 && length >= 1 && length <= 250 &&
        rate >= 1 && rate <= 200 && remaining == 0 && !finishing && damage_offset == damage_size) {
        remaining = count; application_length = length; interval_us = 1000000u / rate;
        next_id = first; next_due = micros(); frozen = muted = false;
        phase_started_us = next_due;
    } else if (!strcmp(command, "STOP")) {
        remaining = 0; frozen = true; muted = false; finishing = true; finish_at = millis() + 200;
    } else if (!strcmp(command, "SNAP")) {
        if (!remaining) { frozen = true; muted = false; finishing = true; finish_at = millis() + 200; }
        else ++command_errors;
    } else if (!strcmp(command, "MUTE")) { muted = true; remaining = 0; }
    else if (!strcmp(command, "HASH") && frozen && !remaining) { sent_hash = UINT64_C(14695981039346656037); }
    else if (!strcmp(command, "RESUME")) { muted = false; }
    else if (!strcmp(command, "RESTART")) { fnr_radio_restart(); muted = false; }
    else if (!strcmp(command, "REBOOT")) { ESP.restart(); }
    else if (!strcmp(command, "ZERO") && frozen && !remaining) {
        damage[0] = 0; damage_size = 1; damage_offset = 0;
    } else if (sscanf(command, "FAULT %u%n", &kind, &end) == 1 && command[end] == 0 &&
               frozen && !remaining && !finishing && damage_offset == damage_size &&
               !transmitter.active_frame && !transmitter.datagram_pending && !transmitter.status_pending)
        make_damage(kind);
    else ++command_errors;
}
void setup() {
    fnr_radio_config config = {}; config.enabled = true; config.channel = 1;
    memcpy(config.allowed_mac, qualification_source, 6);
    fnr_radio_apply_config(&config);
    fnr_esp_transport_init();
    /* RX is qualification control only; production's TX FIFO/writer is retained. */
    Serial.begin(115200, SERIAL_8N1, SERIAL_FULL); Serial.setDebugOutput(false); system_set_os_print(0);
}
void loop() {
    for (unsigned i = 0; i < 32 && Serial.available(); ++i) {
        int value = Serial.read();
        if (value == '\n') {
            if (!command_discard) { command[command_size] = 0; execute(); }
            else ++command_errors;
            command_size = 0; command_discard = false;
        } else if (value != '\r') {
            if (command_size + 1 < sizeof(command)) command[command_size++] = (char)value;
            else command_discard = true;
        }
    }
    if (remaining && (int32_t)(micros() - next_due) >= 0) {
        generate(); --remaining; next_due += interval_us;
        if (!remaining) { phase_ended_us = micros(); frozen = true; finishing = true; finish_at = millis() + 200; }
    }
    if (frozen) last_status_ms = millis(); /* drain with no new heartbeat */
    if (damage_offset < damage_size) {
        int space = Serial.availableForWrite();
        if (space > 1) {
            size_t n = damage_size - damage_offset;
            if (n > 32) n = 32;
            if (n > (size_t)(space - 1)) n = (size_t)(space - 1);
            size_t wrote = Serial.write(damage + damage_offset, n);
            damage_offset += wrote; raw_written += (uint32_t)wrote;
        }
    } else if (!muted) {
        uint32_t completed = transmitter.counters.frames_completed;
        fnr_esp_transport_task(); /* actual production FIFO/TX/ingress machinery */
        if (transmitter.counters.frames_completed != completed) {
            fnr_transport_frame frame;
            if (fnr_transport_decode(transmitter.wire, transmitter.wire_size - 1, &frame) == FNR_TRANSPORT_OK) {
                if (frame.type == FNR_TRANSPORT_RADIO_STATUS) ++sent_status;
                else if (memcmp(frame.payload, qualification_telemetry_source, 6)) {
                    ++sent_data;
                    sent_hash = qualification_hash(sent_hash, frame.payload + 6, frame.payload_length - 6u);
                }
            } else ++command_errors;
        }
    }
    if (finishing && (int32_t)(millis() - finish_at) >= 0 && !transmitter.active_frame &&
        !transmitter.datagram_pending && !transmitter.status_pending && Serial.availableForWrite() == 128) {
        finishing = false; telemetry();
    }
    delay(1); /* Match the production foreground loop's cooperative cadence. */
}
