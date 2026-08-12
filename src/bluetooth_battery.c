// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file bluetooth_battery.c
 * @brief Native BlueZ Battery1 discovery with a non-blocking cache.
 *
 * BlueZ exposes exact percentages through org.bluez.Battery1. Calls are made
 * on a dedicated POSIX worker so a stopped or unhealthy Bluetooth daemon
 * cannot stall GTK or the regular monitor sampler.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "bluetooth_battery.h"

#include <gio/gio.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#define LSM_BLUEZ_BUS_NAME "org.bluez"
#define LSM_BLUEZ_ROOT_PATH "/"
#define LSM_OBJECT_MANAGER_INTERFACE "org.freedesktop.DBus.ObjectManager"
#define LSM_BLUEZ_DEVICE_INTERFACE "org.bluez.Device1"
#define LSM_BLUEZ_BATTERY_INTERFACE "org.bluez.Battery1"
#define LSM_BLUEZ_REFRESH_SECONDS 10
#define LSM_BLUEZ_TIMEOUT_MS 1500

/** Process-wide cache state; the monitor owns exactly one hardware sampler. */
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t thread;
    GCancellable *cancellable;
    LsmBluetoothBatteryRecord records[LSM_BLUETOOTH_BATTERY_MAX];
    size_t count;
    bool stop_requested;
    bool thread_started;
} LsmBluetoothBatteryState;

static LsmBluetoothBatteryState bluetooth_state = {
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .condition = PTHREAD_COND_INITIALIZER
};

static bool lookup_string(GVariant *properties, const char *key,
                          char *destination, size_t destination_size)
{
    const char *value = NULL;
    if (!properties || !key || !destination || destination_size == 0U ||
        !g_variant_lookup(properties, key, "&s", &value) || !value)
        return false;
    g_strlcpy(destination, value, destination_size);
    return destination[0] != '\0';
}

static bool lookup_boolean(GVariant *properties, const char *key,
                           bool *destination)
{
    gboolean value = FALSE;
    if (!properties || !key || !destination ||
        !g_variant_lookup(properties, key, "b", &value))
        return false;
    *destination = value != FALSE;
    return true;
}

size_t lsm_bluetooth_battery_parse_objects(
    GVariant *objects, LsmBluetoothBatteryRecord *records, size_t capacity)
{
    if (!objects || !records || capacity == 0U ||
        !g_variant_is_of_type(objects, G_VARIANT_TYPE("a{oa{sa{sv}}}")))
        return 0U;

    size_t count = 0U;
    GVariantIter *object_iterator = g_variant_iter_new(objects);
    if (!object_iterator) return 0U;
    const char *object_path = NULL;
    GVariant *interfaces = NULL;
    while (count < capacity &&
           g_variant_iter_next(object_iterator, "{&o@a{sa{sv}}}",
                               &object_path, &interfaces)) {
        GVariant *battery = g_variant_lookup_value(
            interfaces, LSM_BLUEZ_BATTERY_INTERFACE,
            G_VARIANT_TYPE("a{sv}"));
        GVariant *device = g_variant_lookup_value(
            interfaces, LSM_BLUEZ_DEVICE_INTERFACE,
            G_VARIANT_TYPE("a{sv}"));
        if (!battery || !device) {
            if (battery) g_variant_unref(battery);
            if (device) g_variant_unref(device);
            g_variant_unref(interfaces);
            continue;
        }

        /* Battery1 objects are useful only while their parent Device1 is
         * explicitly connected. Missing Connected data is not evidence of a
         * live device and must not resurrect a stale ObjectManager entry. */
        gboolean connected = FALSE;
        (void)g_variant_lookup(device, "Connected", "b", &connected);
        guint8 percentage = 0U;
        const gboolean has_percentage =
            g_variant_lookup(battery, "Percentage", "y", &percentage);
        if (!connected || !has_percentage || percentage > 100U) {
            g_variant_unref(device);
            g_variant_unref(battery);
            g_variant_unref(interfaces);
            continue;
        }

        LsmBluetoothBatteryRecord *record = &records[count];
        memset(record, 0, sizeof(*record));
        g_strlcpy(record->object_path, object_path, sizeof(record->object_path));
        if (!lookup_string(device, "Address", record->address,
                           sizeof(record->address))) {
            const char *marker = strstr(object_path, "/dev_");
            if (marker) {
                g_strlcpy(record->address, marker + 5,
                          sizeof(record->address));
                for (char *cursor = record->address; *cursor; cursor++)
                    if (*cursor == '_') *cursor = ':';
            }
        }
        if (!lookup_string(device, "Alias", record->name,
                           sizeof(record->name)))
            (void)lookup_string(device, "Name", record->name,
                                sizeof(record->name));
        if (!record->name[0])
            g_strlcpy(record->name, "Bluetooth device", sizeof(record->name));
        (void)lookup_string(battery, "Source", record->source,
                            sizeof(record->source));
        (void)lookup_string(device, "AddressType", record->address_type,
                            sizeof(record->address_type));
        (void)lookup_string(device, "Icon", record->icon,
                            sizeof(record->icon));
        (void)lookup_string(device, "Modalias", record->modalias,
                            sizeof(record->modalias));
        (void)lookup_boolean(device, "Paired", &record->paired);
        (void)lookup_boolean(device, "Trusted", &record->trusted);
        (void)lookup_boolean(device, "ServicesResolved",
                             &record->services_resolved);
        record->percentage = (double)percentage;
        record->connected = true;
        count++;

        g_variant_unref(device);
        g_variant_unref(battery);
        g_variant_unref(interfaces);
    }
    g_variant_iter_free(object_iterator);
    return count;
}

static size_t collect_bluez_batteries(GCancellable *cancellable,
                                      LsmBluetoothBatteryRecord *records,
                                      size_t capacity)
{
    GError *error = NULL;
    GDBusConnection *connection =
        g_bus_get_sync(G_BUS_TYPE_SYSTEM, cancellable, &error);
    if (!connection) {
        g_clear_error(&error);
        return 0U;
    }

    GVariant *reply = g_dbus_connection_call_sync(
        connection, LSM_BLUEZ_BUS_NAME, LSM_BLUEZ_ROOT_PATH,
        LSM_OBJECT_MANAGER_INTERFACE, "GetManagedObjects", NULL,
        G_VARIANT_TYPE("(a{oa{sa{sv}}})"), G_DBUS_CALL_FLAGS_NONE,
        LSM_BLUEZ_TIMEOUT_MS, cancellable, &error);
    g_object_unref(connection);
    if (!reply) {
        g_clear_error(&error);
        return 0U;
    }

    GVariant *objects = NULL;
    g_variant_get(reply, "(@a{oa{sa{sv}}})", &objects);
    const size_t count = lsm_bluetooth_battery_parse_objects(
        objects, records, capacity);
    g_variant_unref(objects);
    g_variant_unref(reply);
    return count;
}

static void wait_for_next_refresh(LsmBluetoothBatteryState *state)
{
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) return;
    deadline.tv_sec += LSM_BLUEZ_REFRESH_SECONDS;
    while (!state->stop_requested) {
        const int result = pthread_cond_timedwait(
            &state->condition, &state->mutex, &deadline);
        if (result != 0) break;
    }
}

static void *bluetooth_worker(void *user_data)
{
    LsmBluetoothBatteryState *state = user_data;
    for (;;) {
        pthread_mutex_lock(&state->mutex);
        const bool stop = state->stop_requested;
        GCancellable *cancellable = state->cancellable
            ? g_object_ref(state->cancellable) : NULL;
        pthread_mutex_unlock(&state->mutex);
        if (stop || !cancellable) {
            if (cancellable) g_object_unref(cancellable);
            break;
        }

        LsmBluetoothBatteryRecord records[LSM_BLUETOOTH_BATTERY_MAX] = {0};
        const size_t count = collect_bluez_batteries(
            cancellable, records, LSM_BLUETOOTH_BATTERY_MAX);
        g_object_unref(cancellable);

        pthread_mutex_lock(&state->mutex);
        if (!state->stop_requested) {
            memcpy(state->records, records, sizeof(records));
            state->count = count;
            wait_for_next_refresh(state);
        }
        const bool finished = state->stop_requested;
        pthread_mutex_unlock(&state->mutex);
        if (finished) break;
    }
    return NULL;
}

bool lsm_bluetooth_battery_start(void)
{
    pthread_mutex_lock(&bluetooth_state.mutex);
    if (bluetooth_state.thread_started) {
        pthread_mutex_unlock(&bluetooth_state.mutex);
        return true;
    }
    bluetooth_state.stop_requested = false;
    bluetooth_state.count = 0U;
    memset(bluetooth_state.records, 0, sizeof(bluetooth_state.records));
    bluetooth_state.cancellable = g_cancellable_new();
    if (!bluetooth_state.cancellable) {
        pthread_mutex_unlock(&bluetooth_state.mutex);
        return false;
    }
    const int result = pthread_create(
        &bluetooth_state.thread, NULL, bluetooth_worker, &bluetooth_state);
    if (result != 0) {
        g_object_unref(bluetooth_state.cancellable);
        bluetooth_state.cancellable = NULL;
        pthread_mutex_unlock(&bluetooth_state.mutex);
        return false;
    }
    bluetooth_state.thread_started = true;
    pthread_mutex_unlock(&bluetooth_state.mutex);
    return true;
}

size_t lsm_bluetooth_battery_snapshot(LsmBluetoothBatteryRecord *records,
                                      size_t capacity)
{
    if (!records || capacity == 0U) return 0U;
    pthread_mutex_lock(&bluetooth_state.mutex);
    size_t count = bluetooth_state.count;
    if (count > capacity) count = capacity;
    memcpy(records, bluetooth_state.records, count * sizeof(records[0]));
    pthread_mutex_unlock(&bluetooth_state.mutex);
    return count;
}

void lsm_bluetooth_battery_stop(void)
{
    pthread_mutex_lock(&bluetooth_state.mutex);
    if (!bluetooth_state.thread_started) {
        pthread_mutex_unlock(&bluetooth_state.mutex);
        return;
    }
    bluetooth_state.stop_requested = true;
    if (bluetooth_state.cancellable)
        g_cancellable_cancel(bluetooth_state.cancellable);
    pthread_cond_broadcast(&bluetooth_state.condition);
    const pthread_t thread = bluetooth_state.thread;
    pthread_mutex_unlock(&bluetooth_state.mutex);

    pthread_join(thread, NULL);

    pthread_mutex_lock(&bluetooth_state.mutex);
    bluetooth_state.thread_started = false;
    if (bluetooth_state.cancellable) {
        g_object_unref(bluetooth_state.cancellable);
        bluetooth_state.cancellable = NULL;
    }
    bluetooth_state.count = 0U;
    memset(bluetooth_state.records, 0, sizeof(bluetooth_state.records));
    pthread_mutex_unlock(&bluetooth_state.mutex);
}
