// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file logitech_hidpp_protocol.c
 * @brief Logitech HID++ 2.0 battery request and response encoding.
 *
 * The module contains no device access or worker state. Keeping the wire
 * format isolated makes protocol changes independently testable and prevents
 * hidraw transport details from leaking into battery-value decoding.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "logitech_hidpp_protocol.h"

#include "common.h"

#include <math.h>
#include <string.h>

#define LSM_HIDPP_REPORT_ID_LONG 0x11U
#define LSM_HIDPP_REPORT_SIZE_LONG 20U
#define LSM_HIDPP_DEVICE_DIRECT 0xffU
#define LSM_HIDPP_SOFTWARE_ID 0x0cU

void lsm_logitech_hidpp_reset_reading(LsmHidppBatteryReading *reading)
{
    memset(reading, 0, sizeof(*reading));
    reading->percent = NAN;
    reading->next_percent = -1;
}

static void set_level_from_percent(double percent, char *level, size_t size)
{
    const char *text = "Unknown";
    if (percent >= 100.0) text = "Full";
    else if (percent >= 81.0) text = "High";
    else if (percent >= 31.0) text = "Normal";
    else if (percent >= 11.0) text = "Low";
    else if (percent >= 0.0) text = "Critical";
    lsm_copy_string(level, size, text);
}

static void set_battery_status(uint8_t status, char *text, size_t size)
{
    const char *value = "Unknown";
    switch (status) {
        case 0U: value = "Discharging"; break;
        case 1U:
        case 2U: value = "Charging"; break;
        case 3U: value = "Full"; break;
        case 4U: value = "Charging slowly"; break;
        case 5U: value = "Invalid battery"; break;
        case 6U: value = "Thermal error"; break;
        case 7U: value = "Charging error"; break;
        default: break;
    }
    lsm_copy_string(text, size, value);
}

static void set_unified_battery_status(uint8_t status, char *text,
                                       size_t size)
{
    const char *value = "Unknown";
    switch (status) {
        case 0U: value = "Charging"; break;
        case 1U: value = "Full"; break;
        case 2U: value = "Discharging"; break;
        case 7U: value = "Charging error"; break;
        default: break;
    }
    lsm_copy_string(text, size, value);
}

bool lsm_logitech_hidpp_parse_1000(const uint8_t *payload, size_t payload_size,
                                   LsmHidppBatteryReading *reading)
{
    if (!payload || payload_size < 3U || !reading) return false;
    lsm_logitech_hidpp_reset_reading(reading);

    const uint8_t percent = payload[0];
    const uint8_t next_percent = payload[1];
    const uint8_t status = payload[2];
    set_battery_status(status, reading->status, sizeof(reading->status));

    if (percent > 0U && percent <= 100U) {
        reading->percent = (double)percent;
        reading->exact_percent = true;
    } else if (status == 3U) {
        lsm_copy_string(reading->level, sizeof(reading->level), "Full");
    }
    if (next_percent > 0U && next_percent <= 100U) {
        reading->next_percent = (int)next_percent;
        reading->next_percent_available = true;
    }
    if (reading->exact_percent)
        set_level_from_percent(reading->percent, reading->level,
                               sizeof(reading->level));
    return true;
}

bool lsm_logitech_hidpp_parse_1004(const uint8_t *payload, size_t payload_size,
                                   LsmHidppBatteryReading *reading)
{
    if (!payload || payload_size < 4U || !reading) return false;
    lsm_logitech_hidpp_reset_reading(reading);

    const uint8_t state_of_charge = payload[0];
    const uint8_t level_bits = payload[1];
    const uint8_t charging_status = payload[2];
    set_unified_battery_status(charging_status, reading->status,
                               sizeof(reading->status));

    if (state_of_charge > 0U && state_of_charge <= 100U) {
        reading->percent = (double)state_of_charge;
        reading->exact_percent = true;
        set_level_from_percent(reading->percent, reading->level,
                               sizeof(reading->level));
    } else if (level_bits & 0x08U) {
        lsm_copy_string(reading->level, sizeof(reading->level), "Full");
    } else if (level_bits & 0x04U) {
        lsm_copy_string(reading->level, sizeof(reading->level), "Normal");
    } else if (level_bits & 0x02U) {
        lsm_copy_string(reading->level, sizeof(reading->level), "Low");
    } else if (level_bits & 0x01U) {
        lsm_copy_string(reading->level, sizeof(reading->level), "Critical");
    }
    return true;
}

size_t lsm_logitech_hidpp_format_request(
    uint8_t feature_index, uint8_t function, const uint8_t *parameters,
    size_t parameter_count, uint8_t *report, size_t report_capacity)
{
    /* HID++ 2.0 feature-access protocol uses long (0x11) reports even when
     * the request has no parameters. Short reports belong to HID++ 1.0/RAP.
     * Several Bluetooth devices, including the original MX Master, silently
     * ignore a feature-access request sent as a 7-byte short report. */
    if (!report || report_capacity < LSM_HIDPP_REPORT_SIZE_LONG ||
        (parameter_count > 0U && !parameters) ||
        parameter_count > LSM_HIDPP_REPORT_SIZE_LONG - 4U)
        return 0U;
    memset(report, 0, LSM_HIDPP_REPORT_SIZE_LONG);
    report[0] = LSM_HIDPP_REPORT_ID_LONG;
    report[1] = LSM_HIDPP_DEVICE_DIRECT;
    report[2] = feature_index;
    report[3] = (uint8_t)((function & 0xf0U) | LSM_HIDPP_SOFTWARE_ID);
    if (parameters && parameter_count > 0U)
        memcpy(&report[4], parameters, parameter_count);
    return LSM_HIDPP_REPORT_SIZE_LONG;
}

