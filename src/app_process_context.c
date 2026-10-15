// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_process_context.c
 * @brief Private LsmApp accessors used by the shared process domain.
 *
 * This file is deliberately the bridge between process-domain coordination and
 * the complete private application layout. Sampling, lifecycle and export code
 * consume narrow views instead of reaching through unrelated application state.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_process_context.h"

#include "app_internal.h"
#include "monitor.h"
#include "process_backend.h"

bool lsm_app_process_runtime_view(const LsmApp *app,
                                  LsmProcessRuntimeView *view)
{
    if (!app || !view) return false;
    view->active_tab = app->runtime.active_tab >= 0 &&
                       app->runtime.active_tab < LSM_TAB_COUNT
        ? (LsmTabIndex)app->runtime.active_tab : LSM_TAB_COUNT;
    view->paused = app->runtime.paused;
    view->shutting_down = app->runtime.shutting_down;
    view->processes_built = app->runtime.page_built[LSM_TAB_PROCESSES];
    view->details_built = app->runtime.page_built[LSM_TAB_DETAILS];
    view->application_catalog_loading = app->runtime.application_catalog_loading;
    view->application_catalog_ready = app->process.application_catalog != NULL;
    return true;
}

bool lsm_app_process_selection_view(const LsmApp *app,
                                    LsmProcessSelectionView *view)
{
    if (!app || !view) return false;
    view->processes = app->process.process_snapshot;
    view->process_count = app->process.process_snapshot_count;
    view->selected_pid = app->process.selected_pid;
    view->selected_instance_id = app->process.selected_instance_id;
    view->selected_group_pids = app->process.selected_group_pids;
    view->selected_group_instance_ids = app->process.selected_group_instance_ids;
    view->selected_group_count = app->process.selected_group_count;
    return true;
}

double lsm_app_process_displayed_cpu_percent(const LsmApp *app,
                                             double total_percent)
{
    if (!app || !app->runtime.process_cpu_per_core) return total_percent;
    const unsigned cores = app->monitor.cpu.logical_cores
        ? app->monitor.cpu.logical_cores : 1U;
    return total_percent * (double)cores;
}

gboolean lsm_app_process_history_newer_on_right(const LsmApp *app)
{
    return app && app->runtime.newer_on_right;
}

gboolean lsm_app_process_actions_require_confirmation(const LsmApp *app)
{
    return !app || app->runtime.confirm_process_actions;
}

gboolean lsm_app_process_heatmap_enabled(const LsmApp *app)
{
    return app && app->details.process_heatmap;
}

void lsm_app_process_ui_snapshot_changed(LsmApp *app)
{
    if (!app) return;
    app->processes.processes_model_dirty = TRUE;
    app->details.details_model_dirty = TRUE;
}

bool lsm_app_process_ui_catalog_changed(LsmApp *app)
{
    if (!app) return false;
    if (app->processes.process_identity_cache)
        g_hash_table_remove_all(app->processes.process_identity_cache);
    app->processes.processes_structure_valid = FALSE;
    app->processes.processes_model_dirty = TRUE;
    return app->runtime.active_tab == LSM_TAB_PROCESSES &&
           app->runtime.page_built[LSM_TAB_PROCESSES];
}

gboolean lsm_app_process_record_action_sensitive(
    const LsmApp *app, gboolean ordinary_selection,
    gboolean grouped_selection)
{
    if (!app) return FALSE;
    return (ordinary_selection && !grouped_selection) ||
           app->process.recorder != NULL;
}

LsmProcessScanner *lsm_app_process_scanner(LsmApp *app)
{
    return app ? app->process_scanner : NULL;
}

void lsm_app_process_set_scanner(LsmApp *app, LsmProcessScanner *scanner)
{
    if (!app) return;
    app->process_scanner = scanner;
}

void lsm_app_process_set_catalog_loading(LsmApp *app, gboolean loading)
{
    if (!app) return;
    app->runtime.application_catalog_loading = loading;
}

LsmApplicationCatalog *lsm_app_process_replace_catalog(
    LsmApp *app, LsmApplicationCatalog *catalog)
{
    if (!app) return catalog;
    LsmApplicationCatalog *previous = app->process.application_catalog;
    app->process.application_catalog = catalog;
    return previous;
}

LsmProcessInfo *lsm_app_process_take_snapshot(LsmApp *app, size_t *count)
{
    if (count) *count = 0U;
    if (!app) return NULL;
    LsmProcessInfo *processes = app->process.process_snapshot;
    if (count) *count = app->process.process_snapshot_count;
    app->process.process_snapshot = NULL;
    app->process.process_snapshot_count = 0U;
    return processes;
}

GPtrArray *lsm_app_process_take_filters(LsmApp *app)
{
    if (!app) return NULL;
    GPtrArray *filters = app->process.filters;
    app->process.filters = NULL;
    return filters;
}

bool lsm_app_process_recording_target(const LsmApp *app,
                                      LsmProcessId *pid,
                                      LsmProcessInstanceId *instance_id)
{
    if (pid) *pid = 0U;
    if (instance_id) *instance_id = 0U;
    if (!app || !pid || !instance_id || !app->process.recorder ||
        app->process.recording_pid <= 1U ||
        app->process.recording_instance_id == 0U)
        return false;

    *pid = app->process.recording_pid;
    *instance_id = app->process.recording_instance_id;
    return true;
}

void lsm_app_process_publish_snapshot(LsmApp *app,
                                      LsmProcessInfo *processes,
                                      size_t count)
{
    if (!app) {
        lsm_process_list_free(processes);
        return;
    }

    lsm_monitor_set_process_totals(&app->monitor, processes, count);
    lsm_process_list_free(app->process.process_snapshot);
    app->process.process_snapshot = processes;
    app->process.process_snapshot_count = count;
    app->process.process_snapshot_generation++;
    if (app->process.process_snapshot_generation == 0U)
        app->process.process_snapshot_generation = 1U;
}
