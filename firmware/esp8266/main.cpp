#include <Arduino.h>
#include "radio_adapter.h"
#include "transport_adapter.h"
void setup() {
    fnr_radio_start();
    fnr_esp_transport_init();
}
void loop() {
    fnr_esp_transport_task();
    delay(1);
}
