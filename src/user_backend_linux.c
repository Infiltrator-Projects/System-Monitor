// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
/**
 * @file user_backend_linux.c
 * @brief Linux logged-in session backend using systemd-logind D-Bus.
 *
 * Session identity comes directly from org.freedesktop.login1. Account display
 * names are resolved through the native user database. No loginctl command or
 * shell provider is involved.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "user_backend.h"

#include "app_config.h"
#include "common.h"

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct {
    LsmUserSession session;
    uid_t uid;
    char object_path[LSM_PATH_LEN];
} LsmLinuxSessionRecord;

static GVariant *login_manager_call_on_bus(GDBusConnection *bus,
                                           const char *method,
                                           GVariant *parameters,
                                           int timeout_ms,
                                           GCancellable *cancellable,
                                           GError **error)
{
    if (!bus) return NULL;
    return g_dbus_connection_call_sync(
        bus,
        "org.freedesktop.login1",
        "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager",
        method,
        parameters,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        timeout_ms,
        cancellable,
        error);
}

static GVariant *session_properties_on_bus(GDBusConnection *bus,
                                           const char *object_path,
                                           GCancellable *cancellable,
                                           GError **error)
{
    if (!bus) return NULL;
    GVariant *reply = g_dbus_connection_call_sync(
        bus,
        "org.freedesktop.login1",
        object_path,
        "org.freedesktop.DBus.Properties",
        "GetAll",
        g_variant_new("(s)", "org.freedesktop.login1.Session"),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        LSM_DBUS_QUERY_TIMEOUT_MS,
        cancellable,
        error);
    if (!reply) return NULL;
    GVariant *dictionary = g_variant_get_child_value(reply, 0U);
    g_variant_unref(reply);
    return dictionary;
}

static void property_string(GVariant *dictionary, const char *key,
                            char *buffer, size_t buffer_size)
{
    GVariant *value = g_variant_lookup_value(dictionary, key, NULL);
    if (!value) return;
    const char *text = g_variant_get_string(value, NULL);
    if (text) lsm_copy_string(buffer, buffer_size, text);
    g_variant_unref(value);
}

static gboolean property_boolean(GVariant *dictionary, const char *key)
{
    GVariant *value = g_variant_lookup_value(dictionary, key, NULL);
    if (!value) return FALSE;
    const gboolean result = g_variant_get_boolean(value);
    g_variant_unref(value);
    return result;
}

static guint32 property_uint32(GVariant *dictionary, const char *key)
{
    GVariant *value = g_variant_lookup_value(dictionary, key, NULL);
    if (!value) return 0U;
    const guint32 result = g_variant_get_uint32(value);
    g_variant_unref(value);
    return result;
}

static guint64 property_uint64(GVariant *dictionary, const char *key)
{
    GVariant *value = g_variant_lookup_value(dictionary, key, NULL);
    if (!value) return 0U;
    const guint64 result = g_variant_get_uint64(value);
    g_variant_unref(value);
    return result;
}

static void fill_account_identity(LsmLinuxSessionRecord *record)
{
    if (!record) return;
    (void)snprintf(record->session.account_identity,
                   sizeof(record->session.account_identity),
                   "uid:%llu", (unsigned long long)record->uid);

    struct passwd *password = getpwuid(record->uid);
    if (password && password->pw_gecos && password->pw_gecos[0]) {
        char gecos[LSM_NAME_LEN];
        lsm_copy_string(gecos, sizeof(gecos), password->pw_gecos);
        char *comma = strchr(gecos, ',');
        if (comma) *comma = '\0';
        lsm_copy_string(record->session.display_name, sizeof(record->session.display_name), gecos);
    } else if (password && password->pw_name && password->pw_name[0]) {
        lsm_copy_string(record->session.display_name, sizeof(record->session.display_name),
                        password->pw_name);
    } else {
        lsm_copy_string(record->session.display_name, sizeof(record->session.display_name),
                        record->session.username);
    }
}

static LsmLinuxSessionRecord *parse_session_list(GVariant *reply,
                                                 size_t *out_count)
{
    LsmLinuxSessionRecord *sessions = NULL;
    size_t count = 0U;
    size_t capacity = 0U;
    GVariantIter *iter = NULL;
    g_variant_get(reply, "(a(susso))", &iter);
    const char *id = NULL;
    const char *username = NULL;
    const char *seat = NULL;
    const char *path = NULL;
    guint32 uid = 0U;

    while (g_variant_iter_loop(iter, "(&su&s&s&o)", &id, &uid, &username,
                               &seat, &path)) {
        if (!lsm_array_reserve((void **)&sessions, &capacity,
                               sizeof(*sessions), count + 1U, 8U))
            break;
        LsmLinuxSessionRecord *record = &sessions[count++];
        memset(record, 0, sizeof(*record));
        record->uid = (uid_t)uid;
        lsm_copy_string(record->session.id, sizeof(record->session.id), id);
        lsm_copy_string(record->session.username, sizeof(record->session.username), username);
        lsm_copy_string(record->session.seat, sizeof(record->session.seat), seat);
        lsm_copy_string(record->object_path, sizeof(record->object_path), path);
        fill_account_identity(record);
    }
    g_variant_iter_free(iter);
    *out_count = count;
    return sessions;
}

static LsmLinuxSessionRecord *collect_sessions(GDBusConnection *bus,
                                               GCancellable *cancellable,
                                               size_t *out_count,
                                               GError **error)
{
    *out_count = 0U;
    GVariant *reply = login_manager_call_on_bus(
        bus, "ListSessions", NULL, LSM_DBUS_QUERY_TIMEOUT_MS,
        cancellable, error);
    if (!reply) return NULL;

    LsmLinuxSessionRecord *sessions = parse_session_list(reply, out_count);
    g_variant_unref(reply);
    for (size_t index = 0U; index < *out_count; index++) {
        if (cancellable && g_cancellable_is_cancelled(cancellable)) break;
        LsmLinuxSessionRecord *record = &sessions[index];
        LsmUserSession *session = &record->session;
        GError *property_error = NULL;
        GVariant *properties = session_properties_on_bus(
            bus, record->object_path, cancellable, &property_error);
        if (properties) {
            property_string(properties, "State", session->state,
                            sizeof(session->state));
            property_string(properties, "Type", session->type,
                            sizeof(session->type));
            property_string(properties, "Class", session->session_class,
                            sizeof(session->session_class));
            property_string(properties, "TTY", session->tty,
                            sizeof(session->tty));
            property_string(properties, "Display", session->display,
                            sizeof(session->display));
            property_string(properties, "RemoteHost", session->remote_host,
                            sizeof(session->remote_host));
            session->remote = property_boolean(properties, "Remote");
            session->leader =
                (LsmProcessId)property_uint32(properties, "Leader");
            session->timestamp_usec =
                (uint64_t)property_uint64(properties, "Timestamp");
            g_variant_unref(properties);
        }
        if (property_error) g_error_free(property_error);
        if (!session->state[0])
            lsm_copy_string(session->state, sizeof(session->state), "online");
        if (!session->type[0])
            lsm_copy_string(session->type, sizeof(session->type), "unspecified");
    }
    return sessions;
}

bool lsm_user_backend_collect(LsmUserSession **out_sessions,
                              size_t *out_count,
                              GCancellable *cancellable,
                              GError **error)
{
    if (!out_sessions || !out_count) return false;
    *out_sessions = NULL;
    *out_count = 0U;

    GDBusConnection *bus =
        g_bus_get_sync(G_BUS_TYPE_SYSTEM, cancellable, error);
    if (!bus) return false;

    GError *collect_error = NULL;
    size_t count = 0U;
    LsmLinuxSessionRecord *native =
        collect_sessions(bus, cancellable, &count, &collect_error);
    g_object_unref(bus);
    if (collect_error) {
        if (error) *error = collect_error;
        else g_error_free(collect_error);
        free(native);
        return false;
    }

    LsmUserSession *sessions =
        count > 0U ? calloc(count, sizeof(*sessions)) : NULL;
    if (count > 0U && !sessions) {
        free(native);
        return false;
    }
    for (size_t index = 0U; index < count; index++)
        sessions[index] = native[index].session;
    free(native);

    *out_sessions = sessions;
    *out_count = count;
    return true;
}

bool lsm_user_backend_terminate_session(const char *session_id,
                                        GCancellable *cancellable,
                                        GError **error)
{
    if (!session_id || !session_id[0]) return false;
    GDBusConnection *bus =
        g_bus_get_sync(G_BUS_TYPE_SYSTEM, cancellable, error);
    if (!bus) return false;

    GVariant *parameters =
        g_variant_ref_sink(g_variant_new("(s)", session_id));
    GVariant *reply = login_manager_call_on_bus(
        bus, "TerminateSession", parameters, LSM_DBUS_ACTION_TIMEOUT_MS,
        cancellable, error);
    g_variant_unref(parameters);
    g_object_unref(bus);
    if (!reply) return false;
    g_variant_unref(reply);
    return true;
}

void lsm_user_backend_free(LsmUserSession *sessions)
{
    free(sessions);
}
