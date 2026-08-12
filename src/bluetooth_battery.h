// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bluetooth_battery.h
 * @brief Native BlueZ Battery1 discovery with a non-blocking cache.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_BLUETOOTH_BATTERY_H
#define LINUX_SYSTEM_MONITOR_BLUETOOTH_BATTERY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct _GVariant GVariant;

#define LSM_BLUETOOTH_BATTERY_MAX 16U
#define LSM_BLUETOOTH_ADDRESS_LEN 32U
#define LSM_BLUETOOTH_NAME_LEN 256U
#define LSM_BLUETOOTH_SOURCE_LEN 96U
#define LSM_BLUETOOTH_DETAIL_LEN 128U
#define LSM_BLUETOOTH_PATH_LEN 256U

/** One connected BlueZ device that exports org.bluez.Battery1. */
typedef struct {
    char object_path[LSM_BLUETOOTH_PATH_LEN];
    char address[LSM_BLUETOOTH_ADDRESS_LEN];
    char name[LSM_BLUETOOTH_NAME_LEN];
    char source[LSM_BLUETOOTH_SOURCE_LEN];
    char address_type[32];
    char icon[LSM_BLUETOOTH_DETAIL_LEN];
    char modalias[LSM_BLUETOOTH_DETAIL_LEN];
    double percentage;
    bool connected;
    bool paired;
    bool trusted;
    bool services_resolved;
} LsmBluetoothBatteryRecord;

/**
 * Start the single background BlueZ snapshot worker.
 *
 * The worker performs D-Bus I/O away from the GTK main loop and publishes
 * immutable bounded snapshots under internal synchronisation.
 *
 * @return true when the worker is running or was already started.
 */
bool lsm_bluetooth_battery_start(void);
/**
 * Copy the latest cached BlueZ battery records without blocking on D-Bus.
 *
 * @param [out] records Caller-owned destination array.
 * @param [in] capacity Number of records available in @p records.
 * @return Number of records copied; never greater than @p capacity.
 */
size_t lsm_bluetooth_battery_snapshot(LsmBluetoothBatteryRecord *records,
                                      size_t capacity);
/**
 * Stop and join the BlueZ snapshot worker.
 *
 * This function is idempotent and leaves no project-owned background process.
 */
void lsm_bluetooth_battery_stop(void);

/**
 * Parse ObjectManager.GetManagedObjects output into bounded battery records.
 *
 * Exposed for deterministic protocol regression tests; it performs no D-Bus
 * calls and borrows @p objects for the duration of the call only.
 *
 * @param [in] objects BlueZ ObjectManager result variant.
 * @param [out] records Caller-owned destination array.
 * @param [in] capacity Number of records available in @p records.
 * @return Number of valid Battery1 records written.
 */
size_t lsm_bluetooth_battery_parse_objects(
    GVariant *objects, LsmBluetoothBatteryRecord *records, size_t capacity);

#endif
