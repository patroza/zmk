/*
 * Copyright (c) 2022 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/hid_indicators.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/endpoint_selection_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/split/bluetooth/central.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define NUM_USB_PROFILES (COND_CODE_1(IS_ENABLED(CONFIG_ZMK_USB), (1), (0)))
#define NUM_BLE_PROFILES (COND_CODE_1(IS_ENABLED(CONFIG_ZMK_BLE), (ZMK_BLE_PROFILE_COUNT), (0)))
#define NUM_PROFILES (NUM_USB_PROFILES + NUM_BLE_PROFILES)

static zmk_hid_indicators hid_indicators[NUM_PROFILES];

static int profile_index(enum zmk_endpoint endpoint, uint8_t profile) {
    switch (endpoint) {
    case ZMK_ENDPOINT_USB:
        return 0;
    case ZMK_ENDPOINT_BLE:
        if (profile >= NUM_BLE_PROFILES) {
            return -EINVAL;
        }
        return NUM_USB_PROFILES + profile;
    default:
        return -EINVAL;
    }
}

zmk_hid_indicators zmk_hid_indicators_get_current_profile(void) {
    enum zmk_endpoint endpoint = zmk_endpoints_selected();
    uint8_t profile = 0;

#if IS_ENABLED(CONFIG_ZMK_BLE)
    if (endpoint == ZMK_ENDPOINT_BLE) {
        profile = zmk_ble_active_profile_index();
    }
#endif

    return zmk_hid_indicators_get_profile(endpoint, profile);
}

zmk_hid_indicators zmk_hid_indicators_get_profile(enum zmk_endpoint endpoint, uint8_t profile) {
    int index = profile_index(endpoint, profile);
    if (index < 0) {
        return 0;
    }

    return hid_indicators[index];
}

static void raise_led_changed_event(struct k_work *_work) {
    zmk_hid_indicators indicators = zmk_hid_indicators_get_current_profile();

    ZMK_EVENT_RAISE(new_zmk_hid_indicators_changed(
        (struct zmk_hid_indicators_changed){.indicators = indicators}));

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_BLE)
    zmk_split_bt_update_hid_indicator(indicators);
#endif
}

static K_WORK_DEFINE(led_changed_work, raise_led_changed_event);

void zmk_hid_indicators_set_profile(zmk_hid_indicators indicators, enum zmk_endpoint endpoint,
                                    uint8_t profile) {
    int index = profile_index(endpoint, profile);
    if (index < 0) {
        return;
    }

    // This write is not happening on the main thread. To prevent potential data races, every
    // operation involving hid_indicators must be atomic. Currently, each function either reads
    // or writes only one entry at a time, so it is safe to do these operations without a lock.
    hid_indicators[index] = indicators;

    k_work_submit(&led_changed_work);
}

void zmk_hid_indicators_process_report(struct zmk_hid_led_report_body *report,
                                       enum zmk_endpoint endpoint, uint8_t profile) {
    uint8_t indicators = report->leds;
    zmk_hid_indicators_set_profile(indicators, endpoint, profile);

    LOG_DBG("Update HID indicators: endpoint=%d, profile=%d, indicators=%x", endpoint, profile,
            indicators);
}

static int profile_listener(const zmk_event_t *eh) {
    raise_led_changed_event(NULL);
    return 0;
}

static ZMK_LISTENER(profile_listener, profile_listener);
static ZMK_SUBSCRIPTION(profile_listener, zmk_endpoint_selection_changed);
#if IS_ENABLED(CONFIG_ZMK_BLE)
static ZMK_SUBSCRIPTION(profile_listener, zmk_ble_active_profile_changed);
#endif
