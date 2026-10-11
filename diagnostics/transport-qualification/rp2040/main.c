/* Diagnostic-only observer around the exact production adapter, included to
 * expose bounded pending state for evidence. No substitute UART/ring/parser. */
#include "../../../firmware/rp2040/transport_adapter.c"
#include "pattern.h"
#include "pico/bootrom.h"
#include "pico/stdio_usb.h"
#include "flash_identity.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>
static uint32_t delivered, statuses, pattern_errors, metadata_errors, duplicates, gaps;
static uint32_t expected_id, last_id, events_seen, event_batches, payload_bytes, backwards;
static bool have_id;
static uint64_t digest = UINT64_C(14695981039346656037);
static uint32_t consumer_until, foreground_until, irq_until;
static bool consumer_paused, foreground_paused, irq_paused;
static char command[96];
static size_t command_size;
static bool command_discard;
static uint32_t commands_bad;
static void summary(const char *event) {
    fnr_rp_transport_diagnostics d;
    fnr_transport_snapshot_foreground(&d);
    printf("{\"event\":\"%s\",\"ms\":%lu,\"delivered\":%lu,\"statuses\":%lu,"
           "\"pattern_errors\":%lu,\"metadata_errors\":%lu,\"duplicates\":%lu,\"gaps\":%lu,"
           "\"digest\":\"%016llx\",\"payload_bytes\":%lu,\"events\":%lu,\"event_batches\":%lu,"
           "\"commands_bad\":%lu,\"backwards\":%lu,\"generation\":\"%016llx\",\"sequence\":%lu,"
           "\"datagram_pending\":%u,\"status_pending\":%u,\"encoded_pending\":%u,"
           "\"ring_pending\":%u,\"parser\":{",
           event, (unsigned long)to_ms_since_boot(get_absolute_time()), (unsigned long)delivered,
           (unsigned long)statuses, (unsigned long)pattern_errors, (unsigned long)metadata_errors,
           (unsigned long)duplicates, (unsigned long)gaps, (unsigned long long)digest,
           (unsigned long)payload_bytes, (unsigned long)events_seen, (unsigned long)event_batches,
           (unsigned long)commands_bad, (unsigned long)backwards, (unsigned long long)receiver.generation,
           (unsigned long)receiver.last_sequence, receiver.datagram_pending, receiver.status_pending,
           (unsigned)receiver.encoded_size, (unsigned)receive_bytes.count);
#define P(name) printf("\"" #name "\":%lu,", (unsigned long)d.parser.name)
    P(bytes_received); P(bytes_discarded); P(frames_valid); P(datagrams_received); P(status_received);
    P(datagrams_consumed); P(status_consumed); P(datagrams_replaced); P(status_replaced);
    P(datagrams_dropped); P(status_dropped); P(bad_cobs); P(bad_crc); P(bad_length); P(bad_version);
    P(bad_type); P(bad_payload); P(oversized_frames); P(empty_delimiters); P(parser_resets);
    P(link_discontinuities); P(link_timeouts); P(generation_changes); P(sequence_discontinuities);
    P(radio_resets);
    printf("\"radio_unavailable\":%lu},\"uart\":{", (unsigned long)d.parser.radio_unavailable);
#undef P
#define U(name) printf("\"" #name "\":%lu,", (unsigned long)d.uart.name)
    U(received); U(popped); U(dropped); U(overflows);
    printf("\"faults\":%lu},\"framing\":%lu,\"parity\":%lu,\"breaks\":%lu,\"overruns\":%lu}\n",
           (unsigned long)d.uart.faults, (unsigned long)d.framing_errors, (unsigned long)d.parity_errors,
           (unsigned long)d.breaks, (unsigned long)d.overruns);
#undef U
}
void fnr_transport_foreground_consume(const fnr_transport_consumer_api *api) {
    if (consumer_paused) return;
    uint32_t events = api->take_events();
    if (events) { events_seen |= events; ++event_batches; }
    fnr_transport_frame frame;
    if (api->take_status(&frame)) {
        fnr_transport_radio_status status;
        if (fnr_transport_unpack_status(&frame, &status) != FNR_TRANSPORT_OK) ++metadata_errors;
        else ++statuses;
    }
    if (!api->take_datagram(&frame)) return;
    fnr_datagram data;
    if (fnr_transport_unpack_datagram(&frame, &data) != FNR_TRANSPORT_OK) { ++metadata_errors; return; }
    if (!memcmp(data.source_mac, qualification_telemetry_source, 6)) {
        if (data.size != 176 || memcmp(data.payload, "Q005", 4)) { ++metadata_errors; return; }
        printf("{\"event\":\"esp\",\"values\":[");
        for (size_t i = 0; i < 25; ++i)
            printf("%s%lu", i ? "," : "", (unsigned long)qualification_get32(data.payload + 4 + 4 * i));
        printf("],\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"source\":\"%.*s\",\"active_us\":%lu}\n",
               data.payload[104], data.payload[105], data.payload[106], data.payload[107],
               data.payload[108], data.payload[109], 40, (char *)data.payload + 112,
               (unsigned long)(qualification_get32(data.payload + 156) - qualification_get32(data.payload + 152)));
        summary("after_esp"); return;
    }
    if (memcmp(data.source_mac, qualification_source, 6) || frame.version != 1 ||
        frame.type != FNR_TRANSPORT_RADIO_DATAGRAM || frame.payload_length != data.size + 6u ||
        !receiver.have_generation || frame.generation != receiver.generation)
        ++metadata_errors;
    uint32_t id = 0;
    for (size_t i = 0; i < data.size && i < 4; ++i) id |= (uint32_t)data.payload[i] << (8u * i);
    if (data.size < 4) {
        uint32_t mask = (UINT32_C(1) << (8u * data.size)) - 1u;
        if ((expected_id & mask) != id) ++pattern_errors;
        id = expected_id;
    }
    for (size_t i = 0; i < data.size; ++i)
        if (data.payload[i] != qualification_byte(id, i)) { ++pattern_errors; break; }
    if ((int32_t)(id - expected_id) < 0) ++backwards;
    if (have_id) {
        if (id == last_id) ++duplicates;
        if (id != expected_id) gaps += id - expected_id;
    }
    have_id = true; expected_id = id + 1u; last_id = id;
    digest = qualification_hash(digest, data.payload, data.size);
    ++delivered; payload_bytes += data.size;
}
static void execute(void) {
    unsigned value; int end = 0;
    if (!strcmp(command, "INFO")) {
        char uid[17];
        qualification_flash_uid(uid);
        printf("{\"event\":\"identity\",\"identity\":\"FNR-005 observer v1\",\"uid\":\"%s\",\"source\":\"%s\"}\n", uid, FNR_SOURCE_ID);
    } else if (!strcmp(command, "S")) summary("snapshot");
    else if (!strcmp(command, "H")) { digest = UINT64_C(14695981039346656037); summary("hash_reset"); }
    else if (!strcmp(command, "R")) { fnr_transport_reset_foreground(); summary("reset"); }
    else if (!strcmp(command, "B")) reset_usb_boot(0, 0);
    else if (sscanf(command, "E %u%n", &value, &end) == 1 && !command[end]) {
        expected_id = value; have_id = false; summary("expected");
    } else if (sscanf(command, "C %u%n", &value, &end) == 1 && !command[end] && value <= 10000) {
        consumer_paused = value != 0; consumer_until = to_ms_since_boot(get_absolute_time()) + value;
        summary("consumer_pause");
    } else if (sscanf(command, "F %u%n", &value, &end) == 1 && !command[end] && value <= 10000) {
        foreground_paused = value != 0; foreground_until = to_ms_since_boot(get_absolute_time()) + value;
        summary("foreground_pause");
    } else if (sscanf(command, "I %u%n", &value, &end) == 1 && !command[end] && value <= 1000) {
        irq_paused = value != 0; irq_until = to_ms_since_boot(get_absolute_time()) + value;
        irq_set_enabled(UART0_IRQ, !irq_paused); summary("irq_pause");
    } else if (!strncmp(command, "X ", 2) && strlen(command + 2) < 88) {
        /* Diagnostic reverse control, on the already verified GP0 -> ESP RX.
         * Bounded at 88 bytes (<8 ms); never used in the clean timed phase. */
        for (size_t i = 2; command[i]; ++i) uart_putc_raw(uart0, command[i]);
        uart_putc_raw(uart0, '\n');
        printf("{\"event\":\"command\",\"sent\":\"%s\"}\n", command + 2);
    } else ++commands_bad;
}
int main(void) {
    stdio_init_all(); stdio_set_translate_crlf(&stdio_usb, false);
    fnr_rp_transport_init();
    while (true) {
        tud_task();
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (consumer_paused && (int32_t)(now - consumer_until) >= 0) consumer_paused = false;
        if (foreground_paused && (int32_t)(now - foreground_until) >= 0) foreground_paused = false;
        if (irq_paused && (int32_t)(now - irq_until) >= 0) { irq_paused = false; irq_set_enabled(UART0_IRQ, true); }
        if (!foreground_paused) fnr_rp_transport_task();
        for (unsigned i = 0; i < 32; ++i) {
            int ch = getchar_timeout_us(0); if (ch < 0) break;
            if (ch == '\n') {
                if (!command_discard) { command[command_size] = 0; execute(); }
                else ++commands_bad;
                command_size = 0; command_discard = false;
            } else if (ch != '\r') {
                if (ch < 32 || ch > 126) command_discard = true;
                else if (command_size + 1 < sizeof(command)) command[command_size++] = (char)ch;
                else command_discard = true;
            }
        }
        tight_loop_contents();
    }
}
