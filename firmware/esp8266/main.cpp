#include <Arduino.h>

#include "provisioning_adapter.h"
#include "transport_adapter.h"

void setup() {
    (void)fnr_esp_provisioning_init();
    fnr_esp_transport_init();
}

void loop() {
    fnr_esp_transport_task();
    delay(1);
}
