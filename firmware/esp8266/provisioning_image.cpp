#include "fnr/provisioning_image.h"
#include "provisioning_image.h"

const uint8_t *fnr_provisioning_image_public(size_t *size) {
    if (size != nullptr) *size = 0U;
    return nullptr;
}

const uint8_t *fnr_provisioning_image_radio_key(size_t *size) {
    if (size != nullptr) *size = 0U;
    return nullptr;
}
