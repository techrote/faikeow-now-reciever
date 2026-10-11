#ifndef FNR_PLATFORM_H
#define FNR_PLATFORM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* This is an in-process owned record, NOT a wire layout or frozen protocol ABI. */
enum { FNR_DATAGRAM_CAPACITY = 250, FNR_MAC_SIZE = 6 };
typedef struct fnr_datagram {
    uint8_t source_mac[FNR_MAC_SIZE];
    uint16_t size;
    uint8_t payload[FNR_DATAGRAM_CAPACITY];
} fnr_datagram;
typedef enum fnr_copy_result {
    FNR_COPY_OK = 0,
    FNR_COPY_INVALID = 1
} fnr_copy_result;
/* Caller provides valid, non-overlapping storage. Success copies/owns bytes.
 * Invalid input leaves output unchanged. No queue, ordering, or wire parser here. */
fnr_copy_result fnr_datagram_copy(fnr_datagram *out, const uint8_t *mac,
                                 const uint8_t *bytes, size_t size);
#ifdef __cplusplus
}
#endif
#endif
