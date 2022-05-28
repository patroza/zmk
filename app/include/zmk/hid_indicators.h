/*
 * Copyright (c) 2022 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <zmk/endpoints_types.h>
#include <zmk/hid.h>
#include <zmk/hid_indicators_types.h>

zmk_hid_indicators zmk_hid_indicators_get_current_profile(void);
zmk_hid_indicators zmk_hid_indicators_get_profile(enum zmk_endpoint endpoint, uint8_t profile);
void zmk_hid_indicators_set_profile(zmk_hid_indicators indicators, enum zmk_endpoint endpoint,
                                    uint8_t profile);

void zmk_hid_indicators_process_report(struct zmk_hid_led_report_body *report,
                                       enum zmk_endpoint endpoint, uint8_t profile);