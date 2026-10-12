#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Compile the real FNR-005 and FNR-006 RP2040 adapters together. Pico SDK I/O
 * and time are substituted only at the hardware boundary by fake_pico/. */
#include "../../firmware/rp2040/transport_adapter.c"
#include "../../firmware/rp2040/receiver_adapter.c"

static fnr_hid_device hid;
static fnr_relative_mouse_state mouse;
static fnr_hid_profile hid_profile;
static uint8_t sent[3];
static unsigned sends;

static bool hid_ready(void *context, uint8_t instance) {
    (void)context;
    return instance == 0U;
}

static bool hid_send(void *context, uint8_t instance, uint8_t report_id,
                     const uint8_t *data, uint16_t size) {
    (void)context;
    assert(instance == 0U && report_id == 0U && size == 3U);
    memcpy(sent, data, 3U);
    ++sends;
    return true;
}

static void service_hid(void) {
    assert(fnr_hid_device_service(&hid, hid_ready, hid_send, NULL) ==
           FNR_HID_STATUS_OK);
}

static void start(void) {
    fnr_fake_ms = 0U;
    fnr_fake_irq_mask = 0U;
    fnr_fake_restore_hook = NULL;
    fnr_fake_irq_handler = NULL;
    fnr_fake_fifo_size = fnr_fake_fifo_offset = fnr_fake_fifo_reads = 0U;
    fnr_fake_uart_registers.dr = fnr_fake_uart_registers.rsr = 0U;
    memset(sent, 0, sizeof(sent));
    sends = 0U;

    fnr_rp_transport_init();
    fnr_hid_device_init(&hid);
    fnr_relative_mouse_init(&mouse);
    fnr_relative_mouse_make_profile(&mouse, &hid_profile);
    assert(fnr_hid_device_register_profile(&hid, &hid_profile) == FNR_HID_STATUS_OK);
    assert(fnr_hid_device_select_profile(&hid, "relative_mouse") == FNR_HID_STATUS_OK);
    assert(fnr_hid_device_usb_event(&hid, FNR_HID_USB_MOUNT) == FNR_HID_STATUS_OK);
    service_hid();
    assert(fnr_receiver_adapter_init(&hid, &mouse));

    fnr_receiver_config config;
    fnr_receiver_config_default(&config);
    config.peer_enabled = true;
    const uint8_t peer[FNR_MAC_SIZE] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    memcpy(config.peer_mac, peer, sizeof(peer));
    assert(fnr_receiver_adapter_configure(&config));

    /* Consume FNR-005's startup RESET barrier and its neutral HID barrier. */
    fnr_rp_transport_task();
    assert(hid.pending && hid.pending_is_neutral);
    service_hid();
}

static void queue_wire(const uint8_t *bytes, size_t length) {
    for (size_t i = 0U; i < length; ++i)
        assert(fnr_uart_rx_push(&receive_bytes, bytes[i]));
}

static void queue_boundary(void) {
    const uint8_t zero = 0U;
    queue_wire(&zero, 1U);
}

static void queue_platform_datagram(uint32_t transport_sequence,
                                    uint32_t platform_sequence,
                                    int8_t x, int8_t y, uint8_t buttons) {
    uint8_t profile[FNR_RELATIVE_MOUSE_PAYLOAD_SIZE];
    assert(fnr_relative_mouse_payload_encode(x, y, buttons, profile,
        sizeof(profile)) == FNR_RELATIVE_MOUSE_PAYLOAD_OK);

    fnr_envelope envelope;
    memset(&envelope, 0, sizeof(envelope));
    envelope.version = FNR_PLATFORM_VERSION;
    envelope.profile_id = FNR_PROFILE_ID_RELATIVE_MOUSE;
    envelope.message_type = FNR_PLATFORM_MESSAGE_PROFILE_DATA;
    envelope.session = UINT32_C(0x12345678);
    envelope.sequence = platform_sequence;
    envelope.payload_length = FNR_RELATIVE_MOUSE_PAYLOAD_SIZE;
    memcpy(envelope.payload, profile, sizeof(profile));

    uint8_t application[FNR_DATAGRAM_CAPACITY];
    size_t application_size = 0U;
    assert(fnr_envelope_encode(&envelope, application, sizeof(application),
        &application_size) == FNR_ENVELOPE_OK);

    fnr_datagram datagram;
    const uint8_t peer[FNR_MAC_SIZE] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    assert(fnr_datagram_copy(&datagram, peer, application, application_size) ==
           FNR_COPY_OK);

    fnr_transport_frame frame;
    assert(fnr_transport_make_datagram(&frame, &datagram, transport_sequence,
        UINT64_C(42)) == FNR_TRANSPORT_OK);
    uint8_t wire[FNR_TRANSPORT_WIRE_CAPACITY];
    size_t wire_size = 0U;
    assert(fnr_transport_encode(&frame, wire, sizeof(wire), &wire_size) ==
           FNR_TRANSPORT_OK);
    queue_wire(wire, wire_size);
}

static void test_production_hook_chain(void) {
    start();
    queue_boundary();
    queue_platform_datagram(0U, 0U, -9, 12, 3U);
    fnr_rp_transport_task();
    assert(hid.pending && !hid.pending_is_neutral);
    service_hid();
    assert(sends >= 3U);
    assert(sent[0] == 3U && sent[1] == (uint8_t)-9 && sent[2] == 12U);

    /* Internal transport sequence advances but a duplicate PLATFORM sequence
     * must not reapply relative movement. */
    const unsigned before = sends;
    queue_platform_datagram(1U, 0U, 99, 99, 0U);
    fnr_rp_transport_task();
    assert(!hid.pending);
    service_hid();
    assert(sends == before);
    fnr_receiver_core snapshot;
    fnr_receiver_adapter_snapshot(&snapshot);
    assert(snapshot.counters.duplicates == 1U);

    /* A real FNR-005 reset is observed by the production hook before any
     * later datagram and creates a neutral barrier. */
    fnr_transport_reset_foreground();
    fnr_rp_transport_task();
    assert(hid.pending && hid.pending_is_neutral);
    service_hid();
    assert(sent[0] == 0U && sent[1] == 0U && sent[2] == 0U);
    fnr_receiver_adapter_snapshot(&snapshot);
    assert(snapshot.counters.transport_invalidations >= 2U);
}

int main(void) {
    test_production_hook_chain();
    puts("PASS: production FNR-005 hook -> FNR-006 core -> HID");
    return 0;
}
