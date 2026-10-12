#ifndef FNR_TEST_FAKE_ESPNOW_H
#define FNR_TEST_FAKE_ESPNOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t u8;
enum esp_now_role {
    ESP_NOW_ROLE_IDLE = 0,
    ESP_NOW_ROLE_CONTROLLER,
    ESP_NOW_ROLE_SLAVE,
    ESP_NOW_ROLE_COMBO,
    ESP_NOW_ROLE_MAX
};
typedef void (*esp_now_recv_cb_t)(uint8_t *, uint8_t *, uint8_t);

int esp_now_init(void);
int esp_now_deinit(void);
int esp_now_register_recv_cb(esp_now_recv_cb_t cb);
int esp_now_unregister_recv_cb(void);
int esp_now_add_peer(uint8_t *mac_addr, uint8_t role, uint8_t channel,
                     uint8_t *key, uint8_t key_len);
int esp_now_is_peer_exist(uint8_t *mac_addr);
int esp_now_get_peer_channel(uint8_t *mac_addr);
int esp_now_set_self_role(uint8_t role);

#ifdef __cplusplus
}
#endif
#endif
