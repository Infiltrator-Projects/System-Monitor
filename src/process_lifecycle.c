// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_lifecycle.c
 * @brief Process-domain construction, background startup and teardown.
 *
 * Process resources belong to one feature boundary instead of requiring the
 * application composition root to know scanner, catalogue, snapshot, selection
 * and filter storage details. The main window is used only as the existing
 * GTask lifetime anchor for asynchronous catalogue discovery.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_lifecycle.h"

#include "app_presentation_context.h"
#include "app_process_context.h"
#include "application_catalog.h"
#include "process_actions.h"
#include "process_backend.h"
#include "process_scanner.h"
#include "process_ui_bridge.h"
#include "process_workspace.h"

static void application_catalog_result_free(gpointer data)
{
    lsm_application_catalog_destroy(data);
}

static void application_catalog_worker(GTask *task, gpointer source_object,
                                       gpointer task_data,
                                       GCancellable *cancellable)
{
    (void)source_object;
    (void)task_data;
    (void)cancellable;
    LsmApplicationCatalog *catalog = lsm_application_catalog_create();
    g_task_return_pointer(task, catalog, application_catalog_result_free);
}

static void application_catalog_complete(GObject *source_object,
                                         GAsyncResult *async_result,
                                         gpointer user_data)
{
    (void)user_data;
    LsmApplicationCatalog *catalog =
        g_task_propagate_pointer(G_TASK(async_result), NULL);
    LsmApp *app = source_object
        ? g_object_get_data(source_object, "lsm-app") : NULL;
    LsmProcessRuntimeView runtime;
    if (!app || !lsm_app_process_runtime_view(app, &runtime) ||
        runtime.shutting_down) {
        lsm_application_catalog_destroy(catalog);
        return;
    }

    lsm_app_process_set_catalog_loading(app, FALSE);
    if (!catalog) return;
    LsmApplicationCatalog *previous =
        lsm_app_process_replace_catalog(app, catalog);
    lsm_application_catalog_destroy(previous);
    lsm_process_ui_application_catalog_changed(app);
}

static void start_application_catalog_load(LsmApp *app)
{
    LsmProcessRuntimeView runtime;
    GtkWidget *window = lsm_app_presentation_window(app);
    if (!app || !window || !lsm_app_process_runtime_view(app, &runtime) ||
        runtime.shutting_down || runtime.application_catalog_loading ||
        runtime.application_catalog_ready)
        return;

    lsm_app_process_set_catalog_loading(app, TRUE);
    GTask *task = g_task_new(G_OBJECT(window), NULL,
                             application_catalog_complete, NULL);
    g_task_run_in_thread(task, application_catalog_worker);
    g_object_unref(task);
}

static void process_snapshot_ready(void *user_data)
{
    LsmApp *app = user_data;
    LsmProcessRuntimeView runtime;
    if (!lsm_app_process_runtime_view(app, &runtime) ||
        runtime.shutting_down || runtime.paused)
        return;
    lsm_process_workspace_present_ready_snapshot(app);
}

bool lsm_process_lifecycle_init(LsmApp *app)
{
    if (!app) return false;
    LsmProcessScanner *scanner = lsm_app_process_scanner(app);
    if (scanner) return true;

    scanner = lsm_process_scanner_create();
    if (!scanner) return false;
    lsm_app_process_set_scanner(app, scanner);
    lsm_process_filters_load(app);
    return true;
}

void lsm_process_lifecycle_start(LsmApp *app)
{
    LsmProcessRuntimeView runtime;
    LsmProcessScanner *scanner = lsm_app_process_scanner(app);
    if (!scanner || !lsm_app_process_runtime_view(app, &runtime) ||
        runtime.shutting_down)
        return;
    lsm_process_scanner_set_ready_callback(
        scanner, process_snapshot_ready, app);
    start_application_catalog_load(app);
}

void lsm_process_lifecycle_destroy(LsmApp *app)
{
    if (!app) return;

    lsm_process_record_stop(app);
    LsmProcessScanner *scanner = lsm_app_process_scanner(app);
    lsm_process_scanner_destroy(scanner);
    lsm_app_process_set_scanner(app, NULL);

    LsmProcessInfo *snapshot = lsm_app_process_take_snapshot(app, NULL);
    lsm_process_list_free(snapshot);

    LsmApplicationCatalog *catalog =
        lsm_app_process_replace_catalog(app, NULL);
    lsm_application_catalog_destroy(catalog);
    lsm_app_process_set_catalog_loading(app, FALSE);

    lsm_process_group_selection_clear(app);
    GPtrArray *filters = lsm_app_process_take_filters(app);
    if (filters) g_ptr_array_free(filters, TRUE);
}
