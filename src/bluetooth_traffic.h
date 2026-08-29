// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bluetooth_traffic.h
 * @brief Per-device Linux HCI monitor traffic attribution.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_BLUETOOTH_TRAFFIC_H
#define LINUX_SYSTEM_MONITOR_BLUETOOTH_TRAFFIC_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Startup result for the privileged HCI-monitor handoff. */
typedef enum {
    LSM_BLUETOOTH_TRAFFIC_SECURITY_FAILURE = -1,
    LSM_BLUETOOTH_TRAFFIC_UNAVAILABLE = 0,
    LSM_BLUETOOTH_TRAFFIC_STARTED = 1
} LsmBluetoothTrafficStartResult;

/** One decoded data frame from the kernel HCI monitor channel. */
typedef struct {
    uint16_t controller_index;
    uint16_t handle;
    uint64_t payload_bytes;
    bool receive;
} LsmBluetoothMonitorPacket;

/** Aggregated packet counters for one remote Bluetooth device. */
typedef struct {
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    unsigned link_count;
} LsmBluetoothTrafficCounters;

/** Retained rate baseline for one remote Bluetooth device. */
typedef struct {
    uint64_t previous_rx;
    uint64_t previous_tx;
    bool initialized;
} LsmBluetoothTrafficState;

/**
 * Open the read-only Linux HCI monitor channel and immediately drop all
 * process capabilities before starting the packet reader.
 *
 * @return STARTED when exact per-device capture is active, UNAVAILABLE when
 *         the monitor channel cannot be opened, or SECURITY_FAILURE if the
 *         process could not discard its capabilities.
 */
LsmBluetoothTrafficStartResult lsm_bluetooth_traffic_start(void);

/** Stop and join the HCI monitor packet reader. */
void lsm_bluetooth_traffic_stop(void);

/**
 * Decode one kernel HCI monitor frame into a per-handle byte event.
 *
 * Command/event/control monitor records return false because they contain no
 * device data bytes to account.
 *
 * @param [in] buffer Complete monitor datagram.
 * @param [in] length Datagram size in bytes.
 * @param [out] packet Decoded data-packet event.
 * @return true when the datagram contains valid ACL, SCO or ISO device data.
 */
bool lsm_bluetooth_traffic_parse_monitor(
    const void *buffer, size_t length, LsmBluetoothMonitorPacket *packet);

/**
 * Refresh current HCI handle-to-address mappings for one controller.
 *
 * This uses the read-only HCIGETCONNLIST ioctl and does not create, modify or
 * tear down any Bluetooth connection.
 *
 * @param [in] controller Kernel HCI controller name, such as hci0.
 * @return true when the kernel returned the active connection list.
 */
bool lsm_bluetooth_traffic_refresh_connections(const char *controller);

/**
 * Snapshot all captured bytes currently attributed to one connected device.
 *
 * @param [in] controller Owning kernel HCI controller name.
 * @param [in] address Remote Bluetooth address.
 * @param [out] counters Aggregated captured byte counters and active links.
 * @return true when exact capture is active and the device has an active link.
 */
bool lsm_bluetooth_traffic_read_device(
    const char *controller, const char *address,
    LsmBluetoothTrafficCounters *counters);

/**
 * Convert cumulative per-device counters to rates and publish them.
 *
 * @param [in,out] device Public connected-device snapshot.
 * @param [in,out] state Retained rate baseline for this device.
 * @param [in] counters Current exact cumulative packet counters.
 * @param [in] elapsed_seconds Monotonic interval since the previous sample.
 */
void lsm_bluetooth_traffic_apply_device(
    LsmBluetoothDeviceInfo *device, LsmBluetoothTrafficState *state,
    const LsmBluetoothTrafficCounters *counters, double elapsed_seconds);

/**
 * Mark exact device traffic unavailable and reset its rate baseline.
 *
 * @param [in,out] device Public connected-device snapshot.
 * @param [in,out] state Retained rate baseline for this device.
 */
void lsm_bluetooth_traffic_mark_device_unavailable(
    LsmBluetoothDeviceInfo *device, LsmBluetoothTrafficState *state);

#endif
