// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system_snapshot_smoke.c
 * @brief Native diagnostic snapshot content and atomic-write regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "app_internal.h"
#include "system_snapshot.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
    LsmApp app;
    memset(&app, 0, sizeof(app));
    strcpy(app.monitor.cpu.model, "Synthetic CPU");
    app.monitor.cpu.logical_cores = 8U;
    app.monitor.cpu.physical_cores = 4U;
    app.monitor.cpu.socket_count = 1U;
    app.monitor.cpu.usage_percent = 25.0;
    app.monitor.cpu.user_percent = 15.0;
    app.monitor.cpu.kernel_percent = 10.0;
    app.monitor.memory.total_bytes = 16ULL * 1024ULL * 1024ULL * 1024ULL;
    app.monitor.memory.used_bytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
    app.monitor.memory.usage_percent = 50.0;
    app.monitor.memory.committed_bytes = 10ULL * 1024ULL * 1024ULL * 1024ULL;
    app.monitor.memory.commit_limit_bytes = 24ULL * 1024ULL * 1024ULL * 1024ULL;
    app.monitor.memory.cached_bytes = 2ULL * 1024ULL * 1024ULL * 1024ULL;
    app.monitor.memory.kernel_reclaimable_bytes = 256ULL * 1024ULL * 1024ULL;
    app.monitor.memory.kernel_nonreclaimable_bytes = 128ULL * 1024ULL * 1024ULL;
    app.monitor.memory.page_tables_bytes = 64ULL * 1024ULL * 1024ULL;
    app.monitor.memory.module_details_available = true;
    app.monitor.memory.module_count = 1U;
    strcpy(app.monitor.memory.modules[0].locator, "DIMM A1");
    strcpy(app.monitor.memory.modules[0].memory_type, "DDR5");
    strcpy(app.monitor.memory.modules[0].manufacturer, "Acme");
    strcpy(app.monitor.memory.modules[0].part_number, "TEST-16G");
    strcpy(app.monitor.memory.modules[0].serial_number, "1234");
    app.monitor.memory.modules[0].size_bytes =
        16ULL * 1024ULL * 1024ULL * 1024ULL;
    app.monitor.memory.modules[0].speed_mhz = 5600U;

    app.monitor.bluetooth_count = 1U;
    strcpy(app.monitor.bluetooth[0].name, "hci0");
    strcpy(app.monitor.bluetooth[0].alias, "Test Adapter");
    strcpy(app.monitor.bluetooth[0].address, "00:11:22:33:44:55");
    app.monitor.bluetooth[0].powered = true;
    app.monitor.bluetooth[0].device_count = 1U;
    app.monitor.bluetooth[0].connected_count = 1U;
    app.monitor.bluetooth[0].traffic_available = true;
    app.monitor.bluetooth[0].rx_bytes_total = 4096U;
    app.monitor.bluetooth[0].tx_bytes_total = 2048U;
    app.monitor.bluetooth[0].rx_bytes_per_sec = 1000.0;
    app.monitor.bluetooth[0].tx_bytes_per_sec = 500.0;

    app.monitor.bluetooth_device_count = 1U;
    strcpy(app.monitor.bluetooth_devices[0].controller, "hci0");
    strcpy(app.monitor.bluetooth_devices[0].address, "AA:BB:CC:DD:EE:FF");
    strcpy(app.monitor.bluetooth_devices[0].alias, "Test Headset");
    app.monitor.bluetooth_devices[0].connected = true;
    app.monitor.bluetooth_devices[0].paired = true;
    app.monitor.bluetooth_devices[0].trusted = true;
    app.monitor.bluetooth_devices[0].services_resolved = true;
    app.monitor.bluetooth_devices[0].link_count = 1U;
    app.monitor.bluetooth_devices[0].traffic_available = true;
    app.monitor.bluetooth_devices[0].rx_bytes_total = 8192U;
    app.monitor.bluetooth_devices[0].tx_bytes_total = 4096U;
    app.monitor.bluetooth_devices[0].rx_bytes_per_sec = 2000.0;
    app.monitor.bluetooth_devices[0].tx_bytes_per_sec = 1000.0;

    app.monitor.battery_count = 1U;
    strcpy(app.monitor.batteries[0].name, "BAT0");
    strcpy(app.monitor.batteries[0].model, "Synthetic Battery");
    strcpy(app.monitor.batteries[0].status, "Discharging");
    strcpy(app.monitor.batteries[0].battery_source, "power_supply");
    app.monitor.batteries[0].capacity_percent = 88.0;
    app.monitor.batteries[0].power_watts = 12.5;
    app.monitor.batteries[0].temperature_c = 31.5;
    app.monitor.batteries[0].cycle_count = 42U;
    app.monitor.batteries[0].present = true;

    LsmProcessInfo process;
    memset(&process, 0, sizeof(process));
    process.pid = 42;
    strcpy(process.name, "snapshot-test");
    strcpy(process.user, "tester");
    strcpy(process.gpu_engine, "render");
    strcpy(process.command, "snapshot-test --safe");
    process.rss_bytes = 64ULL * 1024ULL * 1024ULL;
    process.gpu_available = true;
    process.gpu_percent = 12.5;
    app.process.process_snapshot = &process;
    app.process.process_snapshot_count = 1U;

    char path[] = "/tmp/lsm-snapshot-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    close(descriptor);
    unlink(path);
    char error[256];
    assert(lsm_system_snapshot_write(&app, path, error, sizeof(error)));
    struct stat status;
    assert(stat(path, &status) == 0);
    assert((status.st_mode & 0777) == 0600);

    FILE *file = fopen(path, "r");
    assert(file);
    assert(fseek(file, 0L, SEEK_END) == 0);
    const long length = ftell(file);
    assert(length > 0L);
    rewind(file);
    char *contents = malloc((size_t)length + 1U);
    assert(contents);
    assert(fread(contents, 1U, (size_t)length, file) == (size_t)length);
    contents[length] = '\0';
    fclose(file);
    assert(strstr(contents, "Synthetic CPU"));
    assert(strstr(contents, "DIMM A1"));
    assert(strstr(contents, "snapshot-test"));
    assert(strstr(contents, "GPU engine"));
    assert(strstr(contents, "Commit:"));
    assert(strstr(contents, "Test Adapter"));
    assert(strstr(contents, "Test Headset"));
    assert(strstr(contents, "AA:BB:CC:DD:EE:FF"));
    assert(strstr(contents, "Synthetic Battery"));
    assert(strstr(contents, "Batteries and peripheral power"));
    assert(!strstr(contents, "K" "iB"));
    assert(!strstr(contents, "M" "iB"));
    free(contents);
    unlink(path);
    puts("Native diagnostic snapshot content and atomic write passed.");
    return 0;
}
