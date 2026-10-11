#include "fnr/hid.h"

#include <string.h>

static bool fnr_hid_descriptor_set_valid(const fnr_hid_descriptor_set *descriptors) {
    if (descriptors == NULL || descriptors->device == NULL ||
        descriptors->device_size == 0U || descriptors->configuration == NULL ||
        descriptors->configuration_size == 0U || descriptors->reports == NULL ||
        descriptors->report_count == 0U ||
        descriptors->report_count > FNR_HID_MAX_INTERFACES) {
        return false;
    }
    if (descriptors->string_count > 0U && descriptors->strings == NULL) {
        return false;
    }
    for (uint8_t i = 0U; i < descriptors->report_count; ++i) {
        if (descriptors->reports[i].bytes == NULL || descriptors->reports[i].size == 0U) {
            return false;
        }
    }
    return true;
}

static fnr_hid_status fnr_hid_profile_status(const fnr_hid_profile *profile) {
    if (profile == NULL || profile->name == NULL || profile->name[0] == '\0' ||
        profile->ops == NULL || profile->ops->descriptors == NULL ||
        profile->ops->build_report == NULL || profile->ops->neutralize == NULL) {
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    return fnr_hid_descriptor_set_valid(profile->ops->descriptors(profile->context))
               ? FNR_HID_STATUS_OK
               : FNR_HID_STATUS_DESCRIPTOR_INVALID;
}

static fnr_hid_status fnr_hid_validate_report(const fnr_hid_device *device,
                                              const fnr_hid_report *report) {
    const fnr_hid_descriptor_set *descriptors = fnr_hid_device_descriptors(device);
    if (descriptors == NULL || report == NULL) {
        return FNR_HID_STATUS_REPORT_INVALID;
    }
    if (report->instance >= descriptors->report_count || report->size == 0U ||
        report->size > FNR_HID_MAX_REPORT_SIZE) {
        return FNR_HID_STATUS_REPORT_INVALID;
    }
    return FNR_HID_STATUS_OK;
}

static fnr_hid_status fnr_hid_queue_current(fnr_hid_device *device, bool neutral) {
    if (!neutral && device->pending && device->pending_is_neutral) {
        device->selected->ops->neutralize(device->selected->context);
        ++device->counters.reports_blocked_by_neutral;
        device->last_status = FNR_HID_STATUS_NEUTRAL_PENDING;
        return FNR_HID_STATUS_NEUTRAL_PENDING;
    }

    fnr_hid_report report;
    memset(&report, 0, sizeof(report));
    fnr_hid_status status = device->selected->ops->build_report(device->selected->context, &report);
    if (status != FNR_HID_STATUS_OK) {
        device->last_status = status;
        return status;
    }
    status = fnr_hid_validate_report(device, &report);
    if (status != FNR_HID_STATUS_OK) {
        device->last_status = status;
        return status;
    }

    const bool replaced = device->pending;
    if (replaced) {
        ++device->counters.reports_replaced;
    }
    device->pending_report = report;
    device->pending = true;
    device->pending_is_neutral = neutral;
    ++device->counters.reports_queued;
    if (neutral) {
        ++device->counters.neutral_reports_queued;
    }
    device->last_status = replaced ? FNR_HID_STATUS_REPLACED_PENDING : FNR_HID_STATUS_OK;
    return device->last_status;
}

static void fnr_hid_drop_and_neutralize(fnr_hid_device *device,
                                        bool notify_usb_event,
                                        fnr_hid_usb_event event) {
    device->pending = false;
    device->pending_is_neutral = false;
    memset(&device->pending_report, 0, sizeof(device->pending_report));
    if (device->selected != NULL) {
        device->selected->ops->neutralize(device->selected->context);
        if (notify_usb_event && device->selected->ops->on_usb_event != NULL) {
            device->selected->ops->on_usb_event(device->selected->context, event);
        }
    }
    ++device->counters.invalidations;
}

void fnr_hid_device_init(fnr_hid_device *device) {
    if (device == NULL) {
        return;
    }
    memset(device, 0, sizeof(*device));
    device->usb_state = FNR_HID_USB_STATE_DETACHED;
    device->last_status = FNR_HID_STATUS_OK;
}

fnr_hid_status fnr_hid_device_register_profile(fnr_hid_device *device,
                                               const fnr_hid_profile *profile) {
    if (device == NULL) {
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    const fnr_hid_status profile_status = fnr_hid_profile_status(profile);
    if (profile_status != FNR_HID_STATUS_OK) {
        device->last_status = profile_status;
        return profile_status;
    }
    if (device->usb_state != FNR_HID_USB_STATE_DETACHED) {
        device->last_status = FNR_HID_STATUS_BUSY;
        return FNR_HID_STATUS_BUSY;
    }
    for (uint8_t i = 0U; i < device->profile_count; ++i) {
        if (strcmp(device->profiles[i]->name, profile->name) == 0) {
            device->last_status = FNR_HID_STATUS_DUPLICATE_PROFILE;
            return FNR_HID_STATUS_DUPLICATE_PROFILE;
        }
    }
    if (device->profile_count >= FNR_HID_MAX_PROFILES) {
        device->last_status = FNR_HID_STATUS_CAPACITY;
        return FNR_HID_STATUS_CAPACITY;
    }
    device->profiles[device->profile_count] = profile;
    ++device->profile_count;
    device->last_status = FNR_HID_STATUS_OK;
    return FNR_HID_STATUS_OK;
}

fnr_hid_status fnr_hid_device_select_profile(fnr_hid_device *device,
                                             const char *name) {
    if (device == NULL || name == NULL || name[0] == '\0') {
        if (device != NULL) {
            device->last_status = FNR_HID_STATUS_INVALID_ARGUMENT;
        }
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    if (device->usb_state != FNR_HID_USB_STATE_DETACHED) {
        device->last_status = FNR_HID_STATUS_BUSY;
        return FNR_HID_STATUS_BUSY;
    }
    for (uint8_t i = 0U; i < device->profile_count; ++i) {
        if (strcmp(device->profiles[i]->name, name) == 0) {
            device->selected = device->profiles[i];
            device->pending = false;
            device->pending_is_neutral = false;
            memset(&device->pending_report, 0, sizeof(device->pending_report));
            device->selected->ops->neutralize(device->selected->context);
            device->last_status = FNR_HID_STATUS_OK;
            return FNR_HID_STATUS_OK;
        }
    }
    device->last_status = FNR_HID_STATUS_PROFILE_NOT_FOUND;
    return FNR_HID_STATUS_PROFILE_NOT_FOUND;
}

const fnr_hid_profile *fnr_hid_device_selected_profile(const fnr_hid_device *device) {
    return device == NULL ? NULL : device->selected;
}

const fnr_hid_descriptor_set *fnr_hid_device_descriptors(const fnr_hid_device *device) {
    if (device == NULL || device->selected == NULL) {
        return NULL;
    }
    return device->selected->ops->descriptors(device->selected->context);
}

fnr_hid_status fnr_hid_device_usb_event(fnr_hid_device *device,
                                        fnr_hid_usb_event event) {
    if (device == NULL || event > FNR_HID_USB_RESUME) {
        if (device != NULL) {
            device->last_status = FNR_HID_STATUS_INVALID_ARGUMENT;
        }
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    if (device->selected == NULL) {
        device->last_status = FNR_HID_STATUS_PROFILE_NOT_SELECTED;
        return FNR_HID_STATUS_PROFILE_NOT_SELECTED;
    }

    switch (event) {
        case FNR_HID_USB_RESET:
            device->usb_state = FNR_HID_USB_STATE_DETACHED;
            fnr_hid_drop_and_neutralize(device, true, event);
            device->last_status = FNR_HID_STATUS_OK;
            return FNR_HID_STATUS_OK;
        case FNR_HID_USB_UNMOUNT:
            device->usb_state = FNR_HID_USB_STATE_DETACHED;
            fnr_hid_drop_and_neutralize(device, true, event);
            device->last_status = FNR_HID_STATUS_OK;
            return FNR_HID_STATUS_OK;
        case FNR_HID_USB_SUSPEND:
            device->usb_state = FNR_HID_USB_STATE_SUSPENDED;
            fnr_hid_drop_and_neutralize(device, true, event);
            device->last_status = FNR_HID_STATUS_OK;
            return FNR_HID_STATUS_OK;
        case FNR_HID_USB_MOUNT:
        case FNR_HID_USB_RESUME:
            device->usb_state = FNR_HID_USB_STATE_MOUNTED;
            fnr_hid_drop_and_neutralize(device, true, event);
            return fnr_hid_queue_current(device, true);
        default:
            device->last_status = FNR_HID_STATUS_INVALID_ARGUMENT;
            return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
}

fnr_hid_status fnr_hid_device_publish_current(fnr_hid_device *device) {
    if (device == NULL) {
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    if (device->selected == NULL) {
        device->last_status = FNR_HID_STATUS_PROFILE_NOT_SELECTED;
        return FNR_HID_STATUS_PROFILE_NOT_SELECTED;
    }
    if (device->usb_state != FNR_HID_USB_STATE_MOUNTED) {
        device->last_status = FNR_HID_STATUS_NOT_READY;
        return FNR_HID_STATUS_NOT_READY;
    }
    return fnr_hid_queue_current(device, false);
}

fnr_hid_status fnr_hid_device_invalidate_output(fnr_hid_device *device) {
    if (device == NULL) {
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    if (device->selected == NULL) {
        device->last_status = FNR_HID_STATUS_PROFILE_NOT_SELECTED;
        return FNR_HID_STATUS_PROFILE_NOT_SELECTED;
    }
    fnr_hid_drop_and_neutralize(device, false, FNR_HID_USB_RESET);
    if (device->usb_state == FNR_HID_USB_STATE_MOUNTED) {
        return fnr_hid_queue_current(device, true);
    }
    device->last_status = FNR_HID_STATUS_OK;
    return FNR_HID_STATUS_OK;
}

fnr_hid_status fnr_hid_device_note_transport_error(fnr_hid_device *device) {
    if (device == NULL) {
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    ++device->counters.transport_failures;
    return fnr_hid_device_invalidate_output(device);
}

fnr_hid_status fnr_hid_device_service(fnr_hid_device *device,
                                      fnr_hid_ready_fn ready,
                                      fnr_hid_send_fn send,
                                      void *transport_context) {
    if (device == NULL || ready == NULL || send == NULL) {
        if (device != NULL) {
            device->last_status = FNR_HID_STATUS_INVALID_ARGUMENT;
        }
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    if (!device->pending) {
        device->last_status = FNR_HID_STATUS_OK;
        return FNR_HID_STATUS_OK;
    }
    if (device->selected == NULL) {
        device->last_status = FNR_HID_STATUS_PROFILE_NOT_SELECTED;
        return FNR_HID_STATUS_PROFILE_NOT_SELECTED;
    }
    if (device->usb_state != FNR_HID_USB_STATE_MOUNTED) {
        device->last_status = FNR_HID_STATUS_NOT_READY;
        return FNR_HID_STATUS_NOT_READY;
    }
    if (!ready(transport_context, device->pending_report.instance)) {
        ++device->counters.backpressure_events;
        device->last_status = FNR_HID_STATUS_NOT_READY;
        return FNR_HID_STATUS_NOT_READY;
    }
    if (!send(transport_context, device->pending_report.instance,
              device->pending_report.report_id, device->pending_report.data,
              device->pending_report.size)) {
        ++device->counters.send_failures;
        device->last_status = FNR_HID_STATUS_SEND_FAILED;
        return FNR_HID_STATUS_SEND_FAILED;
    }

    const fnr_hid_report submitted = device->pending_report;
    device->pending = false;
    device->pending_is_neutral = false;
    memset(&device->pending_report, 0, sizeof(device->pending_report));
    ++device->counters.reports_submitted;
    if (device->selected->ops->on_report_submitted != NULL) {
        device->selected->ops->on_report_submitted(device->selected->context, &submitted);
    }
    device->last_status = FNR_HID_STATUS_OK;
    return FNR_HID_STATUS_OK;
}

fnr_hid_status fnr_hid_device_control_report(const fnr_hid_device *device,
                                             uint8_t instance,
                                             uint8_t report_id,
                                             fnr_hid_report *out) {
    if (device == NULL || out == NULL) {
        return FNR_HID_STATUS_INVALID_ARGUMENT;
    }
    if (device->selected == NULL) {
        return FNR_HID_STATUS_PROFILE_NOT_SELECTED;
    }
    if (device->selected->ops->build_control_report == NULL) {
        return FNR_HID_STATUS_REPORT_INVALID;
    }
    memset(out, 0, sizeof(*out));
    const fnr_hid_status status = device->selected->ops->build_control_report(
        device->selected->context, instance, report_id, out);
    if (status != FNR_HID_STATUS_OK) {
        return status;
    }
    return fnr_hid_validate_report(device, out);
}
