#ifndef FNR_TEST_FAKE_ARDUINO_H
#define FNR_TEST_FAKE_ARDUINO_H

#include <cstddef>
#include <cstdint>

/* Minimal compile seam for the actual production transport adapter. These
 * declarations perform no I/O and intentionally model only its used APIs. */
enum { SERIAL_8N1 = 0x1c, SERIAL_TX_ONLY = 2 };

struct FnrFakeSerial {
    void begin(unsigned long baud, int config, int mode);
    void setDebugOutput(bool enabled);
    int availableForWrite();
    size_t write(const uint8_t *bytes, size_t length);
};

struct FnrFakeESP {
    uint32_t random();
};

extern FnrFakeSerial Serial;
extern FnrFakeESP ESP;
uint32_t millis();

#endif
