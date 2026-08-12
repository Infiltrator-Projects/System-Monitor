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
        "objectpath '/org/bluez/hci0/dev_10_20_30_40_50_60': {"
        "'org.bluez.Device1': {"
        "'Address': <'10:20:30:40:50:60'>,"
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
        "'Alias': <'Disconnected Headset'>,"
        "'Connected': <false>},"
        "'org.bluez.Battery1': {'Percentage': <byte 44>}},"
        "objectpath '/org/bluez/hci0/dev_01_02_03_04_05_06': {"
        "'org.bluez.Device1': {"
        "'Address': <'01:02:03:04:05:06'>,"
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
    const size_t count = lsm_bluetooth_battery_parse_objects(
        objects, records, 4U);
    g_variant_unref(objects);

    const bool ok = count == 1U &&
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
        fputs("BlueZ Battery1 parsing failed\n", stderr);
        return 1;
    }
    puts("BlueZ Battery1 parsing passed.");
    return 0;
}
