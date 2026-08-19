// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file services.c
 * @brief systemd service inventory and control through the system D-Bus.
 *
 * The tab talks directly to org.freedesktop.systemd1 through GDBus.  It does
 * not parse command output or depend on systemctl.  Read-only inventory calls
 * require no privilege.  Start, stop, restart, enable and disable requests are
 * authorised by systemd/polkit only when the user actually invokes them.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "services.h"
#include "app_internal.h"
#include "app_config.h"
#include "ui_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SERVICE_COL_NAME,
    SERVICE_COL_DESCRIPTION,
    SERVICE_COL_STATUS,
    SERVICE_COL_SUBSTATE,
    SERVICE_COL_STARTUP,
    SERVICE_COL_ACTIVE,
    SERVICE_N_COLUMNS
};

typedef struct {
    char name[LSM_NAME_LEN];
    char description[256];
    char active[32];
    char substate[64];
    char startup[64];
} ServiceEntry;

typedef struct {
    ServiceEntry *entries;
    size_t count;
    char *error_message;
    char *preserve_name;
    gboolean cancelled;
} ServiceRefreshResult;

typedef struct {
    char *method;
    GVariant *parameters;
    char *failure_title;
    char *error_message;
    gboolean reload_after;
    gboolean cancelled;
} ServiceActionResult;

/* systemd D-Bus access is kept off the GTK thread. Synchronous calls below
 * execute only inside GTask workers with bounded timeouts. */
static GVariant *manager_call_on_bus(GDBusConnection *bus, const char *method,
                                     GVariant *parameters, int timeout_ms,
                                     GCancellable *cancellable, GError **error)
{
    if (!bus) return NULL;
    return g_dbus_connection_call_sync(bus,
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

static ssize_t service_find(ServiceEntry *entries, size_t count, const char *name)
{
    for (size_t i = 0; i < count; i++)
        if (strcmp(entries[i].name, name) == 0) return (ssize_t)i;
    return -1;
}

static ServiceEntry *service_get(ServiceEntry **entries, size_t *count,
                                 size_t *capacity, const char *name)
{
    ssize_t existing = service_find(*entries, *count, name);
    if (existing >= 0) return &(*entries)[existing];
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 128;
        ServiceEntry *grown = realloc(*entries, next * sizeof(*grown));
        if (!grown) return NULL;
        *entries = grown;
        *capacity = next;
    }
    ServiceEntry *entry = &(*entries)[(*count)++];
    memset(entry, 0, sizeof(*entry));
    g_strlcpy(entry->name, name, sizeof(entry->name));
    g_strlcpy(entry->description, name, sizeof(entry->description));
    g_strlcpy(entry->active, "inactive", sizeof(entry->active));
    g_strlcpy(entry->substate, "dead", sizeof(entry->substate));
    g_strlcpy(entry->startup, "unknown", sizeof(entry->startup));
    return entry;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int service_compare(const void *left, const void *right)
{
    const ServiceEntry *a = left;
    const ServiceEntry *b = right;
    return strcmp(a->name, b->name);
}

static void merge_loaded_units(GVariant *units, ServiceEntry **entries,
                               size_t *count, size_t *capacity)
{
    GVariantIter *iter = NULL;
    g_variant_get(units, "(a(ssssssouso))", &iter);
    const char *name, *description, *load, *active, *substate, *following;
    const char *object_path, *job_type, *job_path;
    guint32 job_id;
    while (g_variant_iter_loop(iter, "(&s&s&s&s&s&s&ou&s&o)",
                               &name, &description, &load, &active, &substate,
                               &following, &object_path, &job_id, &job_type, &job_path)) {
        (void)load; (void)following; (void)object_path; (void)job_id;
        (void)job_type; (void)job_path;
        size_t length = strlen(name);
        if (length < 8 || strcmp(name + length - 8, ".service") != 0) continue;
        ServiceEntry *entry = service_get(entries, count, capacity, name);
        if (!entry) break;
        g_strlcpy(entry->description, description, sizeof(entry->description));
        g_strlcpy(entry->active, active, sizeof(entry->active));
        g_strlcpy(entry->substate, substate, sizeof(entry->substate));
    }
    g_variant_iter_free(iter);
}

static void merge_unit_files(GVariant *files, ServiceEntry **entries,
                             size_t *count, size_t *capacity)
{
    GVariantIter *iter = NULL;
    g_variant_get(files, "(a(ss))", &iter);
    const char *path, *state;
    while (g_variant_iter_loop(iter, "(&s&s)", &path, &state)) {
        const char *unit = base_name(path);
        size_t length = strlen(unit);
        if (length < 8 || strcmp(unit + length - 8, ".service") != 0) continue;
        ServiceEntry *entry = service_get(entries, count, capacity, unit);
        if (!entry) break;
        g_strlcpy(entry->startup, state, sizeof(entry->startup));
    }
    g_variant_iter_free(iter);
}

static ServiceEntry *collect_services(GDBusConnection *bus,
                                      GCancellable *cancellable,
                                      size_t *out_count, GError **error)
{
    ServiceEntry *entries = NULL;
    size_t count = 0, capacity = 0;

    GVariant *units = manager_call_on_bus(bus, "ListUnits", NULL,
                                          LSM_DBUS_QUERY_TIMEOUT_MS,
                                          cancellable, error);
    if (!units) return NULL;
    merge_loaded_units(units, &entries, &count, &capacity);
    g_variant_unref(units);

    /* ListUnitFiles adds disabled and otherwise-unloaded services.  Older
     * systemd releases may not implement it; loaded services are still useful
     * in that case, so this secondary call is deliberately non-fatal. */
    GError *files_error = NULL;
    GVariant *files = manager_call_on_bus(bus, "ListUnitFiles", NULL,
                                          LSM_DBUS_QUERY_TIMEOUT_MS,
                                          cancellable, &files_error);
    if (files) {
        merge_unit_files(files, &entries, &count, &capacity);
        g_variant_unref(files);
    }
    if (files_error) g_error_free(files_error);

    if (count > 1) qsort(entries, count, sizeof(*entries), service_compare);
    *out_count = count;
    return entries;
}

static gboolean selected_service(LsmApp *app, char **name, char **active, char **startup)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->services.services_tree));
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) return FALSE;
    gtk_tree_model_get(model, &iter,
                       SERVICE_COL_NAME, name,
                       SERVICE_COL_ACTIVE, active,
                       SERVICE_COL_STARTUP, startup,
                       -1);
    return TRUE;
}

static gboolean state_is_enabled(const char *state)
{
    return state && (strncmp(state, "enabled", 7) == 0 ||
                     strncmp(state, "linked", 6) == 0 ||
                     strcmp(state, "alias") == 0);
}

static void service_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
    LsmApp *app = user_data;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    gboolean selected = gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_widget_set_sensitive(app->services.service_start_button, selected);
    gtk_widget_set_sensitive(app->services.service_stop_button, selected);
    gtk_widget_set_sensitive(app->services.service_restart_button, selected);
    gtk_widget_set_sensitive(app->services.service_enable_button, selected);
    if (!selected) return;

    char *active = NULL, *startup = NULL;
    gtk_tree_model_get(model, &iter,
                       SERVICE_COL_ACTIVE, &active,
                       SERVICE_COL_STARTUP, &startup,
                       -1);
    gboolean running = active && strcmp(active, "active") == 0;
    gtk_widget_set_sensitive(app->services.service_start_button, !running);
    gtk_widget_set_sensitive(app->services.service_stop_button, running);
    gtk_widget_set_sensitive(app->services.service_restart_button, running);
    gtk_button_set_label(GTK_BUTTON(app->services.service_enable_button),
                         state_is_enabled(startup) ? "Disable" : "Enable");
    g_free(active);
    g_free(startup);
}

static void service_action_result_free(gpointer data)
{
    ServiceActionResult *result = data;
    if (!result) return;
    g_free(result->method);
    if (result->parameters) g_variant_unref(result->parameters);
    g_free(result->failure_title);
    g_free(result->error_message);
    g_free(result);
}

/* Action pipeline: copy immutable request data, perform one D-Bus operation
 * in a worker, then update controls on the GTK main context. */
static void service_action_worker(GTask *task, gpointer source_object,
                                  gpointer task_data,
                                  GCancellable *cancellable)
{
    (void)source_object;
    ServiceActionResult *result = task_data;
    GError *error = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, cancellable, &error);
    if (bus) {
        GVariant *reply = manager_call_on_bus(
            bus, result->method, result->parameters,
            LSM_DBUS_ACTION_TIMEOUT_MS, cancellable, &error);
        if (reply) g_variant_unref(reply);
        if (!error && result->reload_after) {
            reply = manager_call_on_bus(bus, "Reload", NULL,
                                        LSM_DBUS_ACTION_TIMEOUT_MS,
                                        cancellable, &error);
            if (reply) g_variant_unref(reply);
        }
        g_object_unref(bus);
    }
    if (error) {
        result->error_message = g_strdup(error->message);
        g_error_free(error);
    }
    result->cancelled = g_cancellable_is_cancelled(cancellable);
    g_task_return_pointer(task, result, service_action_result_free);
}

static void service_action_complete(GObject *source_object,
                                    GAsyncResult *async_result,
                                    gpointer user_data)
{
    (void)source_object;
    LsmApp *app = user_data;
    ServiceActionResult *result = g_task_propagate_pointer(
        G_TASK(async_result), NULL);
    if (app->services.services_action_pending > 0) app->services.services_action_pending--;
    if (app->services.services_action_pending == 0 &&
        app->services.services_action_cancellable) {
        g_object_unref(app->services.services_action_cancellable);
        app->services.services_action_cancellable = NULL;
    }
    if (result && !result->cancelled && !app->runtime.shutting_down) {
        if (result->error_message)
            lsm_ui_show_error(GTK_WINDOW(app->shell.window), result->failure_title,
                              "%s", result->error_message);
        else
            lsm_services_refresh(app);
    }
    service_action_result_free(result);
}

static void perform_service_action(LsmApp *app, const char *method,
                                   GVariant *parameters,
                                   const char *failure_title,
                                   gboolean reload_after)
{
    if (!app || app->runtime.shutting_down) return;
    ServiceActionResult *result = g_new0(ServiceActionResult, 1);
    result->method = g_strdup(method);
    result->parameters = parameters ? g_variant_ref_sink(parameters) : NULL;
    result->failure_title = g_strdup(failure_title);
    result->reload_after = reload_after;
    if (!app->services.services_action_cancellable)
        app->services.services_action_cancellable = g_cancellable_new();
    app->services.services_action_pending++;
    GTask *task = g_task_new(NULL, app->services.services_action_cancellable,
                             service_action_complete, app);
    g_task_set_task_data(task, result, NULL);
    g_task_run_in_thread(task, service_action_worker);
    g_object_unref(task);
}

static void service_start(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *name = NULL, *active = NULL, *startup = NULL;
    if (selected_service(app, &name, &active, &startup))
        perform_service_action(app, "StartUnit",
                               g_variant_new("(ss)", name, "replace"),
                               "Unable to start service", FALSE);
    g_free(name); g_free(active); g_free(startup);
}

static void service_stop(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *name = NULL, *active = NULL, *startup = NULL;
    if (selected_service(app, &name, &active, &startup))
        perform_service_action(app, "StopUnit",
                               g_variant_new("(ss)", name, "replace"),
                               "Unable to stop service", FALSE);
    g_free(name); g_free(active); g_free(startup);
}

static void service_restart(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *name = NULL, *active = NULL, *startup = NULL;
    if (selected_service(app, &name, &active, &startup))
        perform_service_action(app, "RestartUnit",
                               g_variant_new("(ss)", name, "replace"),
                               "Unable to restart service", FALSE);
    g_free(name); g_free(active); g_free(startup);
}

static void service_enable_disable(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *name = NULL, *active = NULL, *startup = NULL;
    if (!selected_service(app, &name, &active, &startup)) return;

    const char *units[] = {name, NULL};
    const gboolean enable = !state_is_enabled(startup);
    GVariant *parameters = enable
        ? g_variant_new("(^asbb)", units, FALSE, TRUE)
        : g_variant_new("(^asb)", units, FALSE);
    perform_service_action(app,
                           enable ? "EnableUnitFiles" : "DisableUnitFiles",
                           parameters,
                           enable ? "Unable to enable service"
                                  : "Unable to disable service",
                           TRUE);
    g_free(name); g_free(active); g_free(startup);
}

static void service_refresh_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    lsm_services_refresh(user_data);
}

static gboolean service_search_timeout(gpointer user_data)
{
    LsmApp *app = user_data;
    app->services.services_search_timer = 0;
    lsm_services_refresh(app);
    return G_SOURCE_REMOVE;
}

static void service_search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    LsmApp *app = user_data;
    if (app->services.services_search_timer) g_source_remove(app->services.services_search_timer);
    app->services.services_search_timer = g_timeout_add(LSM_SEARCH_DEBOUNCE_MS,
                                                      service_search_timeout, app);
}


static void restore_service_selection(LsmApp *app, const char *name)
{
    if (!name || !*name) return;
    GtkTreeModel *model = GTK_TREE_MODEL(app->services.services_store);
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter_first(model, &iter)) return;
    do {
        char *candidate = NULL;
        gtk_tree_model_get(model, &iter, SERVICE_COL_NAME, &candidate, -1);
        gboolean match = candidate && strcmp(candidate, name) == 0;
        g_free(candidate);
        if (match) {
            GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->services.services_tree));
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(app->services.services_tree), path, NULL,
                                         FALSE, 0.0f, 0.0f);
            gtk_tree_path_free(path);
            return;
        }
    } while (gtk_tree_model_iter_next(model, &iter));
}

static GtkTreeViewColumn *service_column(GtkTreeView *tree, const char *title,
                                         int model_column, gboolean expand, int minimum)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(title, renderer,
                                                                         "text", model_column, NULL);
    gtk_tree_view_column_set_sort_column_id(column, model_column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, expand);
    gtk_tree_view_column_set_min_width(column, minimum);
    gtk_tree_view_append_column(tree, column);
    return column;
}

static void service_refresh_result_free(gpointer data)
{
    ServiceRefreshResult *result = data;
    if (!result) return;
    free(result->entries);
    g_free(result->error_message);
    g_free(result->preserve_name);
    g_free(result);
}

/* Inventory refresh merges loaded units with unit-file enablement state. */
static void service_refresh_worker(GTask *task, gpointer source_object,
                                   gpointer task_data,
                                   GCancellable *cancellable)
{
    (void)source_object;
    ServiceRefreshResult *result = task_data;
    GError *error = NULL;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, cancellable, &error);
    if (bus) {
        result->entries = collect_services(bus, cancellable, &result->count, &error);
        g_object_unref(bus);
    }
    if (error) {
        result->error_message = g_strdup(error->message);
        g_error_free(error);
    }
    result->cancelled = g_cancellable_is_cancelled(cancellable);
    g_task_return_pointer(task, result, service_refresh_result_free);
}

static void apply_service_refresh(LsmApp *app, ServiceRefreshResult *result)
{
    gtk_list_store_clear(app->services.services_store);
    if (!result->entries && result->error_message) {
        app->services.services_available = FALSE;
        GtkTreeIter iter;
        gtk_list_store_append(app->services.services_store, &iter);
        gtk_list_store_set(app->services.services_store, &iter,
                           SERVICE_COL_NAME, "systemd services unavailable",
                           SERVICE_COL_DESCRIPTION, result->error_message,
                           SERVICE_COL_STATUS, "Unavailable",
                           SERVICE_COL_SUBSTATE, "",
                           SERVICE_COL_STARTUP, "",
                           SERVICE_COL_ACTIVE, "",
                           -1);
        lsm_ui_set_label_text(app->services.service_count_label,
                              "Service manager unavailable");
        return;
    }

    app->services.services_available = TRUE;
    size_t visible = 0;
    const char *search = gtk_entry_get_text(GTK_ENTRY(app->services.services_search));
    for (size_t index = 0; index < result->count; index++) {
        ServiceEntry *entry = &result->entries[index];
        if (*search && !lsm_ui_text_matches(entry->name, search) &&
            !lsm_ui_text_matches(entry->description, search) &&
            !lsm_ui_text_matches(entry->active, search) &&
            !lsm_ui_text_matches(entry->startup, search)) continue;
        GtkTreeIter iter;
        gtk_list_store_append(app->services.services_store, &iter);
        gtk_list_store_set(app->services.services_store, &iter,
                           SERVICE_COL_NAME, entry->name,
                           SERVICE_COL_DESCRIPTION, entry->description,
                           SERVICE_COL_STATUS, entry->active,
                           SERVICE_COL_SUBSTATE, entry->substate,
                           SERVICE_COL_STARTUP, entry->startup,
                           SERVICE_COL_ACTIVE, entry->active,
                           -1);
        visible++;
    }
    lsm_ui_set_label_text(app->services.service_count_label, "%zu service%s",
                          visible, visible == 1 ? "" : "s");
    gtk_widget_set_sensitive(app->services.service_start_button, FALSE);
    gtk_widget_set_sensitive(app->services.service_stop_button, FALSE);
    gtk_widget_set_sensitive(app->services.service_restart_button, FALSE);
    gtk_widget_set_sensitive(app->services.service_enable_button, FALSE);
    restore_service_selection(app, result->preserve_name);
}

static void service_refresh_complete(GObject *source_object,
                                     GAsyncResult *async_result,
                                     gpointer user_data)
{
    (void)source_object;
    LsmApp *app = user_data;
    ServiceRefreshResult *result = g_task_propagate_pointer(
        G_TASK(async_result), NULL);
    app->services.services_refresh_pending = FALSE;
    if (app->services.services_refresh_cancellable) {
        g_object_unref(app->services.services_refresh_cancellable);
        app->services.services_refresh_cancellable = NULL;
    }
    if (result && !result->cancelled && !app->runtime.shutting_down &&
        app->services.services_store)
        apply_service_refresh(app, result);
    service_refresh_result_free(result);
}

/* Public lifecycle and cadence control. */
void lsm_services_refresh(LsmApp *app)
{
    if (!app || app->runtime.shutting_down || !app->services.services_store ||
        app->services.services_refresh_pending) return;
    ServiceRefreshResult *result = g_new0(ServiceRefreshResult, 1);
    char *active = NULL, *startup = NULL;
    selected_service(app, &result->preserve_name, &active, &startup);
    g_free(active);
    g_free(startup);

    app->services.services_refresh_pending = TRUE;
    app->services.services_refresh_cancellable = g_cancellable_new();
    lsm_ui_set_label_text(app->services.service_count_label, "Refreshing services…");
    GTask *task = g_task_new(NULL, app->services.services_refresh_cancellable,
                             service_refresh_complete, app);
    g_task_set_task_data(task, result, NULL);
    g_task_run_in_thread(task, service_refresh_worker);
    g_object_unref(task);
}

gboolean lsm_services_update(gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app || app->runtime.paused) return G_SOURCE_CONTINUE;
    if (!app->shell.notebook || gtk_notebook_get_current_page(GTK_NOTEBOOK(app->shell.notebook)) == LSM_TAB_SERVICES)
        lsm_services_refresh(app);
    return G_SOURCE_CONTINUE;
}

void lsm_services_build(LsmApp *app, GtkWidget *container)
{
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 8);
    app->services.services_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->services.services_search), "Search services");
    gtk_widget_set_hexpand(app->services.services_search, TRUE);
    g_signal_connect(app->services.services_search, "changed", G_CALLBACK(service_search_changed), app);

    app->services.service_start_button = gtk_button_new_with_label("Start");
    app->services.service_stop_button = gtk_button_new_with_label("Stop");
    app->services.service_restart_button = gtk_button_new_with_label("Restart");
    app->services.service_enable_button = gtk_button_new_with_label("Enable");
    GtkWidget *refresh = gtk_button_new_with_label("Refresh");
    GtkWidget *buttons[] = {app->services.service_start_button, app->services.service_stop_button,
                            app->services.service_restart_button, app->services.service_enable_button};
    for (size_t i = 0; i < G_N_ELEMENTS(buttons); i++) gtk_widget_set_sensitive(buttons[i], FALSE);
    g_signal_connect(app->services.service_start_button, "clicked", G_CALLBACK(service_start), app);
    g_signal_connect(app->services.service_stop_button, "clicked", G_CALLBACK(service_stop), app);
    g_signal_connect(app->services.service_restart_button, "clicked", G_CALLBACK(service_restart), app);
    g_signal_connect(app->services.service_enable_button, "clicked", G_CALLBACK(service_enable_disable), app);
    g_signal_connect(refresh, "clicked", G_CALLBACK(service_refresh_clicked), app);

    gtk_box_pack_start(GTK_BOX(toolbar), app->services.services_search, TRUE, TRUE, 0);
    for (size_t i = 0; i < G_N_ELEMENTS(buttons); i++)
        gtk_box_pack_start(GTK_BOX(toolbar), buttons[i], FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), refresh, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), toolbar, FALSE, FALSE, 0);

    app->services.services_store = gtk_list_store_new(SERVICE_N_COLUMNS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    app->services.services_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->services.services_store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(app->services.services_tree), TRUE);
    service_column(GTK_TREE_VIEW(app->services.services_tree), "Service", SERVICE_COL_NAME, FALSE, 210);
    service_column(GTK_TREE_VIEW(app->services.services_tree), "Description", SERVICE_COL_DESCRIPTION, TRUE, 280);
    service_column(GTK_TREE_VIEW(app->services.services_tree), "Status", SERVICE_COL_STATUS, FALSE, 85);
    service_column(GTK_TREE_VIEW(app->services.services_tree), "Substate", SERVICE_COL_SUBSTATE, FALSE, 95);
    service_column(GTK_TREE_VIEW(app->services.services_tree), "Startup", SERVICE_COL_STARTUP, FALSE, 105);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->services.services_tree));
    g_signal_connect(selection, "changed", G_CALLBACK(service_selection_changed), app);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    app->runtime.page_scrollers[LSM_TAB_SERVICES] = scroll;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), app->services.services_tree);
    gtk_box_pack_start(GTK_BOX(container), scroll, TRUE, TRUE, 0);

    app->services.service_count_label = gtk_label_new("");
    gtk_widget_set_halign(app->services.service_count_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->services.service_count_label, 8);
    gtk_widget_set_margin_bottom(app->services.service_count_label, 6);
    gtk_box_pack_start(GTK_BOX(container), app->services.service_count_label, FALSE, FALSE, 0);
    lsm_ui_set_label_text(app->services.service_count_label, "Select the Services tab to load services");
}

void lsm_services_destroy(LsmApp *app)
{
    if (!app) return;
    if (app->services.services_refresh_cancellable)
        g_cancellable_cancel(app->services.services_refresh_cancellable);
    if (app->services.services_action_cancellable)
        g_cancellable_cancel(app->services.services_action_cancellable);
    while (app->services.services_refresh_pending || app->services.services_action_pending > 0)
        (void)g_main_context_iteration(NULL, TRUE);
    if (app->services.services_refresh_cancellable) {
        g_object_unref(app->services.services_refresh_cancellable);
        app->services.services_refresh_cancellable = NULL;
    }
    if (app->services.services_action_cancellable) {
        g_object_unref(app->services.services_action_cancellable);
        app->services.services_action_cancellable = NULL;
    }
    if (app->services.services_store) g_object_unref(app->services.services_store);
    app->services.services_store = NULL;
}
