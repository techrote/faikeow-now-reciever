#include <Arduino.h>
#include <espnow.h>
#include <type_traits>
#include "fnr/platform.h"
#include "radio_adapter.h"
using expected_rx = void (*)(uint8_t *, uint8_t *, uint8_t);
static_assert(std::is_same<esp_now_recv_cb_t, expected_rx>::value,
              "ESP8266 NONOS callback required; do not import ESP32/RTOS ABI");
/* A pointer anchor tests library symbol resolution without enabling RF. */
static int (* volatile registration_anchor)(esp_now_recv_cb_t) = esp_now_register_recv_cb;
void fnr_radio_link_probe(void) { (void)registration_anchor; }
/* FNR-004 will add bounded copying and an explicit synchronization contract.
 * No callback registration, unbounded queue, mouse fields, or WiFi init here. */
