// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_battery.c
 * @brief System and peripheral battery collection and source integration.
 *
 * The module combines power-supply sysfs, cached BlueZ Battery1 records and
 * direct Logitech HID++ readings. Slow D-Bus and wireless transactions remain
 * in their own background backends; this file only merges bounded snapshots.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_linux_internal.h"

#include "bluetooth_battery.h"
#include "common.h"
#include "logitech_hidpp.h"

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* The power-supply class is the kernel driver ABI for system batteries and
 * many peripherals; fixture roots keep parsing independently testable. */
static const char *power_supply_root(void)
{
    const char *root = getenv("LSM_POWER_SUPPLY_ROOT");
    return root && root[0] ? root : "/sys/class/power_supply";
}

static bool power_supply_online(void)
{
    const char *root = power_supply_root();
    DIR *directory = opendir(root);
    if (!directory) return false;
    bool online = false;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
        char path[LSM_PATH_LEN], type[64] = "";
        snprintf(path, sizeof(path), "%s/%s/type", root, entry->d_name);
        if (!lsm_read_text_file(path, type, sizeof(type)) || strcmp(type, "Battery") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s/online", root, entry->d_name);
        if (lsm_read_u64_or_zero(path) != 0) {
            online = true;
            break;
        }
    }
    closedir(directory);
    return online;
}

static double battery_energy_wh(const char *base, const char *energy_name,
                                const char *charge_name, double voltage_volts)
{
    char path[LSM_PATH_LEN];
    char suffix[64];
    snprintf(suffix, sizeof(suffix), "/%s", energy_name);
    uint64_t micro = 0;
    if (lsm_join_path(path, sizeof(path), base, suffix) &&
        lsm_read_u64_file(path, &micro)) return (double)micro / 1000000.0;

    snprintf(suffix, sizeof(suffix), "/%s", charge_name);
    if (lsm_join_path(path, sizeof(path), base, suffix) &&
        lsm_read_u64_file(path, &micro) && voltage_volts > 0.0)
        return ((double)micro / 1000000.0) * voltage_volts;
    return NAN;
}

static uint64_t battery_seconds_from_hours(double hours)
{
    if (!isfinite(hours) || hours <= 0.0) return 0U;
    const long double seconds = (long double)hours * 3600.0L;
    if (!isfinite(seconds) || seconds >= (long double)UINT64_MAX)
        return UINT64_MAX;
    return (uint64_t)seconds;
}

static void initialise_battery_measurements(LsmBatteryInfo *battery)
{
    battery->capacity_percent = NAN;
    battery->supplemental_capacity_percent = NAN;
    battery->energy_now_wh = NAN;
    battery->energy_full_wh = NAN;
    battery->energy_design_wh = NAN;
    battery->power_watts = NAN;
    battery->voltage_volts = NAN;
    battery->current_amps = NAN;
    battery->temperature_c = NAN;
}

static bool peripheral_supply_has_charge_telemetry(const char *base)
{
    if (!base || !base[0]) return false;

    char path[LSM_PATH_LEN];
    if (lsm_join_path(path, sizeof(path), base, "/capacity") &&
        isfinite(lsm_read_double_or_nan(path)))
        return true;

    char level[32] = "";
    if (lsm_join_path(path, sizeof(path), base, "/capacity_level") &&
        lsm_read_text_file(path, level, sizeof(level)) && level[0] &&
        strcasecmp(level, "Unknown") != 0)
        return true;

    uint64_t raw = 0U;
    if (lsm_join_path(path, sizeof(path), base, "/energy_now") &&
        lsm_read_u64_file(path, &raw))
        return true;
    if (lsm_join_path(path, sizeof(path), base, "/charge_now") &&
        lsm_read_u64_file(path, &raw))
        return true;
    return false;
}

static bool same_bluetooth_address(const char *left, const char *right)
{
    return left && right && left[0] && right[0] && strcasecmp(left, right) == 0;
}

static LsmBatteryInfo *find_battery_by_serial(LsmMonitor *monitor,
                                               const char *serial)
{
    if (!serial || !serial[0]) return NULL;
    for (size_t index = 0; index < monitor->battery_count; index++)
        if (same_bluetooth_address(monitor->batteries[index].serial, serial))
            return &monitor->batteries[index];
    return NULL;
}

static LsmLinuxBatteryState *find_battery_state(LsmMonitor *monitor,
                                                const char *name)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state || !name) return NULL;
    for (size_t index = 0U; index < state->battery_count; index++)
        if (strcmp(state->batteries[index].name, name) == 0)
            return &state->batteries[index];
    return NULL;
}

static LsmLinuxBatteryState *register_battery_state(
    LsmMonitor *monitor, const char *name)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state || !name || !name[0]) return NULL;
    LsmLinuxBatteryState *existing = find_battery_state(monitor, name);
    if (existing) return existing;
    if (state->battery_count >= LSM_MAX_BATTERIES) return NULL;
    LsmLinuxBatteryState *entry = &state->batteries[state->battery_count++];
    memset(entry, 0, sizeof(*entry));
    lsm_copy_string(entry->name, sizeof(entry->name), name);
    return entry;
}

static void bluetooth_battery_name(const LsmBluetoothBatteryRecord *record,
                                   char *name, size_t size)
{
    snprintf(name, size, "bluez_%s", record->address[0]
             ? record->address : record->object_path);
    for (char *cursor = name; *cursor; cursor++)
        if (*cursor == ':' || *cursor == '/' || *cursor == ' ')
            *cursor = '_';
}


static void bluetooth_device_type(const char *icon, char *destination,
                                  size_t destination_size)
{
    if (!destination || destination_size == 0U) return;
    destination[0] = '\0';
    if (!icon || !icon[0]) return;

    const char *source = icon;
    if (strncmp(source, "audio-", 6U) == 0) source += 6U;
    else if (strncmp(source, "input-", 6U) == 0) source += 6U;

    size_t used = 0U;
    bool first = true;
    for (; *source && used + 1U < destination_size; source++) {
        char value = *source == '-' || *source == '_' ? ' ' : *source;
        if (first && isalpha((unsigned char)value)) {
            value = (char)toupper((unsigned char)value);
            first = false;
        }
        destination[used++] = value;
    }
    destination[used] = '\0';
}

static void apply_bluetooth_details(
    LsmBatteryInfo *battery, const LsmBluetoothBatteryRecord *record)
{
    if (!battery || !record) return;
    lsm_copy_string(battery->connection, sizeof(battery->connection),
                    record->address_type[0] &&
                    strcasecmp(record->address_type, "random") == 0
                        ? "Bluetooth LE" : "Bluetooth");
    lsm_copy_string(battery->battery_source,
                    sizeof(battery->battery_source),
                    record->source[0] ? record->source : "BlueZ Battery1");
    bluetooth_device_type(record->icon, battery->device_type,
                          sizeof(battery->device_type));
    lsm_copy_string(battery->modalias, sizeof(battery->modalias),
                    record->modalias);
    battery->paired = record->paired;
    battery->trusted = record->trusted;
    battery->services_resolved = record->services_resolved;
    battery->bluetooth_details_available = true;
}

/* BlueZ records enrich or append Bluetooth devices without blocking GTK. */
static void merge_bluez_batteries(LsmMonitor *monitor)
{
    LsmBluetoothBatteryRecord records[LSM_BLUETOOTH_BATTERY_MAX] = {0};
    const size_t count = lsm_bluetooth_battery_snapshot(
        records, LSM_BLUETOOTH_BATTERY_MAX);
    for (size_t index = 0; index < count; index++) {
        const LsmBluetoothBatteryRecord *record = &records[index];
        LsmBatteryInfo *battery = find_battery_by_serial(monitor,
                                                         record->address);
        if (battery) {
            battery->supplemental_capacity_percent = record->percentage;
            battery->has_supplemental_capacity = true;
            if (!battery->technology[0])
                lsm_copy_string(battery->technology,
                                sizeof(battery->technology), "Bluetooth");
            apply_bluetooth_details(battery, record);
            continue;
        }
        if (monitor->battery_count >= LSM_MAX_BATTERIES) break;

        battery = &monitor->batteries[monitor->battery_count++];
        memset(battery, 0, sizeof(*battery));
        initialise_battery_measurements(battery);
        bluetooth_battery_name(record, battery->name, sizeof(battery->name));
        lsm_copy_string(battery->model, sizeof(battery->model), record->name);
        lsm_copy_string(battery->serial, sizeof(battery->serial),
                        record->address);
        lsm_copy_string(battery->technology, sizeof(battery->technology),
                        "Bluetooth");
        apply_bluetooth_details(battery, record);
        lsm_copy_string(battery->scope, sizeof(battery->scope), "Device");
        lsm_copy_string(battery->status, sizeof(battery->status), "Connected");
        battery->capacity_percent = record->percentage;
        battery->supplemental_capacity_percent = record->percentage;
        battery->has_supplemental_capacity = true;
        battery->is_peripheral = true;
        LsmLinuxBatteryState *state = register_battery_state(
            monitor, battery->name);
        if (state) state->bluez_record = true;
        battery->present = record->connected;
    }
}

void lsm_bluetooth_enumerate(LsmMonitor *monitor)
{
    if (!monitor) return;
    LsmBluetoothAdapterRecord records[LSM_BLUETOOTH_ADAPTER_MAX] = {0};
    const size_t count = lsm_bluetooth_adapter_snapshot(
        records, LSM_BLUETOOTH_ADAPTER_MAX);

    monitor->bluetooth_count = count < LSM_MAX_BLUETOOTH
        ? count : LSM_MAX_BLUETOOTH;
    for (size_t index = 0U; index < monitor->bluetooth_count; index++) {
        const LsmBluetoothAdapterRecord *source = &records[index];
        LsmBluetoothInfo *destination = &monitor->bluetooth[index];
        memset(destination, 0, sizeof(*destination));

        const char *name = strrchr(source->object_path, '/');
        name = name && name[1] ? name + 1 : source->object_path;
        lsm_copy_string(destination->name, sizeof(destination->name),
                        name && name[0] ? name : "Bluetooth");
        lsm_copy_string(destination->address, sizeof(destination->address),
                        source->address);
        lsm_copy_string(destination->adapter_name,
                        sizeof(destination->adapter_name), source->name);
        lsm_copy_string(destination->alias, sizeof(destination->alias),
                        source->alias);
        lsm_copy_string(destination->connected_devices,
                        sizeof(destination->connected_devices),
                        source->connected_names);
        destination->device_count = source->device_count;
        destination->connected_count = source->connected_count;
        destination->paired_count = source->paired_count;
        destination->trusted_count = source->trusted_count;
        destination->powered = source->powered;
        destination->discoverable = source->discoverable;
        destination->pairable = source->pairable;
        destination->discovering = source->discovering;
    }
    if (monitor->bluetooth_count < LSM_MAX_BLUETOOTH) {
        memset(&monitor->bluetooth[monitor->bluetooth_count], 0,
               (LSM_MAX_BLUETOOTH - monitor->bluetooth_count) *
               sizeof(monitor->bluetooth[0]));
    }
}

static void track_hidpp_batteries(const LsmMonitor *monitor)
{
    const char *devices[LSM_LOGITECH_HIDPP_MAX_DEVICES] = {0};
    size_t count = 0U;
    const LsmLinuxMonitorBackendState *state =
        monitor_backend_state_const(monitor);
    if (state) {
        for (size_t index = 0U;
             index < state->battery_count &&
             count < LSM_LOGITECH_HIDPP_MAX_DEVICES;
             index++) {
            const char *path = state->batteries[index].hidraw_path;
            if (path[0]) devices[count++] = path;
        }
    }
    lsm_logitech_hidpp_set_devices(devices, count);
}

static bool hidpp_status_matches(const char *sysfs_status,
                                 const char *hidpp_status)
{
    if (!hidpp_status || !hidpp_status[0]) return false;
    if (!sysfs_status || !sysfs_status[0] ||
        strcasecmp(sysfs_status, "Unknown") == 0)
        return true;
    return strcasecmp(sysfs_status, hidpp_status) == 0;
}

/* HID++ values override coarse kernel capacity only when identity and status
 * checks show that both records describe the same physical device. */
bool lsm_battery_apply_hidpp_reading(
    LsmBatteryInfo *battery, const LsmHidppBatteryReading *reading)
{
    if (!battery || !reading ||
        !hidpp_status_matches(battery->status, reading->status))
        return false;

    if ((!battery->status[0] ||
         strcasecmp(battery->status, "Unknown") == 0) &&
        reading->status[0])
        lsm_copy_string(battery->status, sizeof(battery->status),
                        reading->status);
    if (reading->level[0])
        lsm_copy_string(battery->capacity_level,
                        sizeof(battery->capacity_level), reading->level);

    /* A status-matched HID++ result comes directly from the Logitech
     * battery feature and therefore supersedes generic BlueZ/sysfs values. */
    if (reading->exact_percent && isfinite(reading->percent) &&
        reading->percent >= 0.0 && reading->percent <= 100.0)
        battery->capacity_percent = reading->percent;
    if (reading->source[0])
        lsm_copy_string(battery->battery_source,
                        sizeof(battery->battery_source), reading->source);
    if (!battery->connection[0])
        lsm_copy_string(battery->connection, sizeof(battery->connection),
                        "Logitech HID++");
    if (!battery->device_type[0])
        lsm_copy_string(battery->device_type,
                        sizeof(battery->device_type), "Mouse");
    return true;
}

static void apply_hidpp_snapshot(LsmMonitor *monitor,
                                 LsmBatteryInfo *battery)
{
    LsmLinuxBatteryState *state = find_battery_state(monitor, battery->name);
    if (!state || !state->hidraw_path[0]) return;

    LsmHidppBatteryReading reading;
    if (!lsm_logitech_hidpp_snapshot(state->hidraw_path, &reading)) return;
    (void)lsm_battery_apply_hidpp_reading(battery, &reading);
}

/* Inventory and sampling are separate so hotplug work stays off fast graphs. */
void lsm_battery_enumerate(LsmMonitor *monitor)
{
    monitor->battery_count = 0;
    LsmLinuxMonitorBackendState *backend = monitor_backend_state(monitor);
    if (backend) {
        memset(backend->batteries, 0, sizeof(backend->batteries));
        backend->battery_count = 0U;
    }
    const char *root = power_supply_root();
    DIR *directory = opendir(root);
    if (directory) {
        struct dirent *entry;
        while ((entry = readdir(directory)) &&
               monitor->battery_count < LSM_MAX_BATTERIES) {
            if (entry->d_name[0] == '.') continue;
            char path[LSM_PATH_LEN], type[64] = "";
            snprintf(path, sizeof(path), "%s/%s/type", root, entry->d_name);
            if (!lsm_read_text_file(path, type, sizeof(type)) ||
                strcmp(type, "Battery") != 0)
                continue;

            LsmBatteryInfo *battery =
                &monitor->batteries[monitor->battery_count++];
            memset(battery, 0, sizeof(*battery));
            initialise_battery_measurements(battery);
            lsm_copy_string(battery->name, sizeof(battery->name), entry->d_name);
            char base[LSM_PATH_LEN];
            snprintf(base, sizeof(base), "%s/%s", root, entry->d_name);
            (void)lsm_join_path(path, sizeof(path), base, "/model_name");
            lsm_read_text_file(path, battery->model, sizeof(battery->model));
            (void)lsm_join_path(path, sizeof(path), base, "/manufacturer");
            lsm_read_text_file(path, battery->manufacturer,
                               sizeof(battery->manufacturer));
            (void)lsm_join_path(path, sizeof(path), base, "/technology");
            lsm_read_text_file(path, battery->technology,
                               sizeof(battery->technology));
            (void)lsm_join_path(path, sizeof(path), base, "/scope");
            lsm_read_text_file(path, battery->scope, sizeof(battery->scope));
            (void)lsm_join_path(path, sizeof(path), base, "/serial_number");
            lsm_read_text_file(path, battery->serial, sizeof(battery->serial));
            battery->is_peripheral =
                strcasecmp(battery->scope, "Device") == 0;
            if (battery->is_peripheral &&
                !peripheral_supply_has_charge_telemetry(base)) {
                monitor->battery_count--;
                memset(battery, 0, sizeof(*battery));
                continue;
            }
            LsmLinuxBatteryState *battery_state = register_battery_state(
                monitor, battery->name);
            if (battery->is_peripheral &&
                (strcasecmp(battery->manufacturer, "Logitech") == 0 ||
                 strncmp(battery->name, "hidpp_battery_", 14U) == 0)) {
                if (battery_state)
                    (void)lsm_logitech_hidpp_find_device(
                        base, battery_state->hidraw_path,
                        sizeof(battery_state->hidraw_path));
                if (!battery->technology[0])
                    lsm_copy_string(battery->technology,
                                    sizeof(battery->technology),
                                    "Logitech HID++");
            }
        }
        closedir(directory);
    }
    merge_bluez_batteries(monitor);
    track_hidpp_batteries(monitor);
}

void lsm_battery_update(LsmMonitor *monitor)
{
    const bool ac = power_supply_online();
    const char *root = power_supply_root();
    for (size_t i = 0; i < monitor->battery_count; i++) {
        LsmBatteryInfo *battery = &monitor->batteries[i];
        LsmLinuxBatteryState *battery_state =
            find_battery_state(monitor, battery->name);
        if (battery_state && battery_state->bluez_record) continue;

        char base[LSM_PATH_LEN], path[LSM_PATH_LEN];
        snprintf(base, sizeof(base), "%s/%s", root, battery->name);
        (void)lsm_join_path(path, sizeof(path), base, "/present");
        battery->present = access(path, R_OK) != 0 ||
                           lsm_read_u64_or_zero(path) != 0;
        (void)lsm_join_path(path, sizeof(path), base, "/status");
        battery->status[0] = '\0';
        lsm_read_text_file(path, battery->status, sizeof(battery->status));
        (void)lsm_join_path(path, sizeof(path), base, "/health");
        battery->health[0] = '\0';
        lsm_read_text_file(path, battery->health, sizeof(battery->health));
        (void)lsm_join_path(path, sizeof(path), base, "/scope");
        lsm_read_text_file(path, battery->scope, sizeof(battery->scope));
        battery->is_peripheral =
            strcasecmp(battery->scope, "Device") == 0;
        (void)lsm_join_path(path, sizeof(path), base, "/capacity_level");
        battery->capacity_level[0] = '\0';
        lsm_read_text_file(path, battery->capacity_level,
                           sizeof(battery->capacity_level));
        (void)lsm_join_path(path, sizeof(path), base, "/capacity");
        battery->capacity_percent = lsm_read_double_or_nan(path);
        if (isfinite(battery->capacity_percent))
            battery->capacity_percent = fmin(
                100.0, fmax(0.0, battery->capacity_percent));
        else if (battery->has_supplemental_capacity)
            battery->capacity_percent = battery->supplemental_capacity_percent;

        (void)lsm_join_path(path, sizeof(path), base, "/voltage_now");
        const double micro_voltage = lsm_read_double_or_nan(path);
        battery->voltage_volts = isfinite(micro_voltage)
            ? micro_voltage / 1000000.0 : NAN;
        (void)lsm_join_path(path, sizeof(path), base, "/current_now");
        const double micro_current = lsm_read_double_or_nan(path);
        battery->current_amps = isfinite(micro_current)
            ? fabs(micro_current) / 1000000.0 : NAN;

        battery->energy_now_wh = battery_energy_wh(
            base, "energy_now", "charge_now", battery->voltage_volts);
        battery->energy_full_wh = battery_energy_wh(
            base, "energy_full", "charge_full", battery->voltage_volts);
        battery->energy_design_wh = battery_energy_wh(
            base, "energy_full_design", "charge_full_design",
            battery->voltage_volts);
        if ((!battery->health[0] ||
             strcmp(battery->health, "Unknown") == 0) &&
            isfinite(battery->energy_full_wh) &&
            isfinite(battery->energy_design_wh) &&
            battery->energy_design_wh > 0.0) {
            const double health_percent = fmin(100.0, fmax(
                0.0, 100.0 * battery->energy_full_wh /
                     battery->energy_design_wh));
            snprintf(battery->health, sizeof(battery->health),
                     "%.0f%%", health_percent);
        }
        (void)lsm_join_path(path, sizeof(path), base, "/power_now");
        const double micro_power = lsm_read_double_or_nan(path);
        battery->power_watts = isfinite(micro_power)
            ? fabs(micro_power) / 1000000.0 : NAN;
        if (!isfinite(battery->power_watts) &&
            isfinite(battery->voltage_volts) &&
            isfinite(battery->current_amps))
            battery->power_watts =
                battery->voltage_volts * battery->current_amps;

        (void)lsm_join_path(path, sizeof(path), base, "/temp");
        const double temperature = lsm_read_double_or_nan(path);
        battery->temperature_c = isfinite(temperature)
            ? temperature / 10.0 : NAN;
        (void)lsm_join_path(path, sizeof(path), base, "/cycle_count");
        const uint64_t cycle_count = lsm_read_u64_or_zero(path);
        battery->cycle_count = cycle_count > UINT_MAX
            ? UINT_MAX : (unsigned)cycle_count;
        battery->on_ac_power = !battery->is_peripheral && ac;

        uint64_t seconds = 0;
        const bool charging =
            strcasecmp(battery->status, "Charging") == 0;
        const bool discharging =
            strcasecmp(battery->status, "Discharging") == 0;
        if (charging || discharging) {
            const char *time_name = charging
                ? "time_to_full_now" : "time_to_empty_now";
            char time_suffix[64];
            snprintf(time_suffix, sizeof(time_suffix), "/%s", time_name);
            (void)lsm_join_path(path, sizeof(path), base, time_suffix);
            if (!lsm_read_u64_file(path, &seconds) &&
                isfinite(battery->power_watts) &&
                battery->power_watts > 0.01) {
                if (charging && isfinite(battery->energy_full_wh) &&
                    isfinite(battery->energy_now_wh))
                    seconds = battery_seconds_from_hours(
                        (battery->energy_full_wh - battery->energy_now_wh) /
                        battery->power_watts);
                else if (discharging && isfinite(battery->energy_now_wh))
                    seconds = battery_seconds_from_hours(
                        battery->energy_now_wh / battery->power_watts);
            }
        }
        battery->seconds_remaining = seconds;
        apply_hidpp_snapshot(monitor, battery);
    }
}

/* Public battery-worker lifecycle. */
void lsm_battery_start(void)
{
    (void)lsm_bluetooth_battery_start();
    (void)lsm_logitech_hidpp_start();
}

void lsm_battery_shutdown(void)
{
    lsm_logitech_hidpp_stop();
    lsm_bluetooth_battery_stop();
}
