#include <Arduino.h>
extern "C" {
#include <user_interface.h>
}

#include "radio_adapter.h"
#include "transport_adapter.h"

#if defined(DEBUG_ESP_PORT)
#error "FNR-005 reserves UART0 for framed transport; UART debug output is unsupported"
#endif

static fnr_transport_tx transmitter = {};
static uint64_t transport_generation = 0;
static uint32_t observed_radio_generation = 0;
static uint32_t last_status_ms = 0;
static bool initialized = false;
enum { STATUS_INTERVAL_MS = 1000, TX_QUANTUM = 32 };

static fnr_transport_radio_status radio_status() {
    fnr_radio_ingress snapshot = {};
    fnr_radio_status_foreground(&snapshot);
    fnr_transport_radio_status result = {};
    result.state = snapshot.state;
    result.channel = snapshot.config.channel;
    result.radio = snapshot.counters;
    return result;
}

void fnr_esp_transport_init(void) {
    /* Qualified FNR-002 UART0: default ESP TX pin, 115200 8N1, no flow control.
     * v1 needs no reverse/control traffic, so allocate no ESP receive buffer.
     * ROM boot output cannot be disabled here; leading zero resynchronizes it.
     * There is one UART writer, and no serial console or callback-context TX. */
    system_set_os_print(0);
    Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);
    Serial.setDebugOutput(false);
    system_set_os_print(0);
    /* SDK random is only an opaque boot discriminator, not an authentication
     * credential or proof of globally unique/persistent boot identity. RESET
     * status and the receiver's silence/error events are independent signals. */
    transport_generation = (static_cast<uint64_t>(ESP.random()) << 32u) |
        static_cast<uint64_t>(ESP.random());
    observed_radio_generation = fnr_radio_generation_foreground();
    const fnr_transport_radio_status status = radio_status();
    initialized = fnr_transport_tx_init(&transmitter, transport_generation, &status);
    last_status_ms = millis();
}

void fnr_esp_transport_task(void) {
    if (!initialized) return;
    const uint32_t now = millis();
    const uint32_t radio_generation = fnr_radio_generation_foreground();
    if (radio_generation != observed_radio_generation) {
        /* Foreground lifecycle changes retire both ingress and transport state.
         * Never detect restart using saturating diagnostic counters. */
        ++transport_generation;
        const fnr_transport_radio_status status = radio_status();
        (void)fnr_transport_tx_restart(&transmitter, transport_generation, &status);
        observed_radio_generation = radio_generation;
        last_status_ms = now;
    } else if (static_cast<uint32_t>(now - last_status_ms) >= STATUS_INTERVAL_MS) {
        const fnr_transport_radio_status status = radio_status();
        (void)fnr_transport_tx_offer_status(&transmitter, &status);
        last_status_ms = now;
    }

    /* Exactly one FNR-004 consumer. Its copy survives later radio callbacks.
     * An active partial wire frame is immutable; only the pending record can
     * be replaced when offered traffic exceeds the finite UART capacity. */
    fnr_datagram datagram = {};
    if (fnr_radio_take_foreground(&datagram))
        (void)fnr_transport_tx_offer_datagram(&transmitter, &datagram);

    const int free_bytes = Serial.availableForWrite();
    /* Core 3.1.2 reports 128-count free bytes but its writer considers count
     * 127 full. Reserve that final byte to avoid a one-character busy wait. */
    if (free_bytes <= 1) return;
    const size_t usable = static_cast<size_t>(free_bytes - 1);
    const size_t capacity = usable < TX_QUANTUM ?
        usable : static_cast<size_t>(TX_QUANTUM);
    const uint8_t *bytes = nullptr;
    const size_t count = fnr_transport_tx_peek(&transmitter, &bytes, capacity);
    if (count != 0) {
        /* Pinned core's availableForWrite reports UART FIFO space. With debug
         * disabled and a single writer this bounded write cannot fill beyond
         * the sampled space. No flush, blocking wait or UART write in rx(). */
        const size_t written = Serial.write(bytes, count);
        (void)fnr_transport_tx_advance(&transmitter, written);
    }
}

void fnr_esp_transport_snapshot(fnr_transport_tx_counters *out) {
    if (out != nullptr) *out = transmitter.counters;
}
