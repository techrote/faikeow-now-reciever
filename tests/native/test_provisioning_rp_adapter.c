#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fnr/provisioning.h"
#include "../../firmware/rp2040/receiver_adapter.h"

static uint8_t image[FNR_PROVISIONING_RECORD_SIZE];
static size_t image_size;
static unsigned stage_calls;
static unsigned revoke_calls;
static fnr_receiver_config staged_config;
static uint8_t staged_channel;
static fnr_receiver_provisioning_status fake_receiver_status;

const uint8_t *fnr_provisioning_image_public(size_t *size) {
    if (size != NULL) *size = image_size;
    return image_size == 0U ? NULL : image;
}

bool fnr_receiver_adapter_stage_provisioning(const fnr_receiver_config *config,
                                             uint8_t channel) {
    ++stage_calls;
    staged_config = *config;
    staged_channel = channel;
    fake_receiver_status.staged = true;
    fake_receiver_status.expected_channel = channel;
    return true;
}

bool fnr_receiver_adapter_revoke_provisioning(void) {
    ++revoke_calls;
    memset(&fake_receiver_status, 0, sizeof(fake_receiver_status));
    return true;
}

void fnr_receiver_adapter_provisioning_snapshot(
    fnr_receiver_provisioning_status *out) {
    if (out != NULL) *out = fake_receiver_status;
}

#include "../../firmware/rp2040/provisioning_adapter.c"

static fnr_provisioning_config fixture(void) {
    fnr_provisioning_config c;
    memset(&c, 0, sizeof(c));
    c.provisioned = true;
    c.encryption_required = true;
    c.channel = 6U;
    c.platform_version = FNR_PLATFORM_VERSION;
    c.profile_id = FNR_PROFILE_ID_RELATIVE_MOUSE;
    c.profile_schema_version = FNR_RELATIVE_MOUSE_SCHEMA_VERSION;
    const uint8_t peer[FNR_MAC_SIZE] = {2,3,4,5,6,7};
    memcpy(c.peer_mac, peer, sizeof(peer));
    c.freshness_timeout_ms = 250U;
    c.config_id = UINT64_C(0x123456789abcdef0);
    c.generation = 9U;
    return c;
}

static void reset_fake(void) {
    memset(image, 0, sizeof(image));
    image_size = 0U;
    stage_calls = revoke_calls = 0U;
    memset(&staged_config, 0, sizeof(staged_config));
    staged_channel = 0U;
    memset(&fake_receiver_status, 0, sizeof(fake_receiver_status));
}

static void test_empty_and_corrupt_fail_closed(void) {
    reset_fake();
    assert(fnr_rp_provisioning_init());
    assert(revoke_calls == 1U && stage_calls == 0U);
    fnr_rp_provisioning_status s;
    fnr_rp_provisioning_snapshot(&s);
    assert(s.record_result == FNR_PROVISIONING_EMPTY && !s.staged);

    reset_fake();
    const fnr_provisioning_config c = fixture();
    assert(fnr_provisioning_encode(&c, image) == FNR_PROVISIONING_OK);
    image[20] ^= 1U;
    image_size = sizeof(image);
    assert(fnr_rp_provisioning_init());
    assert(revoke_calls == 1U && stage_calls == 0U);
    fnr_rp_provisioning_snapshot(&s);
    assert(s.record_result == FNR_PROVISIONING_BAD_CRC && !s.staged);
}

static void test_valid_public_record_stages_without_secret(void) {
    reset_fake();
    const fnr_provisioning_config c = fixture();
    assert(fnr_provisioning_encode(&c, image) == FNR_PROVISIONING_OK);
    image_size = sizeof(image);
    assert(fnr_rp_provisioning_init());
    assert(stage_calls == 1U && revoke_calls == 0U);
    assert(staged_channel == 6U);
    assert(staged_config.peer_enabled && staged_config.profile_enabled);
    assert(memcmp(staged_config.peer_mac, c.peer_mac, FNR_MAC_SIZE) == 0);
    assert(staged_config.freshness_timeout_ms == 250U);

    fake_receiver_status.radio_acknowledged = true;
    fnr_rp_provisioning_status s;
    fnr_rp_provisioning_snapshot(&s);
    assert(s.record_result == FNR_PROVISIONING_OK);
    assert(s.staged && s.radio_acknowledged);
    assert(s.config.config_id == c.config_id);
}

int main(void) {
    test_empty_and_corrupt_fail_closed();
    test_valid_public_record_stages_without_secret();
    puts("PASS: RP provisioning loader fails closed and stages public-only config");
    return 0;
}
