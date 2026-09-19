// SPDX-License-Identifier: GPL-3.0-or-later
#define _POSIX_C_SOURCE 200809L
/**
 * @file users.c
 * @brief GTK presentation and process aggregation for logged-in sessions.
 *
 * Native session discovery and sign-out are delegated through user_backend.h.
 * Resource totals are calculated from the retained process snapshot, avoiding
 * another process scan. Parent rows represent users; child rows represent
 * graphical, terminal or remote sessions.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "users.h"
#include "app_internal.h"
#include "common.h"
#include "ui_helpers.h"
#include "user_backend.h"

#include <math.h>
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
    USER_COL_IS_SESSION,
    USER_COL_USERNAME,
    USER_N_COLUMNS
};

typedef struct {
    char account_identity[128];
    char username[64];
    char display_name[LSM_NAME_LEN];
    unsigned session_count;
    unsigned process_count;
    double cpu_percent;
    uint64_t rss_bytes;
} UserInfo;

typedef struct {
    LsmUserSession *sessions;
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

static ssize_t user_find(UserInfo *users, size_t count,
                         const char *account_identity)
{
    if (!account_identity || !account_identity[0]) return -1;
    for (size_t index = 0U; index < count; index++)
        if (strcmp(users[index].account_identity, account_identity) == 0)
            return (ssize_t)index;
    return -1;
}

/* Multiple sessions are grouped under one user while retaining child rows for
 * session-specific sign-out and location details. */
static UserInfo *aggregate_users(LsmApp *app, const LsmUserSession *sessions,
                                 size_t session_count, size_t *out_count)
{
    if (out_count) *out_count = 0;
    if (session_count > 0 && !sessions) return NULL;
    UserInfo *users = calloc(session_count ? session_count : 1, sizeof(*users));
    if (!users) return NULL;
    size_t count = 0;
    for (size_t i = 0; i < session_count; i++) {
        ssize_t found = user_find(
            users, count, sessions[i].account_identity);
        UserInfo *user;
        if (found < 0) {
            user = &users[count++];
            g_strlcpy(user->account_identity,
                      sessions[i].account_identity,
                      sizeof(user->account_identity));
            g_strlcpy(user->username, sessions[i].username,
                      sizeof(user->username));
            g_strlcpy(user->display_name, sessions[i].display_name,
                      sizeof(user->display_name));
        } else {
            user = &users[found];
        }
        user->session_count++;
    }

    for (size_t i = 0; i < app->process.process_snapshot_count; i++) {
        const LsmProcessInfo *process = &app->process.process_snapshot[i];
        const ssize_t found =
            user_find(users, count, process->account_identity);
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

static void format_login_time(uint64_t usec, char *buffer, size_t size)
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

static void session_location(const LsmUserSession *session, char *buffer, size_t size)
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
                                  gboolean *is_session)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->users.users_tree));
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) return FALSE;
    gtk_tree_model_get(model, &iter,
                       USER_COL_SESSION_ID, session_id,
                       USER_COL_USERNAME, username,
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
    gboolean is_session = FALSE;
    if (selected_user_row(app, &session, &username, &is_session)) {
        (void)is_session;
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
    (void)lsm_user_backend_terminate_session(
        result->session, cancellable, &error);
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
    gboolean is_session = FALSE;
    if (!selected_user_row(app, &session, &username, &is_session) ||
        !is_session || !session || !*session) {
        g_free(session); g_free(username);
        return;
    }
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
    lsm_user_backend_free(result->sessions);
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
    (void)lsm_user_backend_collect(
        &result->sessions, &result->session_count, cancellable, &error);
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
                           USER_COL_IS_SESSION, FALSE,
                           USER_COL_USERNAME, user->username,
                           -1);

        for (size_t session_index = 0;
             session_index < result->session_count; session_index++) {
            LsmUserSession *session = &result->sessions[session_index];
            if (strcmp(session->account_identity,
                       user->account_identity) != 0)
                continue;
            char location[256], leader[32], login[64], type[128];
            session_location(session, location, sizeof(location));
            snprintf(leader, sizeof(leader), "%llu",
                     (unsigned long long)session->leader);
            format_login_time(session->timestamp_usec, login, sizeof(login));
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
    selected_user_row(app, &result->preserve_session,
                      &result->preserve_username,
                      &result->preserve_is_session);

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
        G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);
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
