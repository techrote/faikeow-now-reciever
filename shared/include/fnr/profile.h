#ifndef FNR_PROFILE_H
#define FNR_PROFILE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Preliminary receiver-message dispatch seam. FNR-006 owns IDs, byte encodings
 * and final dispatch semantics. Host HID backend registration/descriptor/report
 * behavior is deliberately separate in fnr/hid.h so this file does not freeze a
 * platform wire contract as a side effect of FNR-003. */
typedef struct fnr_profile_view {
    uint16_t profile_id;
    uint16_t schema_version;
    const uint8_t *payload;
    uint16_t payload_size;
} fnr_profile_view;
typedef struct fnr_profile_ops {
    void *context;
    int (*accept)(void *context, const fnr_profile_view *borrowed_view);
    void (*neutralize)(void *context);
} fnr_profile_ops;
/* Transport/session acceptance precedes dispatch. HID fields do not belong here.
 * accept() must consume/copy before returning; neutralize() is profile-specific.
 * There are deliberately no production profile IDs or wire version constants. */
#ifdef __cplusplus
}
#endif
#endif
