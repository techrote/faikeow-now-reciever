#ifndef FNR_PROVISIONING_IMAGE_H
#define FNR_PROVISIONING_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Firmware-specific image source. Factory images return NULL/0. Provisioned
 * builds replace that source with generated, CRC-protected public bytes.
 * This interface never exposes radio key material. */
const uint8_t *fnr_provisioning_image_public(size_t *size);

#ifdef __cplusplus
}
#endif
#endif
