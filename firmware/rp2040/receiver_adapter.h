#ifndef FNR_RP_RECEIVER_ADAPTER_H
#define FNR_RP_RECEIVER_ADAPTER_H

#include "fnr/hid.h"
#include "fnr/hid_relative_mouse.h"
#include "fnr/receiver.h"
#include <stdbool.h>

bool fnr_receiver_adapter_init(fnr_hid_device *hid,
                               fnr_relative_mouse_state *mouse);
bool fnr_receiver_adapter_configure(const fnr_receiver_config *config);
void fnr_receiver_adapter_snapshot(fnr_receiver_core *out);

#endif
