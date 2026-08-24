// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
/**
 * @file users.c
 * @brief Logged-in users and sessions through systemd-logind D-Bus.
 *
 * Session identity comes directly from org.freedesktop.login1.  Resource totals
 * are calculated from the same retained /proc snapshot used by the Processes
 * tab, avoiding another complete process scan.  Parent rows represent users;
 * child rows represent graphical, terminal or remote sessions.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "users.h"
#include "app_internal.h"
#include "app_config.h"
#include "common.h"
#include "ui_helpers.h"

#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum {
    USER_COL_USER,
    USER_COL_SESSION,
    USER_COL_STATE,
    USER_COL_TYPE,
    USER_COL_LOCATION,
    USER_COL_LEADER,
    USER_COL_PROCESSES,
    USER_COL_CPU,
    USER_COL_MEMORY,
    USER_COL_LOGIN,
    USER_COL_SESSION_ID,
    USER_COL_UID,
    USER_COL_IS_SESSION,
    USER_COL_USERNAME,
    USER_N_COLUMNS
};

typedef struct {
    char id[64];
    uid_t uid;
    char username[64];
    char seat[64];
    char object_path[LSM_PATH_LEN];
    char state[32];
    char type[64];
    char session_class[64];
    char tty[64];
    char display[64];
    char remote_host[LSM_NAME_LEN];
    gboolean remote;
    guint32 leader;
    guint64 timestamp;
} SessionInfo;

typedef struct {
    uid_t uid;
    char username[64];
    char display_name[LSM_NAME_LEN];
    unsigned session_count;
    unsigned process_count;
    double cpu_percent;
    uint64_t rss_bytes;
} UserInfo;

typedef struct {
    SessionInfo *sessions;
    size_t session_count;
    char *error_message;
    char *preserve_session;
    char *preserve_username;
    gboolean preserve_is_session;
    gboolean cancelled;
} UserRefreshResult;

typedef struct {
    char *session;
    char *error_message;
    gboolean cancelled;
} UserActionResult;

/* login1 queries run in GTask workers; GTK receives only bounded snapshots. */
static GVariant *login_manager_call_on_bus(GDBusConnection *bus,
                                           const char *method,
                                           GVariant *parameters,
                                           int timeout_ms,
                                           GCancellable *cancellable,
                                           GError **error)
{
    if (!bus) return NULL;
    return g_dbus_connection_call_sync(bus,
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
    GVariant *reply = g_dbus_connection_call_sync(bus,
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
    GVariant *dictionary = g_variant_get_child_value(reply, 0);
    g_variant_unref(reply);
    return dictionary;
}

static void property_string(GVariant *dictionary, const char *key,
                            char *buffer, size_t buffer_size)
{
    GVariant *value = g_variant_lookup_value(dictionary, key, NULL);
    if (!value) return;
    const char *text = g_variant_get_string(value, NULL);
    if (text) g_strlcpy(buffer, text, buffer_size);
    g_variant_unref(value);
}

static gboolean property_boolean(GVariant *dictionary, const char *key)
{
    GVariant *value = g_variant_lookup_value(dictionary, key, NULL);
    if (!value) return FALSE;
    gboolean result = g_variant_get_boolean(value);
    g_variant_unref(value);
    return result;
}

static guint32 property_uint32(GVariant *dictionary, const char *key)
{
    GVariant *value = g_variant_lookup_value(dictionary, key, NULL);
    if (!value) return 0;
    guint32 result = g_variant_get_uint32(value);
    g_variant_unref(value);
    return result;
}

static guint64 property_uint64(GVariant *dictionary, const char *key)
{
    GVariant *value = g_variant_lookup_value(dictionary, key, NULL);
    if (!value) return 0;
    guint64 result = g_variant_get_uint64(value);
    g_variant_unref(value);
    return result;
}

static SessionInfo *parse_session_list(GVariant *reply, size_t *out_count)
{
    SessionInfo *sessions = NULL;
    size_t count = 0, capacity = 0;
    GVariantIter *iter = NULL;
    g_variant_get(reply, "(a(susso))", &iter);
    const char *id, *username, *seat, *path;
    guint32 uid;
    while (g_variant_iter_loop(iter, "(&su&s&s&o)", &id, &uid, &username, &seat, &path)) {
        if (!lsm_array_reserve((void **)&sessions, &capacity,
                               sizeof(*sessions), count + 1U, 8U))
            break;
        SessionInfo *session = &sessions[count++];
        memset(session, 0, sizeof(*session));
        g_strlcpy(session->id, id, sizeof(session->id));
        session->uid = (uid_t)uid;
        g_strlcpy(session->username, username, sizeof(session->username));
        g_strlcpy(session->seat, seat, sizeof(session->seat));
        g_strlcpy(session->object_path, path, sizeof(session->object_path));
    }
    g_variant_iter_free(iter);
    *out_count = count;
    return sessions;
}

static SessionInfo *collect_sessions(GDBusConnection *bus,
                                     GCancellable *cancellable,
                                     size_t *out_count, GError **error)
{
    *out_count = 0;
    GVariant *reply = login_manager_call_on_bus(
        bus, "ListSessions", NULL, LSM_DBUS_QUERY_TIMEOUT_MS,
        cancellable, error);
    if (!reply) return NULL;

    SessionInfo *sessions = parse_session_list(reply, out_count);
    g_variant_unref(reply);
    for (size_t index = 0; index < *out_count; index++) {
        if (g_cancellable_is_cancelled(cancellable)) break;
        SessionInfo *session = &sessions[index];
        GError *property_error = NULL;
        GVariant *properties = session_properties_on_bus(
            bus, session->object_path, cancellable, &property_error);
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
            session->leader = property_uint32(properties, "Leader");
            session->timestamp = property_uint64(properties, "Timestamp");
            g_variant_unref(properties);
        }
        if (property_error) g_error_free(property_error);
        if (!*session->state)
            g_strlcpy(session->state, "online", sizeof(session->state));
        if (!*session->type)
            g_strlcpy(session->type, "unspecified", sizeof(session->type));
    }
    return sessions;
}

static ssize_t user_find(UserInfo *users, size_t count, uid_t uid)
{
    for (size_t i = 0; i < count; i++) if (users[i].uid == uid) return (ssize_t)i;
    return -1;
}

static void fill_display_name(UserInfo *user)
{
    struct passwd *password = getpwuid(user->uid);
    if (!password) {
        g_strlcpy(user->display_name, user->username, sizeof(user->display_name));
        return;
    }
    if (password->pw_gecos && *password->pw_gecos) {
        char gecos[LSM_NAME_LEN];
        g_strlcpy(gecos, password->pw_gecos, sizeof(gecos));
        char *comma = strchr(gecos, ',');
        if (comma) *comma = '\0';
        g_strlcpy(user->display_name, gecos, sizeof(user->display_name));
    } else {
        g_strlcpy(user->display_name, password->pw_name, sizeof(user->display_name));
    }
}

/* Multiple sessions are grouped under one user while retaining child rows for
 * session-specific sign-out and location details. */
static UserInfo *aggregate_users(LsmApp *app, const SessionInfo *sessions,
                                 size_t session_count, size_t *out_count)
{
    if (out_count) *out_count = 0;
    if (session_count > 0 && !sessions) return NULL;
    UserInfo *users = calloc(session_count ? session_count : 1, sizeof(*users));
    if (!users) return NULL;
    size_t count = 0;
    for (size_t i = 0; i < session_count; i++) {
        ssize_t found = user_find(users, count, sessions[i].uid);
        UserInfo *user;
        if (found < 0) {
            user = &users[count++];
            user->uid = sessions[i].uid;
            g_strlcpy(user->username, sessions[i].username, sizeof(user->username));
            fill_display_name(user);
        } else user = &users[found];
        user->session_count++;
    }

    for (size_t i = 0; i < app->process.process_snapshot_count; i++) {
        const LsmProcessInfo *process = &app->process.process_snapshot[i];
        ssize_t found = -1;
        for (size_t user_index = 0; user_index < count; user_index++) {
            char account_identity[128];
            (void)snprintf(account_identity, sizeof(account_identity),
                           "uid:%llu",
                           (unsigned long long)users[user_index].uid);
            if (strcmp(account_identity, process->account_identity) == 0) {
                found = (ssize_t)user_index;
                break;
            }
        }
        if (found < 0) continue;
        UserInfo *user = &users[found];
        user->process_count++;
        user->cpu_percent = fmin(
            100.0, user->cpu_percent + process->cpu_percent);
        user->rss_bytes = lsm_u64_add_saturating(
            user->rss_bytes, process->rss_bytes);
    }
    if (out_count) *out_count = count;
    return users;
}

static void format_login_time(guint64 usec, char *buffer, size_t size)
{
    if (!usec) {
        g_strlcpy(buffer, "N/A", size);
        return;
    }
    time_t timestamp = (time_t)(usec / 1000000ULL);
    struct tm local;
    localtime_r(&timestamp, &local);
    strftime(buffer, size, "%d/%m/%Y %H:%M", &local);
}

static void session_location(const SessionInfo *session, char *buffer, size_t size)
{
    if (session->remote && *session->remote_host)
        snprintf(buffer, size, "Remote: %s", session->remote_host);
    else if (*session->display)
        snprintf(buffer, size, "%s%s%s", session->seat,
                 *session->seat ? " / " : "", session->display);
    else if (*session->tty)
        snprintf(buffer, size, "%s%s%s", session->seat,
                 *session->seat ? " / " : "", session->tty);
    else if (*session->seat)
        g_strlcpy(buffer, session->seat, size);
    else
        g_strlcpy(buffer, "Local", size);
}

static gboolean selected_user_row(LsmApp *app, char **session_id, char **username,
                                  guint *uid, gboolean *is_session)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->users.users_tree));
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) return FALSE;
    gtk_tree_model_get(model, &iter,
                       USER_COL_SESSION_ID, session_id,
                       USER_COL_USERNAME, username,
                       USER_COL_UID, uid,
                       USER_COL_IS_SESSION, is_session,
                       -1);
    return TRUE;
}

static gboolean restore_user_selection_level(LsmApp *app, GtkTreeModel *model,
                                             GtkTreeIter *parent,
                                             const char *session_id,
                                             const char *username,
                                             gboolean want_session)
{
    GtkTreeIter iter;
    if (!gtk_tree_model_iter_children(model, &iter, parent)) return FALSE;
    do {
        char *candidate_session = NULL, *candidate_user = NULL;
        gboolean is_session = FALSE;
        gtk_tree_model_get(model, &iter,
                           USER_COL_SESSION_ID, &candidate_session,
                           USER_COL_USERNAME, &candidate_user,
                           USER_COL_IS_SESSION, &is_session,
                           -1);
        gboolean match = is_session == want_session;
        if (match && want_session)
            match = session_id && candidate_session && strcmp(candidate_session, session_id) == 0;
        else if (match)
            match = username && candidate_user && strcmp(candidate_user, username) == 0;
        g_free(candidate_session);
        g_free(candidate_user);
        if (match) {
            GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->users.users_tree));
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(app->users.users_tree), path, NULL,
                                         FALSE, 0.0f, 0.0f);
            gtk_tree_path_free(path);
            return TRUE;
        }
        if (restore_user_selection_level(app, model, &iter, session_id, username, want_session))
            return TRUE;
    } while (gtk_tree_model_iter_next(model, &iter));
    return FALSE;
}

static void restore_user_selection(LsmApp *app, const char *session_id,
                                   const char *username, gboolean is_session)
{
    if ((!session_id || !*session_id) && (!username || !*username)) return;
    restore_user_selection_level(app, GTK_TREE_MODEL(app->users.users_store), NULL,
                                 session_id, username, is_session);
}

static void users_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
    LsmApp *app = user_data;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    gboolean selected = gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_widget_set_sensitive(app->users.user_processes_button, selected);
    gboolean is_session = FALSE;
    if (selected) gtk_tree_model_get(model, &iter, USER_COL_IS_SESSION, &is_session, -1);
    gtk_widget_set_sensitive(app->users.user_signout_button, selected && is_session);
}

static void user_show_processes(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *session = NULL, *username = NULL;
    guint uid = 0;
    gboolean is_session = FALSE;
    if (selected_user_row(app, &session, &username, &uid, &is_session)) {
        (void)uid; (void)is_session;
        gtk_entry_set_text(GTK_ENTRY(app->processes.processes_search),
                           username ? username : "");
        gtk_notebook_set_current_page(GTK_NOTEBOOK(app->shell.notebook),
                                      LSM_TAB_PROCESSES);
    }
    g_free(session);
    g_free(username);
}

static void user_action_result_free(gpointer data)
{
    UserActionResult *result = data;
    if (!result) return;
    g_free(result->session);
    g_free(result->error_message);
    g_free(result);
}

/* Session termination uses the selected login1 session identity, never a
 * username-derived shell command. */
static void user_action_worker(GTask *task, gpointer source_object,
                               gpointer task_data,
                               GCancellable *cancellable)
{
    (void)source_object;
    UserActionResult *result = task_data;
    GError *error = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, cancellable, &error);
    if (bus) {
        GVariant *parameters = g_variant_ref_sink(
            g_variant_new("(s)", result->session));
        GVariant *reply = login_manager_call_on_bus(
            bus, "TerminateSession", parameters,
            LSM_DBUS_ACTION_TIMEOUT_MS, cancellable, &error);
        g_variant_unref(parameters);
        if (reply) g_variant_unref(reply);
        g_object_unref(bus);
    }
    if (error) {
        result->error_message = g_strdup(error->message);
        g_error_free(error);
    }
    result->cancelled = g_cancellable_is_cancelled(cancellable);
    g_task_return_pointer(task, result, user_action_result_free);
}

static void user_action_complete(GObject *source_object,
                                 GAsyncResult *async_result,
                                 gpointer user_data)
{
    (void)source_object;
    LsmApp *app = user_data;
    UserActionResult *result = g_task_propagate_pointer(
        G_TASK(async_result), NULL);
    if (app->users.users_action_pending > 0) app->users.users_action_pending--;
    if (app->users.users_action_pending == 0 && app->users.users_action_cancellable) {
        g_object_unref(app->users.users_action_cancellable);
        app->users.users_action_cancellable = NULL;
    }
    if (result && !result->cancelled && !app->runtime.shutting_down) {
        if (result->error_message)
            lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                              "Unable to sign out session", "%s",
                              result->error_message);
        else
            lsm_users_refresh(app);
    }
    user_action_result_free(result);
}

static void terminate_session_async(LsmApp *app, const char *session)
{
    if (!app || app->runtime.shutting_down) return;
    UserActionResult *result = g_new0(UserActionResult, 1);
    result->session = g_strdup(session);
    if (!app->users.users_action_cancellable)
        app->users.users_action_cancellable = g_cancellable_new();
    app->users.users_action_pending++;
    GTask *task = g_task_new(NULL, app->users.users_action_cancellable,
                             user_action_complete, app);
    g_task_set_task_data(task, result, NULL);
    g_task_run_in_thread(task, user_action_worker);
    g_object_unref(task);
}

static void user_signout(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *session = NULL, *username = NULL;
    guint uid = 0;
    gboolean is_session = FALSE;
    if (!selected_user_row(app, &session, &username, &uid, &is_session) ||
        !is_session || !session || !*session) {
        g_free(session); g_free(username);
        return;
    }
    (void)uid;

    GtkWidget *confirm = gtk_message_dialog_new(GTK_WINDOW(app->shell.window),
        GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
        "Sign out session %s?", session);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(confirm),
        "The session belonging to %s will be terminated.",
        username ? username : "this user");
    const gint response = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(confirm);
    if (response == GTK_RESPONSE_YES) terminate_session_async(app, session);
    g_free(session);
    g_free(username);
}

static void users_refresh_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    lsm_users_refresh(user_data);
}

static GtkTreeViewColumn *users_column(GtkTreeView *tree, const char *title,
                                       int column, gboolean expand, int minimum)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *view_column = gtk_tree_view_column_new_with_attributes(title, renderer,
                                                                              "text", column, NULL);
    gtk_tree_view_column_set_resizable(view_column, TRUE);
    gtk_tree_view_column_set_expand(view_column, expand);
    gtk_tree_view_column_set_min_width(view_column, minimum);
    gtk_tree_view_append_column(tree, view_column);
    return view_column;
}

static void user_refresh_result_free(gpointer data)
{
    UserRefreshResult *result = data;
    if (!result) return;
    free(result->sessions);
    g_free(result->error_message);
    g_free(result->preserve_session);
    g_free(result->preserve_username);
    g_free(result);
}

/* Refresh pipeline: query login1, aggregate, then apply on the GTK thread. */
static void user_refresh_worker(GTask *task, gpointer source_object,
                                gpointer task_data,
                                GCancellable *cancellable)
{
    (void)source_object;
    UserRefreshResult *result = task_data;
    GError *error = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, cancellable, &error);
    if (bus) {
        result->sessions = collect_sessions(bus, cancellable,
                                            &result->session_count, &error);
        g_object_unref(bus);
    }
    if (error) {
        result->error_message = g_strdup(error->message);
        g_error_free(error);
    }
    result->cancelled = g_cancellable_is_cancelled(cancellable);
    g_task_return_pointer(task, result, user_refresh_result_free);
}

static void apply_user_refresh(LsmApp *app, UserRefreshResult *result)
{
    gtk_tree_store_clear(app->users.users_store);
    if (!result->sessions && result->error_message) {
        app->users.sessions_available = FALSE;
        GtkTreeIter iter;
        gtk_tree_store_append(app->users.users_store, &iter, NULL);
        gtk_tree_store_set(app->users.users_store, &iter,
                           USER_COL_USER, "Sessions unavailable",
                           USER_COL_STATE, result->error_message,
                           USER_COL_IS_SESSION, FALSE,
                           -1);
        lsm_ui_set_label_text(app->users.user_count_label,
                              "Session manager unavailable");
        return;
    }
    app->users.sessions_available = TRUE;

    if (result->session_count > 0 && !result->sessions) {
        app->users.sessions_available = FALSE;
        lsm_ui_set_label_text(app->users.user_count_label,
                              "Sessions unavailable: invalid session inventory");
        return;
    }

    size_t user_count = 0;
    UserInfo *users = aggregate_users(app, result->sessions,
                                      result->session_count, &user_count);
    for (size_t user_index = 0; user_index < user_count; user_index++) {
        UserInfo *user = &users[user_index];
        char process_text[32], cpu_text[32], memory_text[64], type_text[64];
        snprintf(process_text, sizeof(process_text), "%u", user->process_count);
        snprintf(cpu_text, sizeof(cpu_text), "%.1f%%", user->cpu_percent);
        lsm_format_bytes(user->rss_bytes, memory_text, sizeof(memory_text));
        snprintf(type_text, sizeof(type_text), "%u session%s", user->session_count,
                 user->session_count == 1 ? "" : "s");

        GtkTreeIter parent;
        gtk_tree_store_append(app->users.users_store, &parent, NULL);
        gtk_tree_store_set(app->users.users_store, &parent,
                           USER_COL_USER, user->display_name,
                           USER_COL_SESSION, "",
                           USER_COL_STATE, "Logged in",
                           USER_COL_TYPE, type_text,
                           USER_COL_LOCATION, "",
                           USER_COL_LEADER, "",
                           USER_COL_PROCESSES, process_text,
                           USER_COL_CPU, cpu_text,
                           USER_COL_MEMORY, memory_text,
                           USER_COL_LOGIN, "",
                           USER_COL_SESSION_ID, "",
                           USER_COL_UID, (guint)user->uid,
                           USER_COL_IS_SESSION, FALSE,
                           USER_COL_USERNAME, user->username,
                           -1);

        for (size_t session_index = 0;
             session_index < result->session_count; session_index++) {
            SessionInfo *session = &result->sessions[session_index];
            if (session->uid != user->uid) continue;
            char location[256], leader[32], login[64], type[128];
            session_location(session, location, sizeof(location));
            snprintf(leader, sizeof(leader), "%u", session->leader);
            format_login_time(session->timestamp, login, sizeof(login));
            snprintf(type, sizeof(type), "%.60s%s%.60s", session->type,
                     *session->session_class ? " / " : "", session->session_class);
            GtkTreeIter child;
            gtk_tree_store_append(app->users.users_store, &child, &parent);
            gtk_tree_store_set(app->users.users_store, &child,
                               USER_COL_USER, user->username,
                               USER_COL_SESSION, session->id,
                               USER_COL_STATE, session->state,
                               USER_COL_TYPE, type,
                               USER_COL_LOCATION, location,
                               USER_COL_LEADER, leader,
                               USER_COL_PROCESSES, "",
                               USER_COL_CPU, "",
                               USER_COL_MEMORY, "",
                               USER_COL_LOGIN, login,
                               USER_COL_SESSION_ID, session->id,
                               USER_COL_UID, (guint)user->uid,
                               USER_COL_IS_SESSION, TRUE,
                               USER_COL_USERNAME, user->username,
                               -1);
        }
    }
    free(users);
    lsm_ui_set_label_text(app->users.user_count_label, "%zu user%s, %zu session%s",
                          user_count, user_count == 1 ? "" : "s",
                          result->session_count,
                          result->session_count == 1 ? "" : "s");
    gtk_widget_set_sensitive(app->users.user_signout_button, FALSE);
    gtk_widget_set_sensitive(app->users.user_processes_button, FALSE);
    gtk_tree_view_expand_all(GTK_TREE_VIEW(app->users.users_tree));
    restore_user_selection(app, result->preserve_session,
                           result->preserve_username,
                           result->preserve_is_session);
}

static void user_refresh_complete(GObject *source_object,
                                  GAsyncResult *async_result,
                                  gpointer user_data)
{
    (void)source_object;
    LsmApp *app = user_data;
    UserRefreshResult *result = g_task_propagate_pointer(
        G_TASK(async_result), NULL);
    app->users.users_refresh_pending = FALSE;
    if (app->users.users_refresh_cancellable) {
        g_object_unref(app->users.users_refresh_cancellable);
        app->users.users_refresh_cancellable = NULL;
    }
    if (result && !result->cancelled && !app->runtime.shutting_down &&
        app->users.users_store)
        apply_user_refresh(app, result);
    user_refresh_result_free(result);
}

/* Public lifecycle and cadence control. */
void lsm_users_refresh(LsmApp *app)
{
    if (!app || app->runtime.shutting_down || !app->users.users_store ||
        app->users.users_refresh_pending) return;
    UserRefreshResult *result = g_new0(UserRefreshResult, 1);
    guint preserve_uid = 0;
    selected_user_row(app, &result->preserve_session,
                      &result->preserve_username, &preserve_uid,
                      &result->preserve_is_session);
    (void)preserve_uid;

    app->users.users_refresh_pending = TRUE;
    app->users.users_refresh_cancellable = g_cancellable_new();
    lsm_ui_set_label_text(app->users.user_count_label, "Refreshing sessions…");
    GTask *task = g_task_new(NULL, app->users.users_refresh_cancellable,
                             user_refresh_complete, app);
    g_task_set_task_data(task, result, NULL);
    g_task_run_in_thread(task, user_refresh_worker);
    g_object_unref(task);
}

gboolean lsm_users_update(gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app || app->runtime.paused) return G_SOURCE_CONTINUE;
    if (!app->shell.notebook || gtk_notebook_get_current_page(GTK_NOTEBOOK(app->shell.notebook)) == LSM_TAB_USERS)
        lsm_users_refresh(app);
    return G_SOURCE_CONTINUE;
}

void lsm_users_build(LsmApp *app, GtkWidget *container)
{
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 8);
    GtkWidget *heading = gtk_label_new("Logged-in users and sessions");
    gtk_widget_set_halign(heading, GTK_ALIGN_START);
    gtk_widget_set_hexpand(heading, TRUE);
    app->users.user_processes_button = gtk_button_new_with_label("Show processes");
    app->users.user_signout_button = gtk_button_new_with_label("Sign out session");
    GtkWidget *refresh = gtk_button_new_with_label("Refresh");
    gtk_widget_set_sensitive(app->users.user_processes_button, FALSE);
    gtk_widget_set_sensitive(app->users.user_signout_button, FALSE);
    g_signal_connect(app->users.user_processes_button, "clicked", G_CALLBACK(user_show_processes), app);
    g_signal_connect(app->users.user_signout_button, "clicked", G_CALLBACK(user_signout), app);
    g_signal_connect(refresh, "clicked", G_CALLBACK(users_refresh_clicked), app);
    gtk_box_pack_start(GTK_BOX(toolbar), heading, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->users.user_processes_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->users.user_signout_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), refresh, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), toolbar, FALSE, FALSE, 0);

    app->users.users_store = gtk_tree_store_new(USER_N_COLUMNS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_UINT, G_TYPE_BOOLEAN, G_TYPE_STRING);
    app->users.users_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->users.users_store));
    gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(app->users.users_tree), TRUE);
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(app->users.users_tree), TRUE);
    users_column(GTK_TREE_VIEW(app->users.users_tree), "User", USER_COL_USER, FALSE, 145);
    users_column(GTK_TREE_VIEW(app->users.users_tree), "Session", USER_COL_SESSION, FALSE, 70);
    users_column(GTK_TREE_VIEW(app->users.users_tree), "State", USER_COL_STATE, FALSE, 80);
    users_column(GTK_TREE_VIEW(app->users.users_tree), "Type", USER_COL_TYPE, FALSE, 120);
    users_column(GTK_TREE_VIEW(app->users.users_tree), "Location", USER_COL_LOCATION, TRUE, 150);
    users_column(GTK_TREE_VIEW(app->users.users_tree), "Leader PID", USER_COL_LEADER, FALSE, 75);
    users_column(GTK_TREE_VIEW(app->users.users_tree), "Processes", USER_COL_PROCESSES, FALSE, 75);
    users_column(GTK_TREE_VIEW(app->users.users_tree), "CPU", USER_COL_CPU, FALSE, 65);
    users_column(GTK_TREE_VIEW(app->users.users_tree), "Memory", USER_COL_MEMORY, FALSE, 85);
    users_column(GTK_TREE_VIEW(app->users.users_tree), "Login", USER_COL_LOGIN, FALSE, 135);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->users.users_tree));
    g_signal_connect(selection, "changed", G_CALLBACK(users_selection_changed), app);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    app->runtime.page_scrollers[LSM_TAB_USERS] = scroll;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), app->users.users_tree);
    gtk_box_pack_start(GTK_BOX(container), scroll, TRUE, TRUE, 0);

    app->users.user_count_label = gtk_label_new("");
    gtk_widget_set_halign(app->users.user_count_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->users.user_count_label, 8);
    gtk_widget_set_margin_bottom(app->users.user_count_label, 6);
    gtk_box_pack_start(GTK_BOX(container), app->users.user_count_label, FALSE, FALSE, 0);
    lsm_ui_set_label_text(app->users.user_count_label, "Select the Users tab to load sessions");
}

void lsm_users_destroy(LsmApp *app)
{
    if (!app) return;
    if (app->users.users_refresh_cancellable)
        g_cancellable_cancel(app->users.users_refresh_cancellable);
    if (app->users.users_action_cancellable)
        g_cancellable_cancel(app->users.users_action_cancellable);
    while (app->users.users_refresh_pending || app->users.users_action_pending > 0)
        (void)g_main_context_iteration(NULL, TRUE);
    if (app->users.users_refresh_cancellable) {
        g_object_unref(app->users.users_refresh_cancellable);
        app->users.users_refresh_cancellable = NULL;
    }
    if (app->users.users_action_cancellable) {
        g_object_unref(app->users.users_action_cancellable);
        app->users.users_action_cancellable = NULL;
    }
    if (app->users.users_store) g_object_unref(app->users.users_store);
    app->users.users_store = NULL;
}
