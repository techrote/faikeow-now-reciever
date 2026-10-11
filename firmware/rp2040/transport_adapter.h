#ifndef FNR_RP_TRANSPORT_ADAPTER_H
#define FNR_RP_TRANSPORT_ADAPTER_H

#include "fnr/transport.h"
#include "fnr/uart_buffer.h"

typedef struct fnr_rp_transport_diagnostics {
    fnr_transport_rx_counters parser;
    fnr_uart_rx_counters uart;
    /* IRQ observations, not an inferred count of lost radio datagrams. */
    uint32_t framing_errors, parity_errors, breaks, overruns;
} fnr_rp_transport_diagnostics;

void fnr_rp_transport_init(void);
void fnr_rp_transport_task(void);
uint32_t fnr_transport_take_events_foreground(void);
bool fnr_transport_take_datagram_foreground(fnr_transport_frame *out);
bool fnr_transport_take_status_foreground(fnr_transport_frame *out);
void fnr_transport_snapshot_foreground(fnr_rp_transport_diagnostics *out);
void fnr_transport_reset_foreground(void);

/* Optional foreground integration hook. FNR-006 may define it and use these
 * serialized owned-copy seams. It MUST process events before taking frames.
 * With no hook, only bounded newest slots remain pending; there is no HID
 * dispatch, envelope parsing, test injector or synthetic output here. */
typedef struct fnr_transport_consumer_api {
    uint32_t (*take_events)(void);
    bool (*take_datagram)(fnr_transport_frame *out);
    bool (*take_status)(fnr_transport_frame *out);
    void (*snapshot)(fnr_rp_transport_diagnostics *out);
    void (*reset)(void);
} fnr_transport_consumer_api;
void fnr_transport_foreground_consume(const fnr_transport_consumer_api *api);

#endif
