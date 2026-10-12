#ifndef FNR_TEST_FAKE_USER_INTERFACE_H
#define FNR_TEST_FAKE_USER_INTERFACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void system_set_os_print(uint8_t enabled);
void wifi_station_set_auto_connect(uint8_t enabled);
void wifi_station_disconnect(void);
bool wifi_set_channel(uint8_t channel);
uint8_t wifi_get_channel(void);

#ifdef __cplusplus
}
#endif

#endif
