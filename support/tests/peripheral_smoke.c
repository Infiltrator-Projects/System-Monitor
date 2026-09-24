// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file peripheral_smoke.c
 * @brief Consolidated peripheral regression smoke suite.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <stddef.h>
#include <stdio.h>

int smoke_case_bluetooth_battery(void);
int smoke_case_bluetooth_traffic(void);
int smoke_case_linux_capability(void);
int smoke_case_logitech_hidpp(void);
int smoke_case_wifi_metadata(void);

/* ---- bluetooth_battery ---- */
#define main smoke_case_bluetooth_battery
#define objects_text lsm_test_bluetooth_battery_objects_text
/**
 * @file bluetooth_battery_smoke.c
 * @brief Synthetic BlueZ Battery1 ObjectManager regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
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
    LsmBluetoothDeviceRecord devices[4] = {0};
    const size_t count = lsm_bluetooth_battery_parse_objects(
        objects, records, 4U);
    const size_t adapter_count = lsm_bluetooth_adapter_parse_objects(
        objects, adapters, 2U);
    const size_t device_count = lsm_bluetooth_device_parse_objects(
        objects, devices, 4U);
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

    const bool devices_ok = device_count == 3U &&
        strcmp(devices[0].controller, "hci0") == 0 &&
        strcmp(devices[0].address, "10:20:30:40:50:60") == 0 &&
        strcmp(devices[0].alias, "Marshall Headphones") == 0 &&
        strcmp(devices[0].address_type, "public") == 0 &&
        strcmp(devices[0].icon, "audio-headset") == 0 &&
        strcmp(devices[0].modalias, "bluetooth:v000Ap0001d0001") == 0 &&
        devices[0].connected && devices[0].paired && devices[0].trusted &&
        devices[0].services_resolved &&
        strcmp(devices[1].address, "AA:BB:CC:DD:EE:FF") == 0 &&
        !devices[1].connected &&
        strcmp(devices[2].address, "01:02:03:04:05:06") == 0 &&
        !devices[2].connected;

    const bool ok = adapter_ok && devices_ok && count == 1U &&
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
        fputs("BlueZ adapter/Device1/Battery1 parsing failed\n", stderr);
        return 1;
    }
    puts("BlueZ adapter/Device1/Battery1 parsing passed.");
    return 0;
}

#undef main
#undef objects_text

/* ---- bluetooth_traffic ---- */
#define main smoke_case_bluetooth_traffic
#define near lsm_test_bluetooth_traffic_near
#define put_le16 lsm_test_bluetooth_traffic_put_le16
/**
 * @file bluetooth_traffic_smoke.c
 * @brief Per-device HCI monitor frame and rate-accounting regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
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

#undef main
#undef put_le16
#undef near

/* ---- linux_capability ---- */
#define main smoke_case_linux_capability
#define expected lsm_test_linux_capability_expected
/**
 * @file linux_capability_smoke.c
 * @brief Verify dependency-free CAP_NET_RAW file-capability encoding.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "linux_capability.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned char encoded[32];
    memset(encoded, 0xa5, sizeof(encoded));
    const size_t length =
        lsm_linux_net_raw_capability_xattr(encoded, sizeof(encoded));

    static const unsigned char expected[20] = {
        0x01, 0x00, 0x00, 0x02,
        0x00, 0x20, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    assert(length == sizeof(expected));
    assert(memcmp(encoded, expected, sizeof(expected)) == 0);
    assert(lsm_linux_net_raw_capability_xattr(NULL, sizeof(encoded)) == 0U);
    assert(lsm_linux_net_raw_capability_xattr(
               encoded, sizeof(expected) - 1U) == 0U);

    puts("Direct Linux CAP_NET_RAW xattr encoding passed.");
    return 0;
}

#undef main
#undef expected

/* ---- logitech_hidpp ---- */
#define main smoke_case_logitech_hidpp
#define join_path lsm_test_logitech_hidpp_join_path
#define make_directory lsm_test_logitech_hidpp_make_directory
#define test_parsers lsm_test_logitech_hidpp_test_parsers
#define test_request_framing lsm_test_logitech_hidpp_test_request_framing
#define test_hidraw_mapping lsm_test_logitech_hidpp_test_hidraw_mapping
#define test_worker_lifecycle lsm_test_logitech_hidpp_test_worker_lifecycle
/**
 * @file logitech_hidpp_smoke.c
 * @brief Logitech HID++ battery parser and sysfs mapping regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "logitech_hidpp.h"
#include "logitech_hidpp_protocol.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool join_path(char *destination, size_t size, const char *base,
                      const char *suffix)
{
    const size_t base_length = strlen(base);
    const size_t suffix_length = strlen(suffix);
    if (base_length + suffix_length + 1U > size) return false;
    memcpy(destination, base, base_length);
    memcpy(destination + base_length, suffix, suffix_length + 1U);
    return true;
}

static bool make_directory(const char *path)
{
    return mkdir(path, 0700) == 0 || errno == EEXIST;
}

static bool test_parsers(void)
{
    LsmHidppBatteryReading reading;
    const uint8_t discharging[] = {50U, 20U, 0U};
    if (!lsm_logitech_hidpp_parse_1000(
            discharging, sizeof(discharging), &reading) ||
        !reading.exact_percent || reading.percent != 50.0 ||
        !reading.next_percent_available || reading.next_percent != 20 ||
        strcmp(reading.status, "Discharging") != 0 ||
        strcmp(reading.level, "Normal") != 0)
        return false;

    const uint8_t charging[] = {0U, 0U, 1U};
    if (!lsm_logitech_hidpp_parse_1000(
            charging, sizeof(charging), &reading) ||
        reading.exact_percent || isfinite(reading.percent) ||
        strcmp(reading.status, "Charging") != 0)
        return false;

    const uint8_t full[] = {0U, 0U, 3U};
    if (!lsm_logitech_hidpp_parse_1000(full, sizeof(full), &reading) ||
        reading.exact_percent || isfinite(reading.percent) ||
        strcmp(reading.status, "Full") != 0 ||
        strcmp(reading.level, "Full") != 0)
        return false;

    const uint8_t unified_exact[] = {85U, 0x04U, 0U, 0U};
    if (!lsm_logitech_hidpp_parse_1004(
            unified_exact, sizeof(unified_exact), &reading) ||
        !reading.exact_percent || reading.percent != 85.0 ||
        strcmp(reading.level, "High") != 0 ||
        strcmp(reading.status, "Charging") != 0)
        return false;

    const uint8_t unified_coarse[] = {0U, 0x02U, 2U, 0U};
    return lsm_logitech_hidpp_parse_1004(
               unified_coarse, sizeof(unified_coarse), &reading) &&
           !reading.exact_percent && strcmp(reading.level, "Low") == 0 &&
           strcmp(reading.status, "Discharging") == 0;
}


static bool test_request_framing(void)
{
    uint8_t report[20] = {0};
    const uint8_t parameters[] = {0x10U, 0x00U};
    const size_t size = lsm_logitech_hidpp_format_request(
        0x06U, 0x00U, parameters, sizeof(parameters), report, sizeof(report));
    return size == sizeof(report) && report[0] == 0x11U &&
           report[1] == 0xffU && report[2] == 0x06U &&
           (report[3] & 0xf0U) == 0x00U && report[4] == 0x10U &&
           report[5] == 0x00U;
}

static bool test_hidraw_mapping(void)
{
    char root[] = "/tmp/lsm-hidpp-XXXXXX";
    if (!mkdtemp(root)) return false;

    char devices[512], uhid[512], supplies[512], supply[512];
    char class_root[512], hidraw[512], device_link[512], dev_root[512];
    bool ok = join_path(devices, sizeof(devices), root, "/devices") &&
        join_path(uhid, sizeof(uhid), devices, "/0005:046D:B012.001E") &&
        join_path(supplies, sizeof(supplies), uhid, "/power_supply") &&
        join_path(supply, sizeof(supply), supplies, "/hidpp_battery_26") &&
        join_path(class_root, sizeof(class_root), root, "/class") &&
        join_path(hidraw, sizeof(hidraw), class_root, "/hidraw3") &&
        join_path(device_link, sizeof(device_link), hidraw, "/device") &&
        join_path(dev_root, sizeof(dev_root), root, "/dev") &&
        make_directory(devices) && make_directory(uhid) &&
        make_directory(supplies) && make_directory(supply) &&
        make_directory(class_root) && make_directory(hidraw) &&
        make_directory(dev_root) && symlink(uhid, device_link) == 0 &&
        setenv("LSM_HIDRAW_SYS_ROOT", class_root, 1) == 0 &&
        setenv("LSM_HIDRAW_DEV_ROOT", dev_root, 1) == 0;

    char result[512] = "";
    if (ok)
        ok = lsm_logitech_hidpp_find_device(
                 supply, result, sizeof(result)) &&
             strstr(result, "/dev/hidraw3") != NULL;

    unsetenv("LSM_HIDRAW_SYS_ROOT");
    unsetenv("LSM_HIDRAW_DEV_ROOT");
    unlink(device_link);
    rmdir(hidraw);
    rmdir(class_root);
    rmdir(dev_root);
    rmdir(supply);
    rmdir(supplies);
    rmdir(uhid);
    rmdir(devices);
    rmdir(root);
    return ok;
}

static bool test_worker_lifecycle(void)
{
    if (!lsm_logitech_hidpp_start()) return false;
    lsm_logitech_hidpp_set_devices(NULL, 0U);
    lsm_logitech_hidpp_stop();
    return true;
}

int main(void)
{
    if (!test_parsers() || !test_request_framing() ||
        !test_hidraw_mapping() || !test_worker_lifecycle()) {
        fputs("Logitech HID++ battery regression failed\n", stderr);
        return 1;
    }
    puts("Logitech HID++ battery parsing and mapping passed.");
    return 0;
}

#undef main
#undef test_worker_lifecycle
#undef test_hidraw_mapping
#undef test_request_framing
#undef test_parsers
#undef make_directory
#undef join_path

/* ---- wifi_metadata ---- */
#define main smoke_case_wifi_metadata
/**
 * @file wifi_metadata_smoke.c
 * @brief Verify that Wi-Fi enrichment never blocks the caller.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "wifi_metadata.h"
#include "common.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmWifiMetadata *metadata = lsm_wifi_metadata_create();
    if (!metadata) return 1;

    LsmNetInfo network = {0};
    strcpy(network.name, "lsm-nonexistent-wireless-test");
    network.wireless = true;
    const double started = lsm_monotonic_seconds();
    lsm_wifi_metadata_refresh(metadata, &network);
    const double elapsed = lsm_monotonic_seconds() - started;
    lsm_wifi_metadata_destroy(metadata);

    if (elapsed > 0.100) {
        fprintf(stderr, "Wi-Fi refresh blocked for %.3f seconds\n", elapsed);
        return 2;
    }
    printf("Wi-Fi refresh returned in %.6f seconds\n", elapsed);
    return 0;
}

#undef main

typedef int (*LsmMergedSmokeCaseFunction)(void);
typedef struct { const char *name; LsmMergedSmokeCaseFunction function; } LsmMergedSmokeCase;

int main(void)
{
    static const LsmMergedSmokeCase cases[] = {
        {"bluetooth_battery", smoke_case_bluetooth_battery},
        {"bluetooth_traffic", smoke_case_bluetooth_traffic},
        {"linux_capability", smoke_case_linux_capability},
        {"logitech_hidpp", smoke_case_logitech_hidpp},
        {"wifi_metadata", smoke_case_wifi_metadata},
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0U; i < count; ++i) {
        const int status = cases[i].function();
        if (status != 0) {
            fprintf(stderr, "peripheral smoke suite: %s failed with status %d\n", cases[i].name, status);
            return status;
        }
    }
    printf("peripheral smoke suite passed (%zu cases).\n", count);
    return 0;
}
