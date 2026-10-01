// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bluetooth_battery.h
 * @brief Native BlueZ Battery1 discovery with a non-blocking cache.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_BLUETOOTH_BATTERY_H
#define INFILTRATOR_SYSTEM_MONITOR_BLUETOOTH_BATTERY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct _GVariant GVariant;

#define LSM_BLUETOOTH_BATTERY_MAX 16U
#define LSM_BLUETOOTH_ADAPTER_MAX 4U
#define LSM_BLUETOOTH_DEVICE_MAX 32U
#define LSM_BLUETOOTH_CONNECTED_NAMES_LEN 512U
#define LSM_BLUETOOTH_ADDRESS_LEN 32U
#define LSM_BLUETOOTH_NAME_LEN 256U
#define LSM_BLUETOOTH_SOURCE_LEN 96U
#define LSM_BLUETOOTH_DETAIL_LEN 128U
#define LSM_BLUETOOTH_PATH_LEN 256U

/** One BlueZ Adapter1 controller record and its bounded device summary. */
typedef struct {
    char object_path[LSM_BLUETOOTH_PATH_LEN];
    char address[LSM_BLUETOOTH_ADDRESS_LEN];
    char name[LSM_BLUETOOTH_NAME_LEN];
    char alias[LSM_BLUETOOTH_NAME_LEN];
    char connected_names[LSM_BLUETOOTH_CONNECTED_NAMES_LEN];
    unsigned device_count;
    unsigned connected_count;
    unsigned paired_count;
    unsigned trusted_count;
    bool powered;
    bool discoverable;
    bool pairable;
    bool discovering;
} LsmBluetoothAdapterRecord;

/** One BlueZ Device1 record, independent of Battery1 support. */
typedef struct {
    char object_path[LSM_BLUETOOTH_PATH_LEN];
    char adapter_path[LSM_BLUETOOTH_PATH_LEN];
    char controller[64];
    char address[LSM_BLUETOOTH_ADDRESS_LEN];
    char name[LSM_BLUETOOTH_NAME_LEN];
    char alias[LSM_BLUETOOTH_NAME_LEN];
    char address_type[32];
    char icon[LSM_BLUETOOTH_DETAIL_LEN];
    char modalias[LSM_BLUETOOTH_DETAIL_LEN];
    bool connected;
    bool paired;
    bool trusted;
    bool services_resolved;
} LsmBluetoothDeviceRecord;

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
 * Copy the latest cached BlueZ controller records without blocking on D-Bus.
 *
 * @param [out] records Caller-owned destination array.
 * @param [in] capacity Number of records available in @p records.
 * @return Number of controller records copied.
 */
size_t lsm_bluetooth_adapter_snapshot(LsmBluetoothAdapterRecord *records,
                                      size_t capacity);
/**
 * Copy cached BlueZ Device1 records without blocking on D-Bus.
 *
 * @param [out] records Caller-owned destination array.
 * @param [in] capacity Number of records available in @p records.
 * @return Number of Device1 records copied.
 */
size_t lsm_bluetooth_device_snapshot(LsmBluetoothDeviceRecord *records,
                                     size_t capacity);
/**
 * Stop and join the BlueZ snapshot worker.
 *
 * This function is idempotent and leaves no project-owned background process.
 */
void lsm_bluetooth_battery_stop(void);

/**
 * Parse the complete BlueZ ObjectManager controller set.
 *
 * @param [in] objects BlueZ ObjectManager result variant.
 * @param [out] records Caller-owned controller destination array.
 * @param [in] capacity Number of records available in @p records.
 * @param [out] out_count Number of complete records written on success.
 * @return true only when the entire matching set fits and is parsed.
 */
bool lsm_bluetooth_adapter_parse_objects_checked(
    GVariant *objects, LsmBluetoothAdapterRecord *records, size_t capacity,
    size_t *out_count);
/**
 * Parse the complete BlueZ ObjectManager Device1 set.
 *
 * @param [in] objects BlueZ ObjectManager result variant.
 * @param [out] records Caller-owned device destination array.
 * @param [in] capacity Number of records available in @p records.
 * @param [out] out_count Number of complete records written on success.
 * @return true only when the entire matching set fits and is parsed.
 */
bool lsm_bluetooth_device_parse_objects_checked(
    GVariant *objects, LsmBluetoothDeviceRecord *records, size_t capacity,
    size_t *out_count);
/**
 * Parse the complete connected BlueZ Battery1 set.
 *
 * @param [in] objects BlueZ ObjectManager result variant.
 * @param [out] records Caller-owned battery destination array.
 * @param [in] capacity Number of records available in @p records.
 * @param [out] out_count Number of complete records written on success.
 * @return true only when the entire matching set fits and is parsed.
 */
bool lsm_bluetooth_battery_parse_objects_checked(
    GVariant *objects, LsmBluetoothBatteryRecord *records, size_t capacity,
    size_t *out_count);

#endif
