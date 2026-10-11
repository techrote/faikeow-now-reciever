#include "fnr/transport_tx.h"

#include <limits.h>
#include <string.h>

static void add(uint32_t *value, uint32_t amount) {
    *value = UINT32_MAX - *value < amount ? UINT32_MAX : *value + amount;
}

static bool valid_status(const fnr_transport_radio_status *status) {
    fnr_transport_frame frame;
    return fnr_transport_make_status(&frame, status, 0, 0) == FNR_TRANSPORT_OK;
}

bool fnr_transport_tx_init(fnr_transport_tx *tx, uint64_t generation,
                           const fnr_transport_radio_status *status) {
    if (tx == NULL || !valid_status(status)) return false;
    const fnr_transport_radio_status snapshot = *status;
    memset(tx, 0, sizeof(*tx));
    tx->generation = generation;
    tx->status = snapshot;
    tx->status.flags |= FNR_TRANSPORT_STATUS_RESET;
    tx->status_pending = true;
    tx->delimiter_pending = true;
    return true;
}

bool fnr_transport_tx_restart(fnr_transport_tx *tx, uint64_t generation,
                              const fnr_transport_radio_status *status) {
    if (tx == NULL || !valid_status(status)) return false;
    fnr_transport_tx_counters saved = tx->counters;
    uint32_t aborted = (tx->active_frame ? 1u : 0u) +
        (tx->datagram_pending ? 1u : 0u) + (tx->status_pending ? 1u : 0u);
    add(&saved.records_aborted, aborted);
    add(&saved.restarts, 1);
    (void)fnr_transport_tx_init(tx, generation, status);
    tx->counters = saved;
    return true;
}

bool fnr_transport_tx_offer_datagram(fnr_transport_tx *tx,
                                     const fnr_datagram *datagram) {
    if (tx == NULL) return false;
    fnr_transport_frame frame;
    if (tx->status.state != FNR_RADIO_READY ||
        fnr_transport_make_datagram(&frame, datagram, 0, 0) != FNR_TRANSPORT_OK) {
        add(&tx->counters.invalid_offers, 1);
        return false;
    }
    add(&tx->counters.datagrams_offered, 1);
    if (tx->datagram_pending) add(&tx->counters.datagrams_replaced, 1);
    tx->datagram = *datagram;
    tx->datagram_pending = true;
    return true;
}

bool fnr_transport_tx_offer_status(fnr_transport_tx *tx,
                                   const fnr_transport_radio_status *status) {
    if (tx == NULL) return false;
    if (!valid_status(status) || status->flags != 0) {
        add(&tx->counters.invalid_offers, 1);
        return false;
    }
    const uint8_t pending_reset = tx->status_pending ?
        (uint8_t)(tx->status.flags & FNR_TRANSPORT_STATUS_RESET) : 0;
    if (tx->status_pending) add(&tx->counters.statuses_replaced, 1);
    tx->status = *status;
    tx->status.flags |= pending_reset;
    tx->status_pending = true;
    if (status->state != FNR_RADIO_READY && tx->datagram_pending) {
        tx->datagram_pending = false;
        add(&tx->counters.records_aborted, 1);
    }
    return true;
}

static bool prepare(fnr_transport_tx *tx) {
    if (tx->wire_offset < tx->wire_size) return true;
    tx->wire_offset = 0;
    tx->wire_size = 0;
    if (tx->delimiter_pending) {
        tx->wire[0] = 0;
        tx->wire_size = 1;
        tx->delimiter_pending = false;
        tx->active_frame = false;
        return true;
    }
    fnr_transport_frame frame;
    if (tx->status_pending) {
        tx->status.tx_replaced = tx->counters.datagrams_replaced;
        tx->status.tx_aborted = tx->counters.records_aborted;
        if (fnr_transport_make_status(&frame, &tx->status, tx->next_sequence,
                                      tx->generation) != FNR_TRANSPORT_OK)
            return false;
    } else if (tx->datagram_pending) {
        if (fnr_transport_make_datagram(&frame, &tx->datagram, tx->next_sequence,
                                        tx->generation) != FNR_TRANSPORT_OK)
            return false;
    } else {
        return false;
    }
    if (fnr_transport_encode(&frame, tx->wire, sizeof(tx->wire), &tx->wire_size) !=
        FNR_TRANSPORT_OK) return false;
    if (tx->status_pending) tx->status_pending = false;
    else tx->datagram_pending = false;
    tx->active_frame = true;
    tx->next_sequence = fnr_transport_sequence_next(tx->next_sequence);
    add(&tx->counters.frames_started, 1);
    return true;
}

size_t fnr_transport_tx_peek(fnr_transport_tx *tx, const uint8_t **bytes,
                            size_t limit) {
    if (bytes == NULL) return 0;
    *bytes = NULL;
    if (tx == NULL || limit == 0 || !prepare(tx)) return 0;
    *bytes = tx->wire + tx->wire_offset;
    const size_t remaining = tx->wire_size - tx->wire_offset;
    return remaining < limit ? remaining : limit;
}

bool fnr_transport_tx_advance(fnr_transport_tx *tx, size_t written) {
    if (tx == NULL || written > tx->wire_size - tx->wire_offset) return false;
    tx->wire_offset += written;
    add(&tx->counters.bytes_written, (uint32_t)written);
    if (tx->active_frame && tx->wire_offset == tx->wire_size) {
        add(&tx->counters.frames_completed, 1);
        tx->active_frame = false;
    }
    return true;
}
