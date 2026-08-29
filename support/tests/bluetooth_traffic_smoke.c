// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bluetooth_traffic_smoke.c
 * @brief Per-device HCI monitor frame and rate-accounting regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "bluetooth_traffic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool near(double value, double expected)
{
    return fabs(value - expected) < 0.0001;
}

static void put_le16(unsigned char *bytes, uint16_t value)
{
    bytes[0] = (unsigned char)(value & 0xffU);
    bytes[1] = (unsigned char)(value >> 8U);
}

int main(void)
{
    unsigned char acl[14] = {0};
    put_le16(acl, 4U);
    put_le16(acl + 2U, 2U);
    put_le16(acl + 4U, 8U);
    put_le16(acl + 6U, 0x2005U);
    put_le16(acl + 8U, 4U);
    memset(acl + 10U, 0xaa, 4U);

    LsmBluetoothMonitorPacket packet = {0};
    if (!lsm_bluetooth_traffic_parse_monitor(acl, sizeof(acl), &packet) ||
        packet.controller_index != 2U || packet.handle != 5U ||
        packet.payload_bytes != 4U || packet.receive)
        return 1;

    put_le16(acl, 5U);
    if (!lsm_bluetooth_traffic_parse_monitor(acl, sizeof(acl), &packet) ||
        !packet.receive || packet.payload_bytes != 4U)
        return 2;

    unsigned char sco[12] = {0};
    put_le16(sco, 7U);
    put_le16(sco + 2U, 0U);
    put_le16(sco + 4U, 6U);
    put_le16(sco + 6U, 0x0007U);
    sco[8U] = 3U;
    memset(sco + 9U, 0xbb, 3U);
    if (!lsm_bluetooth_traffic_parse_monitor(sco, sizeof(sco), &packet) ||
        !packet.receive || packet.handle != 7U ||
        packet.payload_bytes != 3U)
        return 3;

    unsigned char iso[15] = {0};
    put_le16(iso, 18U);
    put_le16(iso + 2U, 1U);
    put_le16(iso + 4U, 9U);
    put_le16(iso + 6U, 0x0009U);
    put_le16(iso + 8U, 5U);
    memset(iso + 10U, 0xcc, 5U);
    if (!lsm_bluetooth_traffic_parse_monitor(iso, sizeof(iso), &packet) ||
        packet.receive || packet.handle != 9U ||
        packet.payload_bytes != 5U)
        return 4;

    put_le16(acl + 8U, 20U);
    if (lsm_bluetooth_traffic_parse_monitor(acl, sizeof(acl), &packet))
        return 5;

    LsmBluetoothDeviceInfo device = {0};
    LsmBluetoothTrafficState state = {0};
    const LsmBluetoothTrafficCounters first = {
        .rx_bytes = 1000U, .tx_bytes = 2000U, .link_count = 1U
    };
    lsm_bluetooth_traffic_apply_device(&device, &state, &first, 1.0);
    if (!device.traffic_available || device.rx_bytes_per_sec != 0.0 ||
        device.tx_bytes_per_sec != 0.0 || device.link_count != 1U)
        return 6;

    const LsmBluetoothTrafficCounters second = {
        .rx_bytes = 5096U, .tx_bytes = 10192U, .link_count = 2U
    };
    lsm_bluetooth_traffic_apply_device(&device, &state, &second, 2.0);
    if (!near(device.rx_bytes_per_sec, 2048.0) ||
        !near(device.tx_bytes_per_sec, 4096.0) ||
        device.rx_bytes_total != 5096U ||
        device.tx_bytes_total != 10192U ||
        device.link_count != 2U)
        return 7;

    const LsmBluetoothTrafficCounters reset = {
        .rx_bytes = 20U, .tx_bytes = 30U, .link_count = 1U
    };
    lsm_bluetooth_traffic_apply_device(&device, &state, &reset, 1.0);
    if (device.rx_bytes_per_sec != 0.0 ||
        device.tx_bytes_per_sec != 0.0)
        return 8;

    lsm_bluetooth_traffic_mark_device_unavailable(&device, &state);
    if (device.traffic_available || state.initialized ||
        device.link_count != 0U)
        return 9;

    puts("Per-device Bluetooth HCI parsing and accounting passed.");
    return 0;
}
