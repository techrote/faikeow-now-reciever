#include "fnr/envelope.h"
#include "fnr/hid.h"
#include "fnr/hid_relative_mouse.h"
#include "fnr/platform.h"
#include "fnr/profile_relative_mouse.h"
#include "fnr/receiver.h"
#include "fnr/transport.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)

struct Captured {
    uint16_t size{};
    std::array<uint8_t, FNR_HID_MAX_REPORT_SIZE> data{};
};

struct UsbTransport {
    bool ready{true};
    bool send_ok{true};
    std::array<Captured, 128> sent{};
    std::size_t count{};
};

static bool ready_cb(void *ctx, uint8_t instance) {
    CHECK(instance == 0U);
    return static_cast<UsbTransport *>(ctx)->ready;
}

static bool send_cb(void *ctx, uint8_t instance, uint8_t report_id,
                    const uint8_t *data, uint16_t size) {
    auto *io = static_cast<UsbTransport *>(ctx);
    CHECK(instance == 0U && report_id == 0U && data != nullptr);
    if (!io->send_ok) return false;
    CHECK(io->count < io->sent.size());
    auto &out = io->sent[io->count++];
    out.size = size;
    CHECK(static_cast<std::size_t>(size) <= out.data.size());
    std::memcpy(out.data.data(), data, size);
    return true;
}

static constexpr std::array<uint8_t, FNR_MAC_SIZE> kPeer{{0x10, 0x20, 0x30, 0x40, 0x50, 0x60}};
static constexpr std::array<uint8_t, FNR_MAC_SIZE> kOtherPeer{{1, 2, 3, 4, 5, 6}};

struct Fixture {
    fnr_hid_device hid{};
    fnr_relative_mouse_state mouse{};
    fnr_hid_profile hid_profile{};
    fnr_relative_mouse_profile wire_profile{};
    fnr_profile_ops profile_ops{};
    fnr_receiver_core core{};
    fnr_receiver_config config{};
    UsbTransport usb{};

    Fixture() {
        fnr_hid_device_init(&hid);
        fnr_relative_mouse_init(&mouse);
        fnr_relative_mouse_make_profile(&mouse, &hid_profile);
        CHECK(fnr_hid_device_register_profile(&hid, &hid_profile) == FNR_HID_STATUS_OK);
        CHECK(fnr_hid_device_select_profile(&hid, "relative_mouse") == FNR_HID_STATUS_OK);
        fnr_relative_mouse_profile_init(&wire_profile, &mouse, &hid);
        fnr_relative_mouse_profile_make_ops(&wire_profile, &profile_ops);
        fnr_receiver_config_default(&config);
        config.peer_enabled = true;
        config.profile_enabled = true;
        std::memcpy(config.peer_mac, kPeer.data(), kPeer.size());
        CHECK(fnr_receiver_core_init(&core, &config, &profile_ops));
    }

    void service(fnr_hid_status expected = FNR_HID_STATUS_OK) {
        CHECK(fnr_hid_device_service(&hid, ready_cb, send_cb, &usb) == expected);
    }

    void mount() {
        CHECK(fnr_hid_device_usb_event(&hid, FNR_HID_USB_MOUNT) == FNR_HID_STATUS_OK);
        fnr_receiver_core_usb_state(&core, hid.usb_state);
        service();
        CHECK(usb.count > 0U);
        const auto &r = usb.sent[usb.count - 1U];
        CHECK(r.size == 3U && r.data[0] == 0U && r.data[1] == 0U && r.data[2] == 0U);
    }

    fnr_datagram make_datagram(uint64_t session, uint32_t sequence,
                               int16_t x, int16_t y, uint8_t buttons,
                               uint8_t profile_id = FNR_PROFILE_ID_RELATIVE_MOUSE,
                               const std::array<uint8_t, FNR_MAC_SIZE> &mac = kPeer) {
        uint8_t profile_bytes[FNR_RELATIVE_MOUSE_PAYLOAD_SIZE]{};
        CHECK(fnr_relative_mouse_payload_encode(x, y, buttons, profile_bytes,
              sizeof(profile_bytes)) == FNR_RELATIVE_MOUSE_PAYLOAD_OK);
        fnr_envelope envelope{};
        envelope.version = FNR_PLATFORM_VERSION;
        envelope.profile_id = profile_id;
        envelope.message_type = FNR_PLATFORM_MESSAGE_PROFILE_DATA;
        envelope.flags = 0U;
        envelope.session = session;
        envelope.sequence = sequence;
        envelope.payload_length = FNR_RELATIVE_MOUSE_PAYLOAD_SIZE;
        std::memcpy(envelope.payload, profile_bytes, sizeof(profile_bytes));
        uint8_t bytes[FNR_DATAGRAM_CAPACITY]{};
        size_t written = 0U;
        CHECK(fnr_envelope_encode(&envelope, bytes, sizeof(bytes), &written) == FNR_ENVELOPE_OK);
        fnr_datagram datagram{};
        CHECK(fnr_datagram_copy(&datagram, mac.data(), bytes, written) == FNR_COPY_OK);
        return datagram;
    }

    fnr_receiver_result send(uint64_t session, uint32_t sequence,
                             int16_t x, int16_t y, uint8_t buttons,
                             uint32_t now_ms) {
        const fnr_datagram d = make_datagram(session, sequence, x, y, buttons);
        return fnr_receiver_core_accept_datagram(&core, &d, now_ms);
    }
};

static void test_envelope_golden_and_bounds() {
    const uint8_t profile[] = {1U, 3U, 0xFBU, 7U};
    fnr_envelope minimum{};
    minimum.version = 1U;
    minimum.profile_id = 1U;
    minimum.message_type = 1U;
    minimum.session = 1U;
    std::array<uint8_t, FNR_DATAGRAM_CAPACITY> bytes{};
    size_t written = 999U;
    CHECK(fnr_envelope_encode(&minimum, bytes.data(), bytes.size(), &written) == FNR_ENVELOPE_OK);
    const std::array<uint8_t, 18> minimum_golden{{
        0x01, 0x01, 0x01, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }};
    CHECK(written == minimum_golden.size());
    CHECK(std::memcmp(bytes.data(), minimum_golden.data(), minimum_golden.size()) == 0);

    fnr_envelope envelope{};
    envelope.version = 1U;
    envelope.profile_id = 1U;
    envelope.message_type = 1U;
    envelope.flags = 0U;
    envelope.session = UINT64_C(0x0123456789abcdef);
    envelope.sequence = UINT32_C(0x89abcdef);
    envelope.payload_length = sizeof(profile);
    std::memcpy(envelope.payload, profile, sizeof(profile));
    written = 999U;
    CHECK(fnr_envelope_encode(&envelope, bytes.data(), bytes.size(), &written) == FNR_ENVELOPE_OK);
    const std::array<uint8_t, 22> golden{{
        0x01, 0x01, 0x01, 0x00, 0xef, 0xcd, 0xab, 0x89,
        0x67, 0x45, 0x23, 0x01, 0xef, 0xcd, 0xab, 0x89,
        0x04, 0x00, 0x01, 0x03, 0xfb, 0x07
    }};
    CHECK(written == golden.size());
    CHECK(std::memcmp(bytes.data(), golden.data(), golden.size()) == 0);
    fnr_envelope decoded{};
    CHECK(fnr_envelope_decode(golden.data(), golden.size(), &decoded) == FNR_ENVELOPE_OK);
    CHECK(decoded.session == envelope.session && decoded.sequence == envelope.sequence);
    CHECK(decoded.payload_length == 4U && std::memcmp(decoded.payload, profile, 4U) == 0);

    fnr_envelope maximum{};
    maximum.version = 1U;
    maximum.profile_id = 1U;
    maximum.message_type = 1U;
    maximum.session = 1U;
    maximum.payload_length = FNR_PLATFORM_PAYLOAD_CAPACITY;
    for (std::size_t i = 0; i < FNR_PLATFORM_PAYLOAD_CAPACITY; ++i)
        maximum.payload[i] = static_cast<uint8_t>(i);
    CHECK(fnr_envelope_encode(&maximum, bytes.data(), bytes.size(), &written) == FNR_ENVELOPE_OK);
    CHECK(written == FNR_DATAGRAM_CAPACITY);
    CHECK(fnr_envelope_decode(bytes.data(), written, &decoded) == FNR_ENVELOPE_OK);

    for (std::size_t length = 0U; length <= FNR_PLATFORM_PAYLOAD_CAPACITY; ++length) {
        fnr_envelope sized{};
        sized.version = FNR_PLATFORM_VERSION;
        sized.profile_id = FNR_PROFILE_ID_RELATIVE_MOUSE;
        sized.message_type = FNR_PLATFORM_MESSAGE_PROFILE_DATA;
        sized.session = UINT64_C(0xfedcba9876543210);
        sized.sequence = static_cast<uint32_t>(length);
        sized.payload_length = static_cast<uint16_t>(length);
        for (std::size_t i = 0U; i < length; ++i)
            sized.payload[i] = static_cast<uint8_t>(i ^ length);
        CHECK(fnr_envelope_encode(&sized, bytes.data(), bytes.size(), &written) == FNR_ENVELOPE_OK);
        CHECK(written == FNR_PLATFORM_ENVELOPE_HEADER_SIZE + length);
        CHECK(fnr_envelope_decode(bytes.data(), written, &decoded) == FNR_ENVELOPE_OK);
        CHECK(decoded.payload_length == length);
        CHECK(std::memcmp(decoded.payload, sized.payload, length) == 0);
    }

    auto bad = golden;
    bad[0] = 2U;
    CHECK(fnr_envelope_decode(bad.data(), bad.size(), &decoded) == FNR_ENVELOPE_UNSUPPORTED_VERSION);
    bad = golden; bad[1] = 0U;
    CHECK(fnr_envelope_decode(bad.data(), bad.size(), &decoded) == FNR_ENVELOPE_INVALID_PROFILE);
    bad = golden; bad[2] = 9U;
    CHECK(fnr_envelope_decode(bad.data(), bad.size(), &decoded) == FNR_ENVELOPE_UNKNOWN_MESSAGE);
    bad = golden; bad[3] = 1U;
    CHECK(fnr_envelope_decode(bad.data(), bad.size(), &decoded) == FNR_ENVELOPE_INVALID_FLAGS);
    bad = golden; std::fill(bad.begin() + 4, bad.begin() + 12, 0U);
    CHECK(fnr_envelope_decode(bad.data(), bad.size(), &decoded) == FNR_ENVELOPE_INVALID_SESSION);
    bad = golden; bad[16] = 3U;
    CHECK(fnr_envelope_decode(bad.data(), bad.size(), &decoded) == FNR_ENVELOPE_INVALID_LENGTH);
    CHECK(fnr_envelope_decode(golden.data(), FNR_PLATFORM_ENVELOPE_HEADER_SIZE - 1U, &decoded) == FNR_ENVELOPE_INVALID_LENGTH);
    CHECK(fnr_envelope_encode(nullptr, bytes.data(), bytes.size(), &written) == FNR_ENVELOPE_INVALID_ARGUMENT);
}

static void test_profile_payload_contract() {
    uint8_t bytes[4]{};
    CHECK(fnr_relative_mouse_payload_encode(0, 0, 0U, bytes, sizeof(bytes)) == FNR_RELATIVE_MOUSE_PAYLOAD_OK);
    const uint8_t zero[] = {1U, 0U, 0U, 0U};
    CHECK(std::memcmp(bytes, zero, sizeof(zero)) == 0);
    CHECK(fnr_relative_mouse_payload_encode(-127, 127, 3U, bytes, sizeof(bytes)) == FNR_RELATIVE_MOUSE_PAYLOAD_OK);
    const uint8_t golden[] = {1U, 3U, 0x81U, 0x7fU};
    CHECK(std::memcmp(bytes, golden, sizeof(golden)) == 0);
    fnr_relative_mouse_payload decoded{};
    CHECK(fnr_relative_mouse_payload_decode(bytes, sizeof(bytes), &decoded) == FNR_RELATIVE_MOUSE_PAYLOAD_OK);
    CHECK(decoded.x == -127 && decoded.y == 127 && decoded.buttons == 3U);
    CHECK(fnr_relative_mouse_payload_encode(-128, 0, 0, bytes, sizeof(bytes)) == FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_AXIS);
    CHECK(fnr_relative_mouse_payload_encode(0, 128, 0, bytes, sizeof(bytes)) == FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_AXIS);
    CHECK(fnr_relative_mouse_payload_encode(0, 0, 4U, bytes, sizeof(bytes)) == FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_BUTTONS);
    const uint8_t bad_axis[] = {1U, 0U, 0x80U, 0U};
    CHECK(fnr_relative_mouse_payload_decode(bad_axis, sizeof(bad_axis), &decoded) == FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_AXIS);
    const uint8_t bad_version[] = {2U, 0U, 0U, 0U};
    CHECK(fnr_relative_mouse_payload_decode(bad_version, sizeof(bad_version), &decoded) == FNR_RELATIVE_MOUSE_PAYLOAD_UNSUPPORTED_VERSION);
    const uint8_t bad_buttons[] = {1U, 4U, 0U, 0U};
    CHECK(fnr_relative_mouse_payload_decode(bad_buttons, sizeof(bad_buttons), &decoded) == FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_BUTTONS);
    CHECK(fnr_relative_mouse_payload_decode(bytes, 3U, &decoded) == FNR_RELATIVE_MOUSE_PAYLOAD_INVALID_LENGTH);
}

static void test_sequence_comparator() {
    CHECK(fnr_sequence_compare(0U, 1U) == FNR_SEQUENCE_NEWER);
    CHECK(fnr_sequence_compare(0U, 0U) == FNR_SEQUENCE_DUPLICATE);
    CHECK(fnr_sequence_compare(0U, UINT32_MAX) == FNR_SEQUENCE_STALE);
    CHECK(fnr_sequence_compare(UINT32_MAX, 0U) == FNR_SEQUENCE_NEWER);
    CHECK(fnr_sequence_compare(0U, UINT32_C(0x80000000)) == FNR_SEQUENCE_AMBIGUOUS);
    CHECK(fnr_sequence_compare(UINT32_C(0x80000000), 0U) == FNR_SEQUENCE_AMBIGUOUS);
}

static void test_config_and_authorization() {
    fnr_receiver_config c{};
    fnr_receiver_config_default(&c);
    CHECK(fnr_receiver_config_valid(&c));
    Fixture f;
    fnr_receiver_config blocked{};
    fnr_receiver_config_default(&blocked);
    CHECK(fnr_receiver_core_reconfigure(&f.core, &blocked));
    auto d = f.make_datagram(1U, 0U, 1, 2, 0U);
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &d, 0U) == FNR_RECEIVER_UNAUTHORIZED_PEER);
    blocked.peer_enabled = true;
    CHECK(!fnr_receiver_config_valid(&blocked));
    std::memcpy(blocked.peer_mac, kPeer.data(), kPeer.size());
    CHECK(fnr_receiver_config_valid(&blocked));
    auto disabled = f.make_datagram(1U, 0U, 1, 2, 0U);
    CHECK(fnr_receiver_core_reconfigure(&f.core, &blocked));
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &disabled, 0U) ==
          FNR_RECEIVER_PROFILE_DISABLED);
    CHECK(!f.core.have_session && !f.core.fresh);
    blocked.profile_enabled = true;
    CHECK(fnr_receiver_core_reconfigure(&f.core, &blocked));
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &disabled, 1U) ==
          FNR_RECEIVER_ACCEPTED_OUTPUT_DROPPED);
    blocked.freshness_timeout_ms = 0U;
    CHECK(!fnr_receiver_config_valid(&blocked));
}

static void test_order_profile_and_hid() {
    Fixture f;
    f.mount();
    CHECK(f.send(11U, 0U, 10, -20, 1U, 0U) == FNR_RECEIVER_ACCEPTED);
    CHECK(f.hid.pending && !f.hid.pending_is_neutral);
    f.service();
    const auto first = f.usb.sent[f.usb.count - 1U];
    CHECK(first.data[0] == 1U && first.data[1] == 10U && first.data[2] == static_cast<uint8_t>(-20));
    CHECK(f.send(11U, 0U, 99, 99, 3U, 1U) == FNR_RECEIVER_DUPLICATE);
    CHECK(!f.hid.pending);
    CHECK(f.send(11U, UINT32_MAX, 1, 1, 0U, 2U) == FNR_RECEIVER_STALE);
    CHECK(f.send(11U, 2U, -5, 7, 2U, 3U) == FNR_RECEIVER_ACCEPTED);
    CHECK(f.core.counters.sequence_gap_events == 1U && f.core.counters.sequence_missing == 1U);
    f.service();
    CHECK(f.send(11U, UINT32_C(0x80000002), 1, 1, 0U, 4U) == FNR_RECEIVER_AMBIGUOUS_SEQUENCE);

    auto wrong_peer = f.make_datagram(11U, 3U, 1, 1, 0U, FNR_PROFILE_ID_RELATIVE_MOUSE, kOtherPeer);
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &wrong_peer, 5U) == FNR_RECEIVER_UNAUTHORIZED_PEER);
    auto wrong_profile = f.make_datagram(11U, 3U, 1, 1, 0U, 2U);
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &wrong_profile, 5U) == FNR_RECEIVER_WRONG_PROFILE);

    auto invalid = f.make_datagram(11U, 3U, 1, 1, 0U);
    invalid.payload[FNR_PLATFORM_ENVELOPE_HEADER_SIZE + 1U] = 0x80U;
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &invalid, 5U) == FNR_RECEIVER_INVALID_PROFILE_PAYLOAD);
}

static void test_core_sequence_wrap() {
    Fixture f;
    f.mount();
    const std::array<uint32_t, 4> sequence{{UINT32_MAX - 1U, UINT32_MAX, 0U, 1U}};
    for (std::size_t i = 0U; i < sequence.size(); ++i) {
        CHECK(f.send(UINT64_C(0x1111222233334444), sequence[i],
                     static_cast<int16_t>(i), 0, 0U,
                     static_cast<uint32_t>(i)) == FNR_RECEIVER_ACCEPTED);
        f.service();
    }
    CHECK(f.core.counters.duplicates == 0U);
    CHECK(f.core.counters.stale == 0U);
    CHECK(f.core.counters.ambiguous_sequence == 0U);
    CHECK(f.core.counters.sequence_gap_events == 0U);
}

static void test_rejected_messages_do_not_refresh() {
    Fixture f;
    f.mount();
    CHECK(f.send(1U, 0U, 1, 2, 1U, 0U) == FNR_RECEIVER_ACCEPTED);
    f.service();

    auto wrong_peer = f.make_datagram(1U, 1U, 1, 1, 0U,
                                      FNR_PROFILE_ID_RELATIVE_MOUSE, kOtherPeer);
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &wrong_peer, 100U) ==
          FNR_RECEIVER_UNAUTHORIZED_PEER);

    auto wrong_profile = f.make_datagram(1U, 1U, 1, 1, 0U, 2U);
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &wrong_profile, 120U) ==
          FNR_RECEIVER_WRONG_PROFILE);

    auto malformed = f.make_datagram(1U, 1U, 1, 1, 0U);
    malformed.payload[0] = 2U;
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &malformed, 140U) ==
          FNR_RECEIVER_MALFORMED_ENVELOPE);

    auto invalid_profile = f.make_datagram(1U, 1U, 1, 1, 0U);
    invalid_profile.payload[FNR_PLATFORM_ENVELOPE_HEADER_SIZE] = 2U;
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &invalid_profile, 160U) ==
          FNR_RECEIVER_INVALID_PROFILE_PAYLOAD);

    CHECK(f.send(1U, 0U, 10, 10, 3U, 200U) == FNR_RECEIVER_DUPLICATE);
    fnr_receiver_core_tick(&f.core, 249U);
    CHECK(f.core.fresh);
    fnr_receiver_core_tick(&f.core, 250U);
    CHECK(!f.core.fresh && f.core.counters.freshness_timeouts == 1U);
    f.service();
}

static void test_transport_event_matrix() {
    Fixture f;
    f.mount();
    const std::array<uint32_t, 7> events{{
        FNR_TRANSPORT_EVENT_RESET,
        FNR_TRANSPORT_EVENT_LINK_ERROR,
        FNR_TRANSPORT_EVENT_GENERATION_CHANGE,
        FNR_TRANSPORT_EVENT_SEQUENCE_DISCONTINUITY,
        FNR_TRANSPORT_EVENT_RADIO_RESET,
        FNR_TRANSPORT_EVENT_RADIO_UNAVAILABLE,
        FNR_TRANSPORT_EVENT_TIMEOUT
    }};
    uint32_t sequence = 0U;
    uint32_t now = 0U;
    for (const uint32_t event : events) {
        CHECK(f.send(77U, sequence, 1, -1, 1U, now) == FNR_RECEIVER_ACCEPTED);
        f.service();
        const uint64_t session_before = f.core.session;
        const uint32_t sequence_before = f.core.last_sequence;
        fnr_receiver_core_transport_events(&f.core, event);
        CHECK(!f.core.fresh && f.core.have_session && f.core.have_sequence);
        CHECK(f.core.session == session_before && f.core.last_sequence == sequence_before);
        CHECK(f.hid.pending && f.hid.pending_is_neutral);
        f.service();
        ++sequence;
        ++now;
    }
    CHECK(f.core.counters.transport_invalidations == events.size());

    const uint32_t before = f.wire_profile.neutralizations;
    fnr_receiver_core_transport_events(&f.core,
        FNR_TRANSPORT_EVENT_DATAGRAM_REPLACED | FNR_TRANSPORT_EVENT_STATUS_REPLACED);
    CHECK(f.wire_profile.neutralizations == before);
}

static void test_usb_suspend_resume_no_backlog() {
    Fixture f;
    f.mount();
    CHECK(f.send(5U, 0U, 10, 20, 3U, 0U) == FNR_RECEIVER_ACCEPTED);
    f.service();

    CHECK(fnr_hid_device_usb_event(&f.hid, FNR_HID_USB_SUSPEND) == FNR_HID_STATUS_OK);
    fnr_receiver_core_usb_state(&f.core, f.hid.usb_state);
    CHECK(f.core.fresh && f.mouse.x == 0 && f.mouse.y == 0 && f.mouse.buttons == 0U);
    CHECK(f.send(5U, 1U, 99, 88, 3U, 1U) == FNR_RECEIVER_ACCEPTED_OUTPUT_DROPPED);
    CHECK(f.mouse.x == 0 && f.mouse.y == 0 && f.mouse.buttons == 0U);

    CHECK(fnr_hid_device_usb_event(&f.hid, FNR_HID_USB_RESUME) == FNR_HID_STATUS_OK);
    fnr_receiver_core_usb_state(&f.core, f.hid.usb_state);
    f.service();
    const auto &neutral = f.usb.sent[f.usb.count - 1U];
    CHECK(neutral.data[0] == 0U && neutral.data[1] == 0U && neutral.data[2] == 0U);

    CHECK(f.send(5U, 2U, -7, 8, 2U, 2U) == FNR_RECEIVER_ACCEPTED);
    f.service();
    const auto &fresh = f.usb.sent[f.usb.count - 1U];
    CHECK(fresh.data[0] == 2U && fresh.data[1] == static_cast<uint8_t>(-7) &&
          fresh.data[2] == 8U);
}

static void test_session_change_and_retirement() {
    Fixture f;
    f.mount();
    CHECK(f.send(100U, 10U, 1, 2, 1U, 0U) == FNR_RECEIVER_ACCEPTED);
    f.service();
    CHECK(f.send(200U, 1U, 3, 4, 2U, 10U) == FNR_RECEIVER_SESSION_CHANGE_DEFERRED);
    const auto changed = f.send(200U, 0U, 3, 4, 2U, 11U);
    CHECK(changed == FNR_RECEIVER_ACCEPTED_OUTPUT_DROPPED);
    CHECK(f.hid.pending && f.hid.pending_is_neutral);
    f.service();
    CHECK(f.send(200U, 1U, 5, 6, 2U, 12U) == FNR_RECEIVER_ACCEPTED);
    f.service();
    CHECK(f.usb.sent[f.usb.count - 1U].data[0] == 2U);
    CHECK(f.send(100U, 0U, 9, 9, 3U, 13U) == FNR_RECEIVER_RETIRED_SESSION);

    CHECK(f.send(300U, 5U, 1, 1, 0U, 20U) == FNR_RECEIVER_SESSION_CHANGE_DEFERRED);
    fnr_receiver_core_tick(&f.core, 262U);
    CHECK(!f.core.fresh && f.hid.pending_is_neutral);
    f.service();
    const auto after_timeout = f.send(300U, 5U, 7, 8, 1U, 263U);
    CHECK(after_timeout == FNR_RECEIVER_ACCEPTED_OUTPUT_DROPPED);
    f.service();
    CHECK(f.send(300U, 6U, 7, 8, 1U, 264U) == FNR_RECEIVER_ACCEPTED);
    f.service();
}

static void test_freshness_wrap_and_nonrefresh() {
    Fixture f;
    f.mount();
    CHECK(f.send(1U, 0U, 1, 2, 1U, 0U) == FNR_RECEIVER_ACCEPTED);
    f.service();
    CHECK(f.send(1U, 0U, 99, 99, 3U, 200U) == FNR_RECEIVER_DUPLICATE);
    fnr_receiver_core_tick(&f.core, 249U);
    CHECK(f.core.fresh);
    fnr_receiver_core_tick(&f.core, 250U);
    CHECK(!f.core.fresh && f.core.counters.freshness_timeouts == 1U);
    f.service();

    Fixture w;
    w.mount();
    const uint32_t start = UINT32_MAX - 100U;
    CHECK(w.send(9U, 0U, 1, 1, 0U, start) == FNR_RECEIVER_ACCEPTED);
    w.service();
    fnr_receiver_core_tick(&w.core, 100U);
    CHECK(w.core.fresh);
    fnr_receiver_core_tick(&w.core, 200U);
    CHECK(!w.core.fresh);
}

static void test_transport_and_usb_invalidation() {
    Fixture f;
    f.mount();
    CHECK(f.send(77U, 10U, 20, 30, 3U, 0U) == FNR_RECEIVER_ACCEPTED);
    f.service();
    const uint32_t neutral_before = f.wire_profile.neutralizations;
    fnr_receiver_core_transport_events(&f.core, FNR_TRANSPORT_EVENT_DATAGRAM_REPLACED |
                                                 FNR_TRANSPORT_EVENT_STATUS_REPLACED);
    CHECK(f.wire_profile.neutralizations == neutral_before && f.core.fresh);
    fnr_receiver_core_transport_events(&f.core, FNR_TRANSPORT_EVENT_LINK_ERROR);
    CHECK(!f.core.fresh && f.core.have_sequence && f.core.have_session);
    CHECK(f.hid.pending_is_neutral);
    f.service();
    CHECK(f.send(77U, 3U, -1, 2, 1U, 1U) == FNR_RECEIVER_STALE);
    CHECK(f.send(77U, 11U, -1, 2, 1U, 2U) == FNR_RECEIVER_ACCEPTED);
    f.service();

    CHECK(fnr_hid_device_usb_event(&f.hid, FNR_HID_USB_UNMOUNT) == FNR_HID_STATUS_OK);
    fnr_receiver_core_usb_state(&f.core, f.hid.usb_state);
    CHECK(f.core.fresh && f.mouse.buttons == 0U);
    CHECK(f.send(77U, 12U, 100, 100, 3U, 2U) == FNR_RECEIVER_ACCEPTED_OUTPUT_DROPPED);
    CHECK(f.mouse.x == 0 && f.mouse.y == 0 && f.mouse.buttons == 0U);
    CHECK(fnr_hid_device_usb_event(&f.hid, FNR_HID_USB_MOUNT) == FNR_HID_STATUS_OK);
    fnr_receiver_core_usb_state(&f.core, f.hid.usb_state);
    f.service();
    CHECK(f.send(77U, 13U, 6, 7, 2U, 3U) == FNR_RECEIVER_ACCEPTED);
    f.service();
}

static void test_pending_motion_is_retired_by_neutralization() {
    Fixture f;
    f.mount();

    CHECK(f.send(9U, 0U, 40, 30, 3U, 0U) == FNR_RECEIVER_ACCEPTED);
    CHECK(f.hid.pending && !f.hid.pending_is_neutral);
    fnr_receiver_core_transport_events(&f.core, FNR_TRANSPORT_EVENT_LINK_ERROR);
    CHECK(f.hid.pending && f.hid.pending_is_neutral);
    f.service();
    auto report = f.usb.sent[f.usb.count - 1U];
    CHECK(report.data[0] == 0U && report.data[1] == 0U && report.data[2] == 0U);

    CHECK(f.send(9U, 1U, 50, 20, 1U, 1U) == FNR_RECEIVER_ACCEPTED);
    CHECK(f.hid.pending && !f.hid.pending_is_neutral);
    fnr_receiver_core_tick(&f.core, 251U);
    CHECK(f.hid.pending && f.hid.pending_is_neutral);
    f.service();
    report = f.usb.sent[f.usb.count - 1U];
    CHECK(report.data[0] == 0U && report.data[1] == 0U && report.data[2] == 0U);

    CHECK(f.send(9U, 2U, 60, 10, 2U, 252U) == FNR_RECEIVER_ACCEPTED);
    CHECK(f.hid.pending && !f.hid.pending_is_neutral);
    CHECK(f.send(10U, 0U, 70, 5, 3U, 253U) ==
          FNR_RECEIVER_ACCEPTED_OUTPUT_DROPPED);
    CHECK(f.hid.pending && f.hid.pending_is_neutral);
    f.service();
    report = f.usb.sent[f.usb.count - 1U];
    CHECK(report.data[0] == 0U && report.data[1] == 0U && report.data[2] == 0U);

    CHECK(f.send(10U, 1U, -4, 6, 2U, 254U) == FNR_RECEIVER_ACCEPTED);
    f.service();
    report = f.usb.sent[f.usb.count - 1U];
    CHECK(report.data[0] == 2U && report.data[1] == static_cast<uint8_t>(-4) &&
          report.data[2] == 6U);
}

static void test_real_transport_to_hid_chain() {
    Fixture f;
    f.mount();
    const fnr_datagram input = f.make_datagram(UINT32_C(0x10203040), 0U, -9, 12, 3U);
    fnr_transport_frame frame{};
    CHECK(fnr_transport_make_datagram(&frame, &input, 123U, UINT64_C(456)) == FNR_TRANSPORT_OK);
    fnr_datagram unpacked{};
    CHECK(fnr_transport_unpack_datagram(&frame, &unpacked) == FNR_TRANSPORT_OK);
    CHECK(fnr_receiver_core_accept_datagram(&f.core, &unpacked, 10U) == FNR_RECEIVER_ACCEPTED);
    f.service();
    const auto &out = f.usb.sent[f.usb.count - 1U];
    CHECK(out.data[0] == 3U && out.data[1] == static_cast<uint8_t>(-9) && out.data[2] == 12U);
}

static uint32_t rng_state = UINT32_C(0xd6e8feb8);
static uint32_t rng32() {
    uint32_t x = rng_state;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    rng_state = x;
    return x;
}

static void test_hostile_envelope_property() {
    std::array<uint8_t, 260> bytes{};
    std::array<uint8_t, FNR_DATAGRAM_CAPACITY> encoded{};
    for (std::size_t iteration = 0; iteration < 12000U; ++iteration) {
        const std::size_t length = static_cast<std::size_t>(rng32() % 261U);
        for (std::size_t i = 0; i < length; ++i)
            bytes[i] = static_cast<uint8_t>(rng32());
        fnr_envelope decoded{};
        const auto result = fnr_envelope_decode(bytes.data(), length, &decoded);
        if (result == FNR_ENVELOPE_OK) {
            size_t written = 0U;
            CHECK(fnr_envelope_encode(&decoded, encoded.data(), encoded.size(),
                                      &written) == FNR_ENVELOPE_OK);
            CHECK(written == length);
            CHECK(std::memcmp(encoded.data(), bytes.data(), length) == 0);
        }
    }
}

int main() {
    test_envelope_golden_and_bounds();
    test_profile_payload_contract();
    test_sequence_comparator();
    test_config_and_authorization();
    test_order_profile_and_hid();
    test_core_sequence_wrap();
    test_rejected_messages_do_not_refresh();
    test_transport_event_matrix();
    test_usb_suspend_resume_no_backlog();
    test_session_change_and_retirement();
    test_freshness_wrap_and_nonrefresh();
    test_transport_and_usb_invalidation();
    test_pending_motion_is_retired_by_neutralization();
    test_real_transport_to_hid_chain();
    test_hostile_envelope_property();
    std::printf("PASS: FNR-006 envelope/core/profile/HID integration seed=0xd6e8feb8\n");
    return 0;
}
