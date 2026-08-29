// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bluetooth_battery_smoke.c
 * @brief Synthetic BlueZ Battery1 ObjectManager regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "bluetooth_battery.h"

#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char objects_text[] =
        "{"
        "objectpath '/org/bluez/hci0': {"
        "'org.bluez.Adapter1': {"
        "'Address': <'11:22:33:44:55:66'>,"
        "'Name': <'Test Bluetooth'>,"
        "'Alias': <'Test Bluetooth'>,"
        "'Powered': <true>,'Discoverable': <false>,"
        "'Pairable': <true>,'Discovering': <false>}},"
        "objectpath '/org/bluez/hci0/dev_10_20_30_40_50_60': {"
        "'org.bluez.Device1': {"
        "'Address': <'10:20:30:40:50:60'>,"
        "'Adapter': <objectpath '/org/bluez/hci0'>,"
        "'AddressType': <'public'>,"
        "'Alias': <'Marshall Headphones'>,"
        "'Icon': <'audio-headset'>,"
        "'Modalias': <'bluetooth:v000Ap0001d0001'>,"
        "'Paired': <true>,'Trusted': <true>,"
        "'ServicesResolved': <true>,'Connected': <true>},"
        "'org.bluez.Battery1': {"
        "'Percentage': <byte 73>,"
        "'Source': <'GATT Battery Service'>}},"
        "objectpath '/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF': {"
        "'org.bluez.Device1': {"
        "'Address': <'AA:BB:CC:DD:EE:FF'>,"
        "'Adapter': <objectpath '/org/bluez/hci0'>,"
        "'Alias': <'Disconnected Headset'>,"
        "'Connected': <false>},"
        "'org.bluez.Battery1': {'Percentage': <byte 44>}},"
        "objectpath '/org/bluez/hci0/dev_01_02_03_04_05_06': {"
        "'org.bluez.Device1': {"
        "'Address': <'01:02:03:04:05:06'>,"
        "'Adapter': <objectpath '/org/bluez/hci0'>,"
        "'Alias': <'Connection State Missing'>},"
        "'org.bluez.Battery1': {'Percentage': <byte 88>}}"
        "}";

    GError *error = NULL;
    GVariant *objects = g_variant_parse(
        G_VARIANT_TYPE("a{oa{sa{sv}}}"), objects_text, NULL, NULL, &error);
    if (!objects) {
        if (error) fprintf(stderr, "Variant parse failed: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    LsmBluetoothBatteryRecord records[4] = {0};
    LsmBluetoothAdapterRecord adapters[2] = {0};
    const size_t count = lsm_bluetooth_battery_parse_objects(
        objects, records, 4U);
    const size_t adapter_count = lsm_bluetooth_adapter_parse_objects(
        objects, adapters, 2U);
    g_variant_unref(objects);

    const bool adapter_ok = adapter_count == 1U &&
        strcmp(adapters[0].address, "11:22:33:44:55:66") == 0 &&
        strcmp(adapters[0].name, "Test Bluetooth") == 0 &&
        adapters[0].powered && adapters[0].pairable &&
        !adapters[0].discoverable && !adapters[0].discovering &&
        adapters[0].device_count == 3U &&
        adapters[0].connected_count == 1U &&
        adapters[0].paired_count == 1U &&
        adapters[0].trusted_count == 1U &&
        strcmp(adapters[0].connected_names, "Marshall Headphones") == 0;

    const bool ok = adapter_ok && count == 1U &&
        strcmp(records[0].address, "10:20:30:40:50:60") == 0 &&
        strcmp(records[0].name, "Marshall Headphones") == 0 &&
        strcmp(records[0].source, "GATT Battery Service") == 0 &&
        strcmp(records[0].address_type, "public") == 0 &&
        strcmp(records[0].icon, "audio-headset") == 0 &&
        strcmp(records[0].modalias, "bluetooth:v000Ap0001d0001") == 0 &&
        records[0].percentage == 73.0 && records[0].connected &&
        records[0].paired && records[0].trusted &&
        records[0].services_resolved;
    if (!ok) {
        fputs("BlueZ adapter/Battery1 parsing failed\n", stderr);
        return 1;
    }
    puts("BlueZ adapter/Battery1 parsing passed.");
    return 0;
}
