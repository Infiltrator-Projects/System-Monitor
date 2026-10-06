// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_process_context.c
 * @brief Private LsmApp accessors used by the shared process workspace.
 *
 * This file is deliberately the only bridge between process-domain
 * coordination and the complete private application layout. The process
 * workspace therefore cannot reach unrelated shell, Details or Processes
 * widget state directly.
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
    return true;
}

LsmProcessScanner *lsm_app_process_scanner(LsmApp *app)
{
    return app ? app->process_scanner : NULL;
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

    app->processes.processes_model_dirty = TRUE;
    app->details.details_model_dirty = TRUE;
}
