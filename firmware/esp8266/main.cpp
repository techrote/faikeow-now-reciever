#include <Arduino.h>
#include "radio_adapter.h"
/* FNR-005 may implement this generic foreground consumer only after
 * FNR-002 establishes the physical internal link. No transport is assumed. */
extern "C" void fnr_radio_foreground_consume(const fnr_datagram *datagram)
    __attribute__((weak));
void setup() { fnr_radio_start(); }
void loop() {
    /* No consumer present: retain the newest pending datagram and expose loss.
     * With a consumer: drain at most one owned record per loop iteration. */
    if (fnr_radio_foreground_consume != nullptr) {
        fnr_datagram record = {};
        if (fnr_radio_take_foreground(&record))
            fnr_radio_foreground_consume(&record);
    }
    delay(1);
}
