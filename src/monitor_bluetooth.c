// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_bluetooth.c
 * @brief Bluetooth device membership and retained traffic accounting.
 *
 * BlueZ-facing device enumeration remains with the battery/device collector,
 * while this module owns monitor-level identity reconciliation and per-device
 * traffic baselines. Raw HCI accounting remains in bluetooth_traffic.c.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_linux_internal.h"

#include "common.h"

#include <string.h>

static bool bluetooth_device_state_matches(
    const LsmLinuxBluetoothDeviceState *state,
    const LsmBluetoothDeviceInfo *device)
{
    return state && device &&
        strcmp(state->controller, device->controller) == 0 &&
        strcmp(state->address, device->address) == 0;
}

static LsmLinuxBluetoothDeviceState *find_bluetooth_device_state(
    LsmLinuxMonitorBackendState *state,
    const LsmBluetoothDeviceInfo *device)
{
    if (!state || !device) return NULL;
    for (size_t index = 0U; index < state->bluetooth_device_count; index++)
        if (bluetooth_device_state_matches(
                &state->bluetooth_devices[index], device))
            return &state->bluetooth_devices[index];
    return NULL;
}

void lsm_monitor_bluetooth_reconcile_states(LsmMonitor *monitor)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return;

    LsmLinuxBluetoothDeviceState next[LSM_MAX_BLUETOOTH_DEVICES] = {0};
    const size_t count =
        monitor->bluetooth_device_count < LSM_MAX_BLUETOOTH_DEVICES
            ? monitor->bluetooth_device_count : LSM_MAX_BLUETOOTH_DEVICES;
    for (size_t index = 0U; index < count; index++) {
        const LsmBluetoothDeviceInfo *device =
            &monitor->bluetooth_devices[index];
        LsmLinuxBluetoothDeviceState *old =
            find_bluetooth_device_state(state, device);
        if (old) next[index] = *old;
        lsm_copy_string(next[index].controller,
                        sizeof(next[index].controller),
                        device->controller);
        lsm_copy_string(next[index].address, sizeof(next[index].address),
                        device->address);
    }
    memcpy(state->bluetooth_devices, next, sizeof(next));
    state->bluetooth_device_count = count;

    /* Handle-to-address membership changes with Bluetooth topology, not with
     * every byte-rate sample. Refreshing here avoids opening a raw HCI socket
     * and issuing HCIGETCONNLIST once per controller every second. */
    for (size_t index = 0U; index < monitor->bluetooth_count; index++)
        (void)lsm_bluetooth_traffic_refresh_connections(
            monitor->bluetooth[index].name);
}

void lsm_monitor_bluetooth_update_traffic(LsmMonitor *monitor, double elapsed)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return;

    const size_t count =
        monitor->bluetooth_device_count < state->bluetooth_device_count
            ? monitor->bluetooth_device_count
            : state->bluetooth_device_count;
    for (size_t index = 0U; index < count; index++) {
        LsmBluetoothDeviceInfo *device =
            &monitor->bluetooth_devices[index];
        LsmBluetoothTrafficCounters counters;
        if (lsm_bluetooth_traffic_read_device(
                device->controller, device->address, &counters)) {
            lsm_bluetooth_traffic_apply_device(
                device, &state->bluetooth_devices[index].accounting,
                &counters, elapsed);
        } else {
            lsm_bluetooth_traffic_mark_device_unavailable(
                device, &state->bluetooth_devices[index].accounting);
        }
    }
}

bool lsm_monitor_bluetooth_membership_changed(
    const LsmBluetoothDeviceInfo *old_records,
    size_t old_count,
    const LsmMonitor *monitor)
{
    if (!monitor || old_count != monitor->bluetooth_device_count) return true;
    for (size_t index = 0U; index < monitor->bluetooth_device_count; index++) {
        const LsmBluetoothDeviceInfo *current =
            &monitor->bluetooth_devices[index];
        bool found = false;
        for (size_t old_index = 0U; old_index < old_count; old_index++) {
            const LsmBluetoothDeviceInfo *old = &old_records[old_index];
            if (strcmp(current->controller, old->controller) == 0 &&
                strcmp(current->address, old->address) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return true;
    }
    return false;
}
