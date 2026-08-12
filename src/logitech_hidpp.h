// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file logitech_hidpp.h
 * @brief Minimal native Logitech HID++ battery queries over Linux hidraw.
 *
 * The collector uses only read-only battery feature calls and deliberately
 * avoids pairing, configuration and firmware-update operations.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_LOGITECH_HIDPP_H
#define LINUX_SYSTEM_MONITOR_LOGITECH_HIDPP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Parsed result of one HID++ battery feature query. */
typedef struct {
    double percent;
    int next_percent;
    char status[32];
    char level[32];
    char source[48];
    bool exact_percent;
    bool next_percent_available;
} LsmHidppBatteryReading;

/**
 * Resolve the hidraw node associated with a Logitech power-supply device.
 *
 * @param [in] power_supply_path Canonical sysfs power-supply directory.
 * @param [out] device_path Destination for the matching /dev/hidraw path.
 * @param [in] device_path_size Capacity of @p device_path in bytes.
 * @return true when a matching readable hidraw node was found.
 */
bool lsm_logitech_hidpp_find_device(const char *power_supply_path,
                                    char *device_path, size_t device_path_size);

#define LSM_LOGITECH_HIDPP_MAX_DEVICES 16U

/**
 * Start the single HID++ polling worker.
 *
 * @return true when the worker is running or was already started.
 */
bool lsm_logitech_hidpp_start(void);
/**
 * Replace the bounded set of hidraw devices sampled by the worker.
 *
 * Paths are copied; the caller retains ownership of the supplied strings.
 *
 * @param [in] device_paths Array of NUL-terminated hidraw paths.
 * @param [in] count Number of paths, truncated to the project maximum.
 */
void lsm_logitech_hidpp_set_devices(const char *const *device_paths,
                                    size_t count);
/**
 * Copy the latest cached HID++ reading for one device.
 *
 * @param [in] device_path hidraw path used as the cache key.
 * @param [out] reading Destination for the immutable cached record.
 * @return true when a valid current record exists for @p device_path.
 */
bool lsm_logitech_hidpp_snapshot(const char *device_path,
                                 LsmHidppBatteryReading *reading);
/**
 * Stop and join the HID++ polling worker.
 */
void lsm_logitech_hidpp_stop(void);

#endif
