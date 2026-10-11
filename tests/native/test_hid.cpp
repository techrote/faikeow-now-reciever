#include "fnr/hid.h"
#include "fnr/hid_relative_mouse.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::abort(); } } while (0)

struct Captured {
    uint8_t id{};
    uint16_t size{};
    std::array<uint8_t, FNR_HID_MAX_REPORT_SIZE> data{};
};

struct Transport {
    bool ready{true};
    bool send_ok{true};
    std::array<Captured, 512> sent{};
    std::size_t count{};
};

static bool ready_cb(void *ctx, uint8_t instance) {
    CHECK(instance == 0U);
    return static_cast<Transport *>(ctx)->ready;
}

static bool send_cb(void *ctx, uint8_t instance, uint8_t report_id,
                    const uint8_t *data, uint16_t size) {
    auto *t = static_cast<Transport *>(ctx);
    CHECK(instance == 0U && report_id == 0U);
    if (!t->send_ok) return false;
    CHECK(data != nullptr && t->count < t->sent.size());
    auto &out = t->sent[t->count++];
    out.id = report_id;
    out.size = size;
    CHECK(static_cast<std::size_t>(size) <= out.data.size());
    std::memcpy(out.data.data(), data, size);
    return true;
}

struct Fixture {
    fnr_hid_device dev{};
    fnr_relative_mouse_state mouse{};
    fnr_hid_profile profile{};
    Transport io{};

    Fixture() {
        fnr_hid_device_init(&dev);
        fnr_relative_mouse_init(&mouse);
        fnr_relative_mouse_make_profile(&mouse, &profile);
        CHECK(fnr_hid_device_register_profile(&dev, &profile) == FNR_HID_STATUS_OK);
        CHECK(fnr_hid_device_select_profile(&dev, "relative_mouse") == FNR_HID_STATUS_OK);
    }

    void service(fnr_hid_status expected = FNR_HID_STATUS_OK) {
        CHECK(fnr_hid_device_service(&dev, ready_cb, send_cb, &io) == expected);
    }

    void mount() {
        CHECK(fnr_hid_device_usb_event(&dev, FNR_HID_USB_MOUNT) == FNR_HID_STATUS_OK);
        CHECK(dev.pending && dev.pending_is_neutral);
        service();
        CHECK(!dev.pending && io.count != 0U);
        const auto &r = io.sent[io.count - 1U];
        CHECK(r.size == 3U && r.data[0] == 0U && r.data[1] == 0U && r.data[2] == 0U);
    }
};

struct Probe {
    uint32_t neutral{};
    std::array<uint32_t, 5> events{};
};

static const fnr_hid_descriptor_set *probe_descriptors(void *) {
    return fnr_relative_mouse_descriptors();
}

static fnr_hid_status probe_report(void *, fnr_hid_report *out) {
    if (out == nullptr) return FNR_HID_STATUS_INVALID_ARGUMENT;
    std::memset(out, 0, sizeof(*out));
    out->size = 3U;
    return FNR_HID_STATUS_OK;
}

static void probe_neutral(void *ctx) { ++static_cast<Probe *>(ctx)->neutral; }
static void probe_event(void *ctx, fnr_hid_usb_event event) {
    auto *p = static_cast<Probe *>(ctx);
    CHECK(static_cast<std::size_t>(event) < p->events.size());
    ++p->events[static_cast<std::size_t>(event)];
}

static const fnr_hid_backend_ops probe_ops = {
    probe_descriptors, probe_report, nullptr, probe_neutral, nullptr, probe_event
};

static const fnr_hid_descriptor_set *bad_descriptors(void *) {
    static const fnr_hid_descriptor_set bad{};
    return &bad;
}
static const fnr_hid_backend_ops bad_ops = {
    bad_descriptors, probe_report, nullptr, probe_neutral, nullptr, probe_event
};

static void test_registry_and_notifications() {
    fnr_hid_device d{};
    fnr_hid_device_init(&d);
    fnr_relative_mouse_state mouse{};
    fnr_relative_mouse_init(&mouse);
    fnr_hid_profile mouse_profile{};
    fnr_relative_mouse_make_profile(&mouse, &mouse_profile);
    CHECK(fnr_hid_device_register_profile(nullptr, &mouse_profile) == FNR_HID_STATUS_INVALID_ARGUMENT);
    CHECK(fnr_hid_device_register_profile(&d, nullptr) == FNR_HID_STATUS_INVALID_ARGUMENT);
    fnr_hid_profile empty{};
    CHECK(fnr_hid_device_register_profile(&d, &empty) == FNR_HID_STATUS_INVALID_ARGUMENT);
    Probe bad_ctx{};
    fnr_hid_profile bad{"bad", &bad_ctx, &bad_ops};
    CHECK(fnr_hid_device_register_profile(&d, &bad) == FNR_HID_STATUS_DESCRIPTOR_INVALID);
    CHECK(fnr_hid_device_register_profile(&d, &mouse_profile) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_register_profile(&d, &mouse_profile) == FNR_HID_STATUS_DUPLICATE_PROFILE);
    CHECK(fnr_hid_device_select_profile(&d, nullptr) == FNR_HID_STATUS_INVALID_ARGUMENT);
    CHECK(fnr_hid_device_select_profile(&d, "missing") == FNR_HID_STATUS_PROFILE_NOT_FOUND);
    CHECK(fnr_hid_device_select_profile(&d, "relative_mouse") == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_selected_profile(&d) == &mouse_profile);
    CHECK(fnr_hid_device_usb_event(&d, FNR_HID_USB_MOUNT) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_register_profile(&d, &mouse_profile) == FNR_HID_STATUS_BUSY);
    CHECK(fnr_hid_device_select_profile(&d, "relative_mouse") == FNR_HID_STATUS_BUSY);

    fnr_hid_device cap{};
    fnr_hid_device_init(&cap);
    Probe cap_ctx{};
    std::array<fnr_hid_profile, 5> profiles{};
    const std::array<const char *, 5> names{{"p0", "p1", "p2", "p3", "p4"}};
    for (std::size_t i = 0U; i < profiles.size(); ++i) {
        profiles[i] = {names[i], &cap_ctx, &probe_ops};
        CHECK(fnr_hid_device_register_profile(&cap, &profiles[i]) ==
              (i < FNR_HID_MAX_PROFILES ? FNR_HID_STATUS_OK : FNR_HID_STATUS_CAPACITY));
    }

    fnr_hid_device n{};
    fnr_hid_device_init(&n);
    Probe probe{};
    fnr_hid_profile p{"probe", &probe, &probe_ops};
    Transport io{};
    CHECK(fnr_hid_device_register_profile(&n, &p) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_select_profile(&n, "probe") == FNR_HID_STATUS_OK);
    CHECK(probe.neutral == 1U);
    CHECK(fnr_hid_device_usb_event(&n, FNR_HID_USB_MOUNT) == FNR_HID_STATUS_OK);
    CHECK(probe.events[FNR_HID_USB_MOUNT] == 1U);
    CHECK(fnr_hid_device_service(&n, ready_cb, send_cb, &io) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_usb_event(&n, FNR_HID_USB_SUSPEND) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_usb_event(&n, FNR_HID_USB_RESUME) == FNR_HID_STATUS_OK);
    CHECK(probe.events[FNR_HID_USB_SUSPEND] == 1U && probe.events[FNR_HID_USB_RESUME] == 1U);
    CHECK(fnr_hid_device_invalidate_output(&n) == FNR_HID_STATUS_OK);
    CHECK(probe.events[FNR_HID_USB_RESET] == 0U); /* Generic invalidation is not a USB event. */
    CHECK(fnr_hid_device_usb_event(&n, FNR_HID_USB_RESET) == FNR_HID_STATUS_OK);
    CHECK(probe.events[FNR_HID_USB_RESET] == 1U);
    CHECK(fnr_hid_device_usb_event(&n, FNR_HID_USB_MOUNT) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_usb_event(&n, FNR_HID_USB_UNMOUNT) == FNR_HID_STATUS_OK);
    CHECK(probe.events[FNR_HID_USB_UNMOUNT] == 1U);
}

static void test_descriptors() {
    const auto *d = fnr_relative_mouse_descriptors();
    CHECK(d != nullptr && d->device_size == 18U && d->configuration_size == 34U);
    CHECK(d->device[0] == 18U && d->device[1] == 0x01U);
    CHECK(d->device[2] == 0x00U && d->device[3] == 0x02U);
    CHECK(d->device[4] == 0U && d->device[5] == 0U && d->device[6] == 0U);
    CHECK(d->device[7] == 64U);
    CHECK(d->device[8] == 0xFEU && d->device[9] == 0xCAU);   /* VID 0xCAFE */
    CHECK(d->device[10] == 0x03U && d->device[11] == 0xF0U); /* PID 0xF003 */
    CHECK(d->device[14] == 1U && d->device[15] == 2U && d->device[16] == 0U);
    CHECK(d->device[17] == 1U && d->string_count == 2U);
    CHECK(std::strcmp(d->strings[0], "FNR Project") == 0);
    CHECK(std::strcmp(d->strings[1], "FNR Relative Mouse (Development)") == 0);

    const uint8_t *c = d->configuration;
    CHECK(c[0] == 9U && c[1] == 0x02U);
    CHECK((static_cast<uint16_t>(c[2]) | (static_cast<uint16_t>(c[3]) << 8U)) == 34U);
    CHECK(c[4] == 1U && c[5] == 1U && c[7] == 0x80U && c[8] == 50U);
    CHECK(c[9] == 9U && c[10] == 0x04U && c[13] == 1U);
    CHECK(c[14] == 0x03U && c[15] == 0x01U && c[16] == 0x02U); /* HID boot mouse */
    CHECK(c[18] == 9U && c[19] == 0x21U && c[23] == 1U && c[24] == 0x22U);
    CHECK(d->report_count == 1U && d->reports[0].size == 50U);
    CHECK((static_cast<uint16_t>(c[25]) | (static_cast<uint16_t>(c[26]) << 8U)) == 50U);
    CHECK(c[27] == 7U && c[28] == 0x05U && c[29] == 0x81U && c[30] == 0x03U);
    CHECK(c[31] == 8U && c[32] == 0U && c[33] == 1U);

    const uint8_t *r = d->reports[0].bytes;
    bool report_id = false;
    uint32_t bits = 0U, size = 0U, count = 0U;
    for (std::size_t i = 0U; i + 1U < d->reports[0].size; i += 2U) {
        if (r[i] == 0x85U) report_id = true;
        if (r[i] == 0x75U) size = r[i + 1U];
        if (r[i] == 0x95U) count = r[i + 1U];
        if (r[i] == 0x81U) bits += size * count;
    }
    CHECK(!report_id && bits == 24U);
    const uint8_t signed_xy[] = {0x15U, 0x81U, 0x25U, 0x7FU, 0x75U, 0x08U, 0x95U, 0x02U, 0x81U, 0x06U};
    bool found = false;
    for (std::size_t i = 0U; i + sizeof(signed_xy) <= d->reports[0].size; ++i) {
        if (std::memcmp(r + i, signed_xy, sizeof(signed_xy)) == 0) found = true;
    }
    CHECK(found);
}

static void test_mouse_reports() {
    Fixture f;
    f.mount();
    const std::array<std::array<bool, 2>, 4> buttons{{{{false, false}}, {{true, false}}, {{false, true}}, {{true, true}}}};
    for (std::size_t i = 0U; i < buttons.size(); ++i) {
        CHECK(fnr_relative_mouse_apply_input(&f.mouse, -127, 127, buttons[i][0], buttons[i][1]) == FNR_HID_STATUS_OK);
        CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_OK);
        f.service();
        const auto &r = f.io.sent[f.io.count - 1U];
        CHECK(r.size == 3U && r.data[0] == static_cast<uint8_t>(i));
        CHECK(r.data[1] == static_cast<uint8_t>(-127) && r.data[2] == 127U);
        CHECK(f.mouse.x == 0 && f.mouse.y == 0);
        CHECK(f.mouse.buttons == static_cast<uint8_t>(i));
    }
    CHECK(fnr_relative_mouse_apply_input(&f.mouse, -1000, 1000, true, false) == FNR_HID_STATUS_CLAMPED);
    CHECK(f.mouse.x == -127 && f.mouse.y == 127 && f.mouse.clamped_axes == 2U);
    fnr_hid_report control{};
    CHECK(fnr_hid_device_control_report(&f.dev, 0U, 0U, &control) == FNR_HID_STATUS_OK);
    CHECK(control.size == 3U && control.data[0] == 1U && control.data[1] == 0U && control.data[2] == 0U);
    CHECK(fnr_hid_device_control_report(&f.dev, 1U, 0U, &control) == FNR_HID_STATUS_INVALID_ARGUMENT);
    fnr_relative_mouse_release_all(&f.mouse);
    CHECK(f.mouse.buttons == 0U && f.mouse.x == 0 && f.mouse.y == 0);
}

static void test_backpressure_and_recovery() {
    Fixture f;
    f.io.ready = false;
    CHECK(fnr_hid_device_usb_event(&f.dev, FNR_HID_USB_MOUNT) == FNR_HID_STATUS_OK);
    f.service(FNR_HID_STATUS_NOT_READY);
    CHECK(fnr_relative_mouse_apply_input(&f.mouse, 10, 20, true, false) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_NEUTRAL_PENDING);
    CHECK(f.dev.pending_is_neutral && f.mouse.x == 0 && f.mouse.y == 0 && f.mouse.buttons == 0U);
    CHECK(f.dev.counters.reports_blocked_by_neutral == 1U);
    f.io.ready = true;
    f.service();
    CHECK(f.io.sent[f.io.count - 1U].data[0] == 0U);

    f.io.ready = false;
    CHECK(fnr_relative_mouse_apply_input(&f.mouse, 10, 20, true, false) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_OK);
    CHECK(fnr_relative_mouse_apply_input(&f.mouse, -7, 9, false, true) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_REPLACED_PENDING);
    f.io.ready = true;
    f.service();
    const auto &latest = f.io.sent[f.io.count - 1U];
    CHECK(latest.data[0] == 2U && latest.data[1] == static_cast<uint8_t>(-7) && latest.data[2] == 9U);

    CHECK(fnr_relative_mouse_apply_input(&f.mouse, 44, 45, true, true) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_OK);
    f.io.send_ok = false;
    f.service(FNR_HID_STATUS_SEND_FAILED);
    CHECK(f.dev.pending && f.dev.counters.send_failures == 1U);
    CHECK(fnr_relative_mouse_apply_input(&f.mouse, 1, 2, false, false) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_REPLACED_PENDING);
    f.io.send_ok = true;
    f.service();
    CHECK(f.io.sent[f.io.count - 1U].data[1] == 1U && f.io.sent[f.io.count - 1U].data[2] == 2U);

    CHECK(fnr_relative_mouse_apply_input(&f.mouse, 25, -30, true, false) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_usb_event(&f.dev, FNR_HID_USB_UNMOUNT) == FNR_HID_STATUS_OK);
    CHECK(!f.dev.pending && f.mouse.buttons == 0U && f.mouse.x == 0 && f.mouse.y == 0);
    f.mount();

    CHECK(fnr_relative_mouse_apply_input(&f.mouse, 0, 0, true, true) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_OK);
    f.service();
    CHECK(f.io.sent[f.io.count - 1U].data[0] == 3U);
    CHECK(fnr_hid_device_usb_event(&f.dev, FNR_HID_USB_SUSPEND) == FNR_HID_STATUS_OK);
    CHECK(fnr_relative_mouse_apply_input(&f.mouse, 99, 99, true, false) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_NOT_READY);
    CHECK(fnr_hid_device_usb_event(&f.dev, FNR_HID_USB_RESUME) == FNR_HID_STATUS_OK);
    f.service();
    CHECK(f.io.sent[f.io.count - 1U].data[0] == 0U && f.io.sent[f.io.count - 1U].data[1] == 0U);

    CHECK(fnr_relative_mouse_apply_input(&f.mouse, -88, 77, false, true) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_usb_event(&f.dev, FNR_HID_USB_RESET) == FNR_HID_STATUS_OK);
    CHECK(!f.dev.pending && f.mouse.buttons == 0U && f.mouse.x == 0 && f.mouse.y == 0);
    f.mount();

    CHECK(fnr_relative_mouse_apply_input(&f.mouse, 3, 4, true, false) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_OK);
    CHECK(fnr_hid_device_invalidate_output(&f.dev) == FNR_HID_STATUS_OK);
    CHECK(f.dev.pending_is_neutral && f.mouse.buttons == 0U);
    f.service();
    CHECK(f.io.sent[f.io.count - 1U].data[0] == 0U && f.io.sent[f.io.count - 1U].data[1] == 0U);
}

static void test_fault_stress() {
    Fixture f;
    for (int cycle = 0; cycle < 100; ++cycle) {
        f.mount();
        const int16_t x = static_cast<int16_t>((cycle % 255) - 127);
        const int16_t y = static_cast<int16_t>(127 - (cycle % 255));
        CHECK(fnr_relative_mouse_apply_input(&f.mouse, x, y, (cycle & 1) != 0, (cycle & 2) != 0) == FNR_HID_STATUS_OK);
        CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_OK);
        if ((cycle % 3) == 0) {
            f.io.ready = false;
            f.service(FNR_HID_STATUS_NOT_READY);
            CHECK(fnr_relative_mouse_apply_input(&f.mouse, 1, -1, false, false) == FNR_HID_STATUS_OK);
            CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_REPLACED_PENDING);
            f.io.ready = true;
        }
        f.service();
        if ((cycle & 1) == 0) {
            CHECK(fnr_hid_device_usb_event(&f.dev, FNR_HID_USB_SUSPEND) == FNR_HID_STATUS_OK);
            CHECK(fnr_relative_mouse_apply_input(&f.mouse, 100, 100, true, true) == FNR_HID_STATUS_OK);
            CHECK(fnr_hid_device_publish_current(&f.dev) == FNR_HID_STATUS_NOT_READY);
            CHECK(fnr_hid_device_usb_event(&f.dev, FNR_HID_USB_RESUME) == FNR_HID_STATUS_OK);
            f.service();
            CHECK(f.io.sent[f.io.count - 1U].data[0] == 0U);
        }
        if ((cycle % 5) == 0) {
            CHECK(fnr_hid_device_note_transport_error(&f.dev) == FNR_HID_STATUS_OK);
            f.service();
            CHECK(f.io.sent[f.io.count - 1U].data[0] == 0U);
        }
        CHECK(fnr_hid_device_usb_event(&f.dev, FNR_HID_USB_UNMOUNT) == FNR_HID_STATUS_OK);
        CHECK(!f.dev.pending && f.mouse.buttons == 0U && f.mouse.x == 0 && f.mouse.y == 0);
    }
    CHECK(f.dev.counters.backpressure_events > 0U);
    CHECK(f.dev.counters.reports_replaced > 0U);
    CHECK(f.dev.counters.transport_failures > 0U);
}

int main() {
    test_registry_and_notifications();
    test_descriptors();
    test_mouse_reports();
    test_backpressure_and_recovery();
    test_fault_stress();
    std::printf("PASS: HID registry, descriptors, relative mouse, recovery and stress\n");
    return 0;
}
