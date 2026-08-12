// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file logitech_hidpp_protocol.h
 * @brief Internal Logitech HID++ battery wire-format helpers.
 *
 * This interface is private to the HID++ transport and its regression tests.
 * It separates byte-level protocol handling from hidraw discovery, I/O and
 * worker lifetime management.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_LOGITECH_HIDPP_PROTOCOL_H
#define LINUX_SYSTEM_MONITOR_LOGITECH_HIDPP_PROTOCOL_H

#include "logitech_hidpp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Initialise a HID++ battery reading to explicit unavailable values.
 *
 * @param [out] reading Record to reset.
 */
void lsm_logitech_hidpp_reset_reading(LsmHidppBatteryReading *reading);

/**
 * Encode one bounded HID++ 2.0 feature-access request.
 *
 * @param [in] feature_index Device-advertised HID++ feature index.
 * @param [in] function Four-bit HID++ function selector.
 * @param [in] parameters Optional request payload.
 * @param [in] parameter_count Number of bytes in @p parameters.
 * @param [out] report Destination HID report buffer.
 * @param [in] report_capacity Capacity of @p report in bytes.
 * @return Encoded report length, or zero when arguments or capacity are invalid.
 */
size_t lsm_logitech_hidpp_format_request(
    uint8_t feature_index, uint8_t function, const uint8_t *parameters,
    size_t parameter_count, uint8_t *report, size_t report_capacity);

/**
 * Decode a Battery Status (feature 0x1000) response payload.
 *
 * @param [in] payload HID++ function payload.
 * @param [in] payload_size Number of readable payload bytes.
 * @param [out] reading Normalised battery reading.
 * @return true when the payload was structurally valid and decoded.
 */
bool lsm_logitech_hidpp_parse_1000(const uint8_t *payload, size_t payload_size,
                                   LsmHidppBatteryReading *reading);
/**
 * Decode a Unified Battery (feature 0x1004) response payload.
 *
 * @param [in] payload HID++ function payload.
 * @param [in] payload_size Number of readable payload bytes.
 * @param [out] reading Normalised battery reading.
 * @return true when the payload was structurally valid and decoded.
 */
bool lsm_logitech_hidpp_parse_1004(const uint8_t *payload, size_t payload_size,
                                   LsmHidppBatteryReading *reading);

#endif
