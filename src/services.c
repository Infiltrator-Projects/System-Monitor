// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file services.c
 * @brief GTK presentation and asynchronous orchestration for Services.
 *
 * Native service-manager discovery and control are delegated through
 * service_backend.h. This module owns only selection, filtering, presentation
 * and worker/main-context handoff.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "services.h"
#include "app_internal.h"
#include "app_config.h"
#include "service_backend.h"
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
    LsmServiceEntry *entries;
    size_t count;
    char *error_message;
    char *preserve_name;
    gboolean cancelled;
} ServiceRefreshResult;

typedef struct {
    char *name;
    LsmServiceAction action;
    char *failure_title;
    char *error_message;
    gboolean cancelled;
} ServiceActionResult;

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
                         lsm_service_backend_state_is_enabled(startup) ? "Disable" : "Enable");
    g_free(active);
    g_free(startup);
}

static void service_action_result_free(gpointer data)
{
    ServiceActionResult *result = data;
    if (!result) return;
    g_free(result->name);
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
    (void)lsm_service_backend_action(
        result->name, result->action, cancellable, &error);
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

static void perform_service_action(LsmApp *app, const char *name,
                                   LsmServiceAction action,
                                   const char *failure_title)
{
    if (!app || !name || !name[0] || app->runtime.shutting_down) return;
    ServiceActionResult *result = g_new0(ServiceActionResult, 1);
    result->name = g_strdup(name);
    result->action = action;
    result->failure_title = g_strdup(failure_title);
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
        perform_service_action(app, name, LSM_SERVICE_ACTION_START,
                               "Unable to start service");
    g_free(name);
    g_free(active);
    g_free(startup);
}

static void service_stop(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *name = NULL, *active = NULL, *startup = NULL;
    if (selected_service(app, &name, &active, &startup))
        perform_service_action(app, name, LSM_SERVICE_ACTION_STOP,
                               "Unable to stop service");
    g_free(name);
    g_free(active);
    g_free(startup);
}

static void service_restart(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *name = NULL, *active = NULL, *startup = NULL;
    if (selected_service(app, &name, &active, &startup))
        perform_service_action(app, name, LSM_SERVICE_ACTION_RESTART,
                               "Unable to restart service");
    g_free(name);
    g_free(active);
    g_free(startup);
}

static void service_enable_disable(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *name = NULL, *active = NULL, *startup = NULL;
    if (!selected_service(app, &name, &active, &startup)) return;

    const gboolean enable =
        !lsm_service_backend_state_is_enabled(startup);
    perform_service_action(
        app, name,
        enable ? LSM_SERVICE_ACTION_ENABLE : LSM_SERVICE_ACTION_DISABLE,
        enable ? "Unable to enable service" : "Unable to disable service");
    g_free(name);
    g_free(active);
    g_free(startup);
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
    lsm_service_backend_free(result->entries);
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
    (void)lsm_service_backend_collect(
        &result->entries, &result->count, cancellable, &error);
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
        LsmServiceEntry *entry = &result->entries[index];
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
