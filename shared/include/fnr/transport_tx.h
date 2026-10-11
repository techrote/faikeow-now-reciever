#ifndef FNR_TRANSPORT_TX_H
#define FNR_TRANSPORT_TX_H

#include "fnr/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fnr_transport_tx_counters {
    uint32_t datagrams_offered, datagrams_replaced, invalid_offers;
    uint32_t statuses_replaced, records_aborted, restarts;
    uint32_t frames_started, frames_completed, bytes_written;
} fnr_transport_tx_counters;

/* Single foreground owner. One immutable active wire frame, one newest-wins
 * pending datagram and one status snapshot. A reset aborts the active suffix,
 * drops pending records, emits a zero boundary, then a mandatory RESET status
 * before any new datagram. Already written UART bytes cannot be recalled.
 * A peek's borrowed span is valid until advance/restart; a UART adapter must
 * only advance by the number of bytes actually accepted by its finite FIFO.
 * These functions never wait, yield, allocate, or perform I/O. */
typedef struct fnr_transport_tx {
    uint8_t wire[FNR_TRANSPORT_WIRE_CAPACITY];
    size_t wire_size, wire_offset;
    bool active_frame, delimiter_pending;
    fnr_datagram datagram;
    fnr_transport_radio_status status;
    bool datagram_pending, status_pending;
    uint64_t generation;
    uint32_t next_sequence;
    fnr_transport_tx_counters counters;
} fnr_transport_tx;

bool fnr_transport_tx_init(fnr_transport_tx *tx, uint64_t generation,
                           const fnr_transport_radio_status *status);
bool fnr_transport_tx_restart(fnr_transport_tx *tx, uint64_t generation,
                              const fnr_transport_radio_status *status);
bool fnr_transport_tx_offer_datagram(fnr_transport_tx *tx,
                                     const fnr_datagram *datagram);
/* Periodic snapshots require flags=0 and cannot erase a pending RESET flag.
 * Use restart to retire old records and request a reset; pass a different
 * generation so losing RESET on the wire still leaves a restart discriminator.
 * init/restart may safely use &tx->status as input. */
bool fnr_transport_tx_offer_status(fnr_transport_tx *tx,
                                   const fnr_transport_radio_status *status);
size_t fnr_transport_tx_peek(fnr_transport_tx *tx, const uint8_t **bytes,
                            size_t limit);
bool fnr_transport_tx_advance(fnr_transport_tx *tx, size_t written);

#ifdef __cplusplus
}
#endif
#endif
