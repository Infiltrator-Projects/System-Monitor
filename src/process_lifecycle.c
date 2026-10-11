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

#include "app_internal.h"
#include "application_catalog.h"
#include "process_actions.h"
#include "process_backend.h"
#include "process_scanner.h"
#include "processes_ui.h"

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
    if (!app || app->runtime.shutting_down) {
        lsm_application_catalog_destroy(catalog);
        return;
    }

    app->runtime.application_catalog_loading = FALSE;
    if (!catalog) return;
    lsm_application_catalog_destroy(app->process.application_catalog);
    app->process.application_catalog = catalog;
    if (app->processes.process_identity_cache)
        g_hash_table_remove_all(app->processes.process_identity_cache);
    app->processes.processes_structure_valid = FALSE;
    app->processes.processes_model_dirty = TRUE;
    if (app->runtime.active_tab == LSM_TAB_PROCESSES &&
        app->runtime.page_built[LSM_TAB_PROCESSES])
        lsm_processes_present_snapshot(app);
}

static void start_application_catalog_load(LsmApp *app)
{
    if (!app || !app->shell.window || app->runtime.shutting_down ||
        app->runtime.application_catalog_loading ||
        app->process.application_catalog)
        return;

    app->runtime.application_catalog_loading = TRUE;
    GTask *task = g_task_new(G_OBJECT(app->shell.window), NULL,
                             application_catalog_complete, NULL);
    g_task_run_in_thread(task, application_catalog_worker);
    g_object_unref(task);
}

static void process_snapshot_ready(void *user_data)
{
    LsmApp *app = user_data;
    if (!app || app->runtime.shutting_down || app->runtime.paused)
        return;
    lsm_processes_present_ready_snapshot(app);
}

bool lsm_process_lifecycle_init(LsmApp *app)
{
    if (!app || app->process_scanner) return app && app->process_scanner;

    app->process_scanner = lsm_process_scanner_create();
    if (!app->process_scanner) return false;
    lsm_process_filters_load(app);
    return true;
}

void lsm_process_lifecycle_start(LsmApp *app)
{
    if (!app || !app->process_scanner || app->runtime.shutting_down) return;
    lsm_process_scanner_set_ready_callback(
        app->process_scanner, process_snapshot_ready, app);
    start_application_catalog_load(app);
}

void lsm_process_lifecycle_destroy(LsmApp *app)
{
    if (!app) return;

    lsm_process_record_stop(app);
    lsm_process_scanner_destroy(app->process_scanner);
    app->process_scanner = NULL;

    lsm_process_list_free(app->process.process_snapshot);
    app->process.process_snapshot = NULL;
    app->process.process_snapshot_count = 0U;

    lsm_application_catalog_destroy(app->process.application_catalog);
    app->process.application_catalog = NULL;
    app->runtime.application_catalog_loading = FALSE;

    lsm_process_group_selection_clear(app);
    if (app->process.filters) {
        g_ptr_array_free(app->process.filters, TRUE);
        app->process.filters = NULL;
    }
}
