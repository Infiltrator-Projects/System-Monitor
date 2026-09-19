// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file service_backend_linux.c
 * @brief Native Linux service inventory and control through systemd D-Bus.
 *
 * This backend talks directly to org.freedesktop.systemd1 through GDBus. It
 * never invokes systemctl or parses command output. Read-only inventory calls
 * require no privilege; mutating requests are authorised by systemd/polkit.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "service_backend.h"

#include "app_config.h"
#include "common.h"

#include <stdlib.h>
#include <string.h>

static GVariant *manager_call_on_bus(GDBusConnection *bus, const char *method,
                                     GVariant *parameters, int timeout_ms,
                                     GCancellable *cancellable, GError **error)
{
    if (!bus) return NULL;
    return g_dbus_connection_call_sync(
        bus,
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        method,
        parameters,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        timeout_ms,
        cancellable,
        error);
}

static ssize_t service_find(LsmServiceEntry *entries, size_t count,
                            const char *name)
{
    for (size_t index = 0U; index < count; index++)
        if (strcmp(entries[index].name, name) == 0) return (ssize_t)index;
    return -1;
}

static LsmServiceEntry *service_get(LsmServiceEntry **entries, size_t *count,
                                    size_t *capacity, const char *name)
{
    const ssize_t existing = service_find(*entries, *count, name);
    if (existing >= 0) return &(*entries)[existing];
    if (!lsm_array_reserve((void **)entries, capacity, sizeof(**entries),
                           *count + 1U, 128U))
        return NULL;

    LsmServiceEntry *entry = &(*entries)[(*count)++];
    memset(entry, 0, sizeof(*entry));
    lsm_copy_string(entry->name, sizeof(entry->name), name);
    lsm_copy_string(entry->description, sizeof(entry->description), name);
    lsm_copy_string(entry->active, sizeof(entry->active), "inactive");
    lsm_copy_string(entry->substate, sizeof(entry->substate), "dead");
    lsm_copy_string(entry->startup, sizeof(entry->startup), "unknown");
    return entry;
}

static int service_compare(const void *left, const void *right)
{
    const LsmServiceEntry *a = left;
    const LsmServiceEntry *b = right;
    return strcmp(a->name, b->name);
}

static void merge_loaded_units(GVariant *units, LsmServiceEntry **entries,
                               size_t *count, size_t *capacity)
{
    GVariantIter *iter = NULL;
    g_variant_get(units, "(a(ssssssouso))", &iter);
    const char *name = NULL;
    const char *description = NULL;
    const char *load = NULL;
    const char *active = NULL;
    const char *substate = NULL;
    const char *following = NULL;
    const char *object_path = NULL;
    const char *job_type = NULL;
    const char *job_path = NULL;
    guint32 job_id = 0U;

    while (g_variant_iter_loop(iter, "(&s&s&s&s&s&s&ou&s&o)",
                               &name, &description, &load, &active, &substate,
                               &following, &object_path, &job_id, &job_type,
                               &job_path)) {
        (void)load;
        (void)following;
        (void)object_path;
        (void)job_id;
        (void)job_type;
        (void)job_path;
        const size_t length = strlen(name);
        if (length < 8U || strcmp(name + length - 8U, ".service") != 0) continue;
        LsmServiceEntry *entry = service_get(entries, count, capacity, name);
        if (!entry) break;
        lsm_copy_string(entry->description, sizeof(entry->description), description);
        lsm_copy_string(entry->active, sizeof(entry->active), active);
        lsm_copy_string(entry->substate, sizeof(entry->substate), substate);
    }
    g_variant_iter_free(iter);
}

static void merge_unit_files(GVariant *files, LsmServiceEntry **entries,
                             size_t *count, size_t *capacity)
{
    GVariantIter *iter = NULL;
    g_variant_get(files, "(a(ss))", &iter);
    const char *path = NULL;
    const char *state = NULL;

    while (g_variant_iter_loop(iter, "(&s&s)", &path, &state)) {
        const char *unit = lsm_path_basename(path);
        const size_t length = strlen(unit);
        if (length < 8U || strcmp(unit + length - 8U, ".service") != 0) continue;
        LsmServiceEntry *entry = service_get(entries, count, capacity, unit);
        if (!entry) break;
        lsm_copy_string(entry->startup, sizeof(entry->startup), state);
    }
    g_variant_iter_free(iter);
}

static LsmServiceEntry *collect_services(GDBusConnection *bus,
                                         GCancellable *cancellable,
                                         size_t *out_count, GError **error)
{
    LsmServiceEntry *entries = NULL;
    size_t count = 0U;
    size_t capacity = 0U;

    GVariant *units = manager_call_on_bus(
        bus, "ListUnits", NULL, LSM_DBUS_QUERY_TIMEOUT_MS, cancellable, error);
    if (!units) return NULL;
    merge_loaded_units(units, &entries, &count, &capacity);
    g_variant_unref(units);

    GError *files_error = NULL;
    GVariant *files = manager_call_on_bus(
        bus, "ListUnitFiles", NULL, LSM_DBUS_QUERY_TIMEOUT_MS, cancellable,
        &files_error);
    if (files) {
        merge_unit_files(files, &entries, &count, &capacity);
        g_variant_unref(files);
    }
    if (files_error) g_error_free(files_error);

    if (count > 1U) qsort(entries, count, sizeof(*entries), service_compare);
    *out_count = count;
    return entries;
}

bool lsm_service_backend_collect(LsmServiceEntry **out_entries,
                                 size_t *out_count,
                                 GCancellable *cancellable,
                                 GError **error)
{
    if (!out_entries || !out_count) return false;
    *out_entries = NULL;
    *out_count = 0U;

    GDBusConnection *bus =
        g_bus_get_sync(G_BUS_TYPE_SYSTEM, cancellable, error);
    if (!bus) return false;

    GError *collect_error = NULL;
    LsmServiceEntry *entries =
        collect_services(bus, cancellable, out_count, &collect_error);
    g_object_unref(bus);
    if (collect_error) {
        if (error) *error = collect_error;
        else g_error_free(collect_error);
        free(entries);
        *out_count = 0U;
        return false;
    }

    *out_entries = entries;
    return true;
}

bool lsm_service_backend_state_is_enabled(const char *state)
{
    return state &&
           (lsm_string_starts_with(state, "enabled") ||
            lsm_string_starts_with(state, "linked") ||
            strcmp(state, "alias") == 0);
}

bool lsm_service_backend_action(const char *name,
                                LsmServiceAction action,
                                GCancellable *cancellable,
                                GError **error)
{
    if (!name || !name[0]) return false;

    const char *method = NULL;
    GVariant *parameters = NULL;
    bool reload_after = false;
    const char *units[] = {name, NULL};

    switch (action) {
    case LSM_SERVICE_ACTION_START:
        method = "StartUnit";
        parameters = g_variant_new("(ss)", name, "replace");
        break;
    case LSM_SERVICE_ACTION_STOP:
        method = "StopUnit";
        parameters = g_variant_new("(ss)", name, "replace");
        break;
    case LSM_SERVICE_ACTION_RESTART:
        method = "RestartUnit";
        parameters = g_variant_new("(ss)", name, "replace");
        break;
    case LSM_SERVICE_ACTION_ENABLE:
        method = "EnableUnitFiles";
        parameters = g_variant_new("(^asbb)", units, FALSE, TRUE);
        reload_after = true;
        break;
    case LSM_SERVICE_ACTION_DISABLE:
        method = "DisableUnitFiles";
        parameters = g_variant_new("(^asb)", units, FALSE);
        reload_after = true;
        break;
    default:
        return false;
    }

    parameters = g_variant_ref_sink(parameters);
    GDBusConnection *bus =
        g_bus_get_sync(G_BUS_TYPE_SYSTEM, cancellable, error);
    if (!bus) {
        g_variant_unref(parameters);
        return false;
    }

    GVariant *reply = manager_call_on_bus(
        bus, method, parameters, LSM_DBUS_ACTION_TIMEOUT_MS, cancellable, error);
    g_variant_unref(parameters);
    if (!reply) {
        g_object_unref(bus);
        return false;
    }
    g_variant_unref(reply);

    if (reload_after) {
        reply = manager_call_on_bus(
            bus, "Reload", NULL, LSM_DBUS_ACTION_TIMEOUT_MS, cancellable, error);
        if (!reply) {
            g_object_unref(bus);
            return false;
        }
        g_variant_unref(reply);
    }
    g_object_unref(bus);
    return true;
}

void lsm_service_backend_free(LsmServiceEntry *entries)
{
    free(entries);
}
