// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bluetooth_traffic.c
 * @brief Direct Linux HCI byte-counter sampling for Bluetooth throughput.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "bluetooth_traffic.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

static bool controller_index(const char *controller, uint16_t *index)
{
    if (!controller || !index || strncmp(controller, "hci", 3U) != 0 ||
        controller[3] == '\0')
        return false;

    errno = 0;
    char *end = NULL;
    const unsigned long value = strtoul(controller + 3U, &end, 10);
    if (errno != 0 || !end || *end != '\0' || value > UINT16_MAX)
        return false;
    *index = (uint16_t)value;
    return true;
}

static uint64_t add_saturating(uint64_t left, uint64_t right)
{
    return UINT64_MAX - left < right ? UINT64_MAX : left + right;
}

static bool counter_delta(uint32_t current, uint32_t previous,
                          uint64_t *delta)
{
    if (!delta) return false;
    if (current >= previous) {
        *delta = (uint64_t)(current - previous);
        return true;
    }

    if (previous >= UINT32_C(0xf0000000) &&
        current <= UINT32_C(0x0fffffff)) {
        *delta = (uint64_t)(UINT32_MAX - previous) +
                 UINT64_C(1) + (uint64_t)current;
        return true;
    }

    *delta = 0U;
    return false;
}

bool lsm_bluetooth_traffic_read(const char *controller,
                                LsmBluetoothTrafficCounters *counters)
{
    if (!counters) return false;
    memset(counters, 0, sizeof(*counters));

    uint16_t index = 0U;
    if (!controller_index(controller, &index)) return false;

    const int socket_fd = socket(
        AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (socket_fd < 0) return false;

    struct hci_dev_info info;
    memset(&info, 0, sizeof(info));
    info.dev_id = index;
    const int result = ioctl(socket_fd, HCIGETDEVINFO, &info);
    (void)close(socket_fd);
    if (result < 0) return false;

    counters->rx_bytes = info.stat.byte_rx;
    counters->tx_bytes = info.stat.byte_tx;
    return true;
}

void lsm_bluetooth_traffic_apply(
    LsmBluetoothInfo *adapter, LsmBluetoothTrafficState *state,
    const LsmBluetoothTrafficCounters *counters, double elapsed_seconds)
{
    if (!adapter || !state || !counters) return;

    adapter->traffic_available = true;
    adapter->rx_bytes_per_sec = 0.0;
    adapter->tx_bytes_per_sec = 0.0;

    if (!state->initialized) {
        state->rx_bytes_total = counters->rx_bytes;
        state->tx_bytes_total = counters->tx_bytes;
    } else {
        uint64_t rx_delta = 0U;
        uint64_t tx_delta = 0U;
        const bool rx_valid = counter_delta(
            counters->rx_bytes, state->previous_rx, &rx_delta);
        const bool tx_valid = counter_delta(
            counters->tx_bytes, state->previous_tx, &tx_delta);

        if (rx_valid)
            state->rx_bytes_total = add_saturating(
                state->rx_bytes_total, rx_delta);
        else
            state->rx_bytes_total = counters->rx_bytes;
        if (tx_valid)
            state->tx_bytes_total = add_saturating(
                state->tx_bytes_total, tx_delta);
        else
            state->tx_bytes_total = counters->tx_bytes;

        if (isfinite(elapsed_seconds) && elapsed_seconds > 0.0) {
            if (rx_valid)
                adapter->rx_bytes_per_sec =
                    (double)rx_delta / elapsed_seconds;
            if (tx_valid)
                adapter->tx_bytes_per_sec =
                    (double)tx_delta / elapsed_seconds;
        }
    }

    adapter->rx_bytes_total = state->rx_bytes_total;
    adapter->tx_bytes_total = state->tx_bytes_total;
    state->previous_rx = counters->rx_bytes;
    state->previous_tx = counters->tx_bytes;
    state->initialized = true;
}

void lsm_bluetooth_traffic_mark_unavailable(
    LsmBluetoothInfo *adapter, LsmBluetoothTrafficState *state)
{
    if (!adapter || !state) return;
    adapter->traffic_available = false;
    adapter->rx_bytes_per_sec = 0.0;
    adapter->tx_bytes_per_sec = 0.0;
    state->initialized = false;
}
