// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bluetooth_traffic_smoke.c
 * @brief HCI throughput, wrap and reset regression coverage.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "bluetooth_traffic.h"

#include <math.h>
#include <stdio.h>

static bool near(double value, double expected)
{
    return fabs(value - expected) < 0.0001;
}

int main(void)
{
    LsmBluetoothInfo adapter = {0};
    LsmBluetoothTrafficState state = {0};

    const LsmBluetoothTrafficCounters first = {
        .rx_bytes = 1000U,
        .tx_bytes = 2000U
    };
    lsm_bluetooth_traffic_apply(&adapter, &state, &first, 1.0);
    if (!adapter.traffic_available || !state.initialized ||
        adapter.rx_bytes_per_sec != 0.0 ||
        adapter.tx_bytes_per_sec != 0.0 ||
        adapter.rx_bytes_total != 1000U ||
        adapter.tx_bytes_total != 2000U)
        return 1;

    const LsmBluetoothTrafficCounters second = {
        .rx_bytes = 5096U,
        .tx_bytes = 10192U
    };
    lsm_bluetooth_traffic_apply(&adapter, &state, &second, 2.0);
    if (!near(adapter.rx_bytes_per_sec, 2048.0) ||
        !near(adapter.tx_bytes_per_sec, 4096.0) ||
        adapter.rx_bytes_total != 5096U ||
        adapter.tx_bytes_total != 10192U)
        return 2;

    state.previous_rx = UINT32_C(0xfffffff0);
    state.previous_tx = UINT32_C(0xfffffff8);
    state.rx_bytes_total = 100000U;
    state.tx_bytes_total = 200000U;
    state.initialized = true;
    const LsmBluetoothTrafficCounters wrapped = {
        .rx_bytes = UINT32_C(0x20),
        .tx_bytes = UINT32_C(0x18)
    };
    lsm_bluetooth_traffic_apply(&adapter, &state, &wrapped, 1.0);
    if (!near(adapter.rx_bytes_per_sec, 48.0) ||
        !near(adapter.tx_bytes_per_sec, 32.0) ||
        adapter.rx_bytes_total != 100048U ||
        adapter.tx_bytes_total != 200032U)
        return 3;

    state.previous_rx = 100000U;
    state.previous_tx = 200000U;
    state.rx_bytes_total = 500000U;
    state.tx_bytes_total = 600000U;
    state.initialized = true;
    const LsmBluetoothTrafficCounters reset = {
        .rx_bytes = 10U,
        .tx_bytes = 20U
    };
    lsm_bluetooth_traffic_apply(&adapter, &state, &reset, 1.0);
    if (adapter.rx_bytes_per_sec != 0.0 ||
        adapter.tx_bytes_per_sec != 0.0 ||
        adapter.rx_bytes_total != 10U ||
        adapter.tx_bytes_total != 20U)
        return 4;

    lsm_bluetooth_traffic_mark_unavailable(&adapter, &state);
    if (adapter.traffic_available || state.initialized ||
        adapter.rx_bytes_per_sec != 0.0 ||
        adapter.tx_bytes_per_sec != 0.0)
        return 5;

    const LsmBluetoothTrafficCounters recovered = {
        .rx_bytes = 30U,
        .tx_bytes = 50U
    };
    lsm_bluetooth_traffic_apply(&adapter, &state, &recovered, 1.0);
    if (!adapter.traffic_available ||
        adapter.rx_bytes_per_sec != 0.0 ||
        adapter.tx_bytes_per_sec != 0.0 ||
        adapter.rx_bytes_total != 30U ||
        adapter.tx_bytes_total != 50U)
        return 6;

    LsmBluetoothTrafficCounters invalid = {0};
    if (lsm_bluetooth_traffic_read("not-hci", &invalid))
        return 7;

    puts("Bluetooth HCI throughput, wrap and reset handling passed.");
    return 0;
}
