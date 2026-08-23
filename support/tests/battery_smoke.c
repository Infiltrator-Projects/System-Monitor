// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file battery_smoke.c
 * @brief Synthetic system and peripheral power-supply regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "common.h"
#include "monitor_linux_internal.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool make_directory(const char *path)
{
    return mkdir(path, 0700) == 0 || errno == EEXIST;
}

static bool write_value(const char *directory, const char *name,
                        const char *value)
{
    char path[LSM_PATH_LEN];
    if (snprintf(path, sizeof(path), "%s/%s", directory, name) >= (int)sizeof(path))
        return false;
    FILE *file = fopen(path, "w");
    if (!file) return false;
    const bool written = fputs(value, file) >= 0;
    return fclose(file) == 0 && written;
}

static void remove_supply(const char *directory, const char *const *files,
                          size_t file_count)
{
    char path[LSM_PATH_LEN];
    for (size_t index = 0; index < file_count; index++) {
        snprintf(path, sizeof(path), "%s/%s", directory, files[index]);
        unlink(path);
    }
    rmdir(directory);
}

static const LsmBatteryInfo *find_battery(const LsmMonitor *monitor,
                                          const char *name)
{
    for (size_t index = 0; index < monitor->battery_count; index++)
        if (strcmp(monitor->batteries[index].name, name) == 0)
            return &monitor->batteries[index];
    return NULL;
}

int main(void)
{
    char root[] = "/tmp/lsm-power-supply-XXXXXX";
    if (!mkdtemp(root)) return 1;

    char ac[LSM_PATH_LEN], system[LSM_PATH_LEN], peripheral[LSM_PATH_LEN];
    snprintf(ac, sizeof(ac), "%s/AC", root);
    snprintf(system, sizeof(system), "%s/BAT0", root);
    snprintf(peripheral, sizeof(peripheral), "%s/hidpp_battery_0", root);
    if (!make_directory(ac) || !make_directory(system) || !make_directory(peripheral))
        return 1;

    const bool files_ok =
        write_value(ac, "type", "Mains\n") &&
        write_value(ac, "online", "1\n") &&
        write_value(system, "type", "Battery\n") &&
        write_value(system, "scope", "System\n") &&
        write_value(system, "model_name", "Internal Battery\n") &&
        write_value(system, "status", "Charging\n") &&
        write_value(system, "capacity", "83\n") &&
        write_value(peripheral, "type", "Battery\n") &&
        write_value(peripheral, "scope", "Device\n") &&
        write_value(peripheral, "model_name", "Wireless Mouse MX Master\n") &&
        write_value(peripheral, "manufacturer", "Logitech\n") &&
        write_value(peripheral, "status", "Discharging\n") &&
        write_value(peripheral, "capacity_level", "Low\n");
    if (!files_ok || setenv("LSM_POWER_SUPPLY_ROOT", root, 1) != 0) return 1;

    LsmMonitor monitor = {0};
    LsmLinuxMonitorBackendState backend_state = {0};
    monitor.backend_state = &backend_state;
    lsm_hardware_initialise(&monitor);
    const LsmBatteryInfo *bat0 = find_battery(&monitor, "BAT0");
    const LsmBatteryInfo *mouse = find_battery(&monitor, "hidpp_battery_0");

    bool hidpp_priority_ok = false;
    bool hidpp_unavailable_preserves_generic = false;
    if (mouse) {
        LsmBatteryInfo merged = *mouse;
        merged.capacity_percent = 5.0;
        lsm_copy_string(merged.status, sizeof(merged.status), "Discharging");

        LsmHidppBatteryReading direct = {0};
        direct.percent = 50.0;
        direct.exact_percent = true;
        lsm_copy_string(direct.status, sizeof(direct.status), "Discharging");
        lsm_copy_string(direct.level, sizeof(direct.level), "Normal");
        lsm_copy_string(direct.source, sizeof(direct.source),
                        "Logitech HID++ 0x1000");
        hidpp_priority_ok = lsm_battery_apply_hidpp_reading(
                                &merged, &direct) &&
                            merged.capacity_percent == 50.0 &&
                            strcmp(merged.capacity_level, "Normal") == 0 &&
                            strcmp(merged.battery_source,
                                   "Logitech HID++ 0x1000") == 0;

        merged.capacity_percent = 5.0;
        direct.percent = NAN;
        direct.exact_percent = false;
        hidpp_unavailable_preserves_generic =
            lsm_battery_apply_hidpp_reading(&merged, &direct) &&
            merged.capacity_percent == 5.0;
    }

    const bool ok = monitor.battery_count == 2 && bat0 && mouse &&
        !bat0->is_peripheral && bat0->on_ac_power &&
        isfinite(bat0->capacity_percent) && bat0->capacity_percent == 83.0 &&
        mouse->is_peripheral && !mouse->on_ac_power &&
        !isfinite(mouse->capacity_percent) &&
        strcmp(mouse->capacity_level, "Low") == 0 &&
        strcmp(mouse->model, "Wireless Mouse MX Master") == 0 &&
        strcmp(mouse->manufacturer, "Logitech") == 0 &&
        hidpp_priority_ok && hidpp_unavailable_preserves_generic;

    lsm_hardware_shutdown(&monitor);
    monitor.backend_state = NULL;
    unsetenv("LSM_POWER_SUPPLY_ROOT");

    static const char *const ac_files[] = {"type", "online"};
    static const char *const system_files[] = {
        "type", "scope", "model_name", "status", "capacity"
    };
    static const char *const peripheral_files[] = {
        "type", "scope", "model_name", "manufacturer", "status",
        "capacity_level"
    };
    remove_supply(ac, ac_files, sizeof(ac_files) / sizeof(ac_files[0]));
    remove_supply(system, system_files,
                  sizeof(system_files) / sizeof(system_files[0]));
    remove_supply(peripheral, peripheral_files,
                  sizeof(peripheral_files) / sizeof(peripheral_files[0]));
    rmdir(root);

    if (!ok) {
        fputs("battery classification, coarse-level parsing or HID++ priority failed\n", stderr);
        return 1;
    }
    puts("System and peripheral battery parsing passed.");
    return 0;
}
