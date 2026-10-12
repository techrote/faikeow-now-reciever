#ifndef FNR_TEST_FAKE_ESP8266_WIFI_H
#define FNR_TEST_FAKE_ESP8266_WIFI_H

#include <stdint.h>

enum { WIFI_OFF = 0, WIFI_STA = 1 };

struct FnrFakeWiFi {
    void persistent(bool enabled);
    void setAutoReconnect(bool enabled);
    void mode(int value);
    void macAddress(uint8_t *out);
};

extern FnrFakeWiFi WiFi;

#endif
