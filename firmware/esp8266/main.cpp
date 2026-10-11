#include <Arduino.h>
#include "radio_adapter.h"
/* Core 3.x defaults WiFi OFF; never opt into legacy WiFi-at-boot. */
void setup() { fnr_radio_link_probe(); }
void loop() { delay(100); }
