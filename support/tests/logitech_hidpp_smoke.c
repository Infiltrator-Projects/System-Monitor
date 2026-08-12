// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file logitech_hidpp_smoke.c
 * @brief Logitech HID++ battery parser and sysfs mapping regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "logitech_hidpp.h"
#include "logitech_hidpp_protocol.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool join_path(char *destination, size_t size, const char *base,
                      const char *suffix)
{
    const size_t base_length = strlen(base);
    const size_t suffix_length = strlen(suffix);
    if (base_length + suffix_length + 1U > size) return false;
    memcpy(destination, base, base_length);
    memcpy(destination + base_length, suffix, suffix_length + 1U);
    return true;
}

static bool make_directory(const char *path)
{
    return mkdir(path, 0700) == 0 || errno == EEXIST;
}

static bool test_parsers(void)
{
    LsmHidppBatteryReading reading;
    const uint8_t discharging[] = {50U, 20U, 0U};
    if (!lsm_logitech_hidpp_parse_1000(
            discharging, sizeof(discharging), &reading) ||
        !reading.exact_percent || reading.percent != 50.0 ||
        !reading.next_percent_available || reading.next_percent != 20 ||
        strcmp(reading.status, "Discharging") != 0 ||
        strcmp(reading.level, "Normal") != 0)
        return false;

    const uint8_t charging[] = {0U, 0U, 1U};
    if (!lsm_logitech_hidpp_parse_1000(
            charging, sizeof(charging), &reading) ||
        reading.exact_percent || isfinite(reading.percent) ||
        strcmp(reading.status, "Charging") != 0)
        return false;

    const uint8_t full[] = {0U, 0U, 3U};
    if (!lsm_logitech_hidpp_parse_1000(full, sizeof(full), &reading) ||
        reading.exact_percent || isfinite(reading.percent) ||
        strcmp(reading.status, "Full") != 0 ||
        strcmp(reading.level, "Full") != 0)
        return false;

    const uint8_t unified_exact[] = {85U, 0x04U, 0U, 0U};
    if (!lsm_logitech_hidpp_parse_1004(
            unified_exact, sizeof(unified_exact), &reading) ||
        !reading.exact_percent || reading.percent != 85.0 ||
        strcmp(reading.level, "High") != 0 ||
        strcmp(reading.status, "Charging") != 0)
        return false;

    const uint8_t unified_coarse[] = {0U, 0x02U, 2U, 0U};
    return lsm_logitech_hidpp_parse_1004(
               unified_coarse, sizeof(unified_coarse), &reading) &&
           !reading.exact_percent && strcmp(reading.level, "Low") == 0 &&
           strcmp(reading.status, "Discharging") == 0;
}


static bool test_request_framing(void)
{
    uint8_t report[20] = {0};
    const uint8_t parameters[] = {0x10U, 0x00U};
    const size_t size = lsm_logitech_hidpp_format_request(
        0x06U, 0x00U, parameters, sizeof(parameters), report, sizeof(report));
    return size == sizeof(report) && report[0] == 0x11U &&
           report[1] == 0xffU && report[2] == 0x06U &&
           (report[3] & 0xf0U) == 0x00U && report[4] == 0x10U &&
           report[5] == 0x00U;
}

static bool test_hidraw_mapping(void)
{
    char root[] = "/tmp/lsm-hidpp-XXXXXX";
    if (!mkdtemp(root)) return false;

    char devices[512], uhid[512], supplies[512], supply[512];
    char class_root[512], hidraw[512], device_link[512], dev_root[512];
    bool ok = join_path(devices, sizeof(devices), root, "/devices") &&
        join_path(uhid, sizeof(uhid), devices, "/0005:046D:B012.001E") &&
        join_path(supplies, sizeof(supplies), uhid, "/power_supply") &&
        join_path(supply, sizeof(supply), supplies, "/hidpp_battery_26") &&
        join_path(class_root, sizeof(class_root), root, "/class") &&
        join_path(hidraw, sizeof(hidraw), class_root, "/hidraw3") &&
        join_path(device_link, sizeof(device_link), hidraw, "/device") &&
        join_path(dev_root, sizeof(dev_root), root, "/dev") &&
        make_directory(devices) && make_directory(uhid) &&
        make_directory(supplies) && make_directory(supply) &&
        make_directory(class_root) && make_directory(hidraw) &&
        make_directory(dev_root) && symlink(uhid, device_link) == 0 &&
        setenv("LSM_HIDRAW_SYS_ROOT", class_root, 1) == 0 &&
        setenv("LSM_HIDRAW_DEV_ROOT", dev_root, 1) == 0;

    char result[512] = "";
    if (ok)
        ok = lsm_logitech_hidpp_find_device(
                 supply, result, sizeof(result)) &&
             strstr(result, "/dev/hidraw3") != NULL;

    unsetenv("LSM_HIDRAW_SYS_ROOT");
    unsetenv("LSM_HIDRAW_DEV_ROOT");
    unlink(device_link);
    rmdir(hidraw);
    rmdir(class_root);
    rmdir(dev_root);
    rmdir(supply);
    rmdir(supplies);
    rmdir(uhid);
    rmdir(devices);
    rmdir(root);
    return ok;
}

static bool test_worker_lifecycle(void)
{
    if (!lsm_logitech_hidpp_start()) return false;
    lsm_logitech_hidpp_set_devices(NULL, 0U);
    lsm_logitech_hidpp_stop();
    return true;
}

int main(void)
{
    if (!test_parsers() || !test_request_framing() ||
        !test_hidraw_mapping() || !test_worker_lifecycle()) {
        fputs("Logitech HID++ battery regression failed\n", stderr);
        return 1;
    }
    puts("Logitech HID++ battery parsing and mapping passed.");
    return 0;
}
