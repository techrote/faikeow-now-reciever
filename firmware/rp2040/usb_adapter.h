#ifndef FNR_USB_ADAPTER_H
#define FNR_USB_ADAPTER_H

#include <stdbool.h>

#include "fnr/hid.h"

#ifdef __cplusplus
extern "C" {
#endif

bool fnr_usb_adapter_init(fnr_hid_device *device);
void fnr_usb_adapter_task(void);

#ifdef __cplusplus
}
#endif

#endif
