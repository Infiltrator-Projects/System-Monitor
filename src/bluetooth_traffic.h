// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bluetooth_traffic.h
 * @brief Linux HCI controller byte counters and overflow-safe rate accounting.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_BLUETOOTH_TRAFFIC_H
#define LINUX_SYSTEM_MONITOR_BLUETOOTH_TRAFFIC_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stdint.h>

/** Cumulative 32-bit byte counters exposed by Linux HCIGETDEVINFO. */
typedef struct {
    uint32_t rx_bytes;
    uint32_t tx_bytes;
} LsmBluetoothTrafficCounters;

/** Retained counter baseline and extended totals for one HCI controller. */
typedef struct {
    uint32_t previous_rx;
    uint32_t previous_tx;
    uint64_t rx_bytes_total;
    uint64_t tx_bytes_total;
    bool initialized;
} LsmBluetoothTrafficState;

/**
 * Read native HCI traffic counters for a controller such as hci0.
 *
 * @param [in] controller Kernel HCI controller name.
 * @param [out] counters Current cumulative kernel byte counters.
 * @return true when HCIGETDEVINFO returned a complete controller snapshot.
 */
bool lsm_bluetooth_traffic_read(const char *controller,
                                LsmBluetoothTrafficCounters *counters);

/**
 * Apply one HCI counter sample to public Bluetooth throughput.
 *
 * Counter rollback is treated as reset unless it crosses the narrow unsigned
 * wrap boundary. Resets establish a new baseline without a false rate spike.
 *
 * @param [in,out] adapter Published controller metrics.
 * @param [in,out] state Retained private counter state.
 * @param [in] counters Current cumulative HCI byte counters.
 * @param [in] elapsed_seconds Monotonic seconds since the prior sample.
 */
void lsm_bluetooth_traffic_apply(
    LsmBluetoothInfo *adapter, LsmBluetoothTrafficState *state,
    const LsmBluetoothTrafficCounters *counters, double elapsed_seconds);

/**
 * Mark traffic unavailable and invalidate the previous rate baseline.
 *
 * @param [in,out] adapter Published controller metrics.
 * @param [in,out] state Retained private counter state.
 */
void lsm_bluetooth_traffic_mark_unavailable(
    LsmBluetoothInfo *adapter, LsmBluetoothTrafficState *state);

#endif
