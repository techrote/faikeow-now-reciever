#include <Arduino.h>
#include "radio_adapter.h"
/* FNR-005 will attach a transport consumer after FNR-002 pinout verification.
 * Until then the bounded slot remains available; do not discard or decode it. */
void setup() { fnr_radio_start(); }
void loop() { delay(10); }
