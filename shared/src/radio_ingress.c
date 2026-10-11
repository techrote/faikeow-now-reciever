#include "fnr/radio_ingress.h"
#include <string.h>
#include <limits.h>
static void bump(uint32_t *v) { if (*v != UINT32_MAX) ++*v; }
bool fnr_radio_config_valid(const fnr_radio_config *c) {
    if (!c || !c->enabled || c->channel < 1 || c->channel > 14) return false;
    uint8_t any = 0;
    for (size_t i = 0; i < FNR_MAC_SIZE; ++i) any |= c->allowed_mac[i];
    /* No broadcast, multicast, or zero MAC in a production admission rule. */
    return any != 0 && (c->allowed_mac[0] & 1u) == 0;
}
void fnr_radio_reset(fnr_radio_ingress *ctx, const fnr_radio_config *config) {
    if (!ctx) return;
    fnr_radio_counters saved = ctx->counters;
    bool previously_started = saved.starts != 0;
    memset(ctx, 0, sizeof(*ctx));
    ctx->counters = saved;
    if (previously_started) bump(&ctx->counters.restarts);
    if (fnr_radio_config_valid(config)) {
        ctx->config = *config;
        ctx->state = FNR_RADIO_STARTING;
        bump(&ctx->counters.starts);
    } else {
        ctx->state = FNR_RADIO_UNCONFIGURED;
    }
}
void fnr_radio_transition(fnr_radio_ingress *ctx, fnr_radio_state state) {
    if (!ctx) return;
    if (state == FNR_RADIO_ERROR) {
        bump(&ctx->counters.errors);
        ctx->pending = false;
    }
    if (state != FNR_RADIO_READY) ctx->pending = false;
    ctx->state = state;
}
void fnr_radio_receive(fnr_radio_ingress *ctx, const uint8_t *mac,
                       const uint8_t *payload, size_t length) {
    if (!ctx) return;
    bump(&ctx->counters.received);
    if (!mac || !payload || !length || length > FNR_DATAGRAM_CAPACITY) {
        bump(&ctx->counters.invalid_length); return;
    }
    if (ctx->state != FNR_RADIO_READY || !fnr_radio_config_valid(&ctx->config))
        return;
    if (memcmp(mac, ctx->config.allowed_mac, FNR_MAC_SIZE) != 0) {
        bump(&ctx->counters.rejected_source); return;
    }
    if (ctx->pending) bump(&ctx->counters.dropped); /* newest wins */
    /* Source and payload copied before publication; call must be serialized. */
    if (fnr_datagram_copy(&ctx->slot, mac, payload, length) == FNR_COPY_OK) {
        ctx->pending = true;
        bump(&ctx->counters.accepted);
    }
}
bool fnr_radio_take(fnr_radio_ingress *ctx, fnr_datagram *out) {
    if (!ctx || !out || ctx->state != FNR_RADIO_READY || !ctx->pending) return false;
    *out = ctx->slot;
    ctx->pending = false;
    bump(&ctx->counters.consumed);
    return true;
}
void fnr_radio_snapshot(const fnr_radio_ingress *ctx, fnr_radio_ingress *out) {
    if (ctx && out && ctx != out) *out = *ctx;
}
