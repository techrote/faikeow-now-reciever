#ifndef FNR_ESP_PROVISIONING_IMAGE_H
#define FNR_ESP_PROVISIONING_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ESP-only generated secret image. Factory firmware returns NULL/0.
 * The RP2040 build never links this symbol or radio key material. */
const uint8_t *fnr_provisioning_image_radio_key(size_t *size);

#ifdef __cplusplus
}
#endif
#endif
