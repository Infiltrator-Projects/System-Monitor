// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_workspace.c
 * @brief Shared process sampling and cross-page snapshot coordination.
 *
 * The workspace owns process collection cadence and completed-snapshot
 * publication. It deliberately depends on a narrow application context instead
 * of the complete private LsmApp layout; GTK actions and cross-page navigation
 * are separate presentation concerns.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_workspace.h"

#include "app_process_context.h"
#include "details_page.h"
#include "history.h"
#include "overview.h"
#include "process_actions.h"
#include "process_scanner.h"
#include "processes_ui.h"

static unsigned process_scan_flags(const LsmApp *app)
{
    LsmProcessRuntimeView runtime;
    if (!lsm_app_process_runtime_view(app, &runtime))
        return LSM_PROCESS_SCAN_EXECUTABLE;

    unsigned flags = LSM_PROCESS_SCAN_EXECUTABLE;
    switch (runtime.active_tab) {
        case LSM_TAB_PROCESSES:
            flags |= LSM_PROCESS_SCAN_GPU | LSM_PROCESS_SCAN_CGROUP;
            break;
        case LSM_TAB_DETAILS:
            flags |= lsm_details_process_scan_flags(app);
            break;
        case LSM_TAB_PERFORMANCE:
        case LSM_TAB_APP_HISTORY:
        case LSM_TAB_STARTUP:
        case LSM_TAB_USERS:
        case LSM_TAB_SERVICES:
        case LSM_TAB_FILESYSTEMS:
        case LSM_TAB_OVERVIEW:
        case LSM_TAB_COUNT:
            break;
    }
    return flags;
}

static void append_record_if_needed(LsmApp *app,
                                    const LsmProcessInfo *processes,
                                    size_t count)
{
    LsmProcessId recording_pid = 0U;
    LsmProcessInstanceId recording_instance_id = 0U;
    if (!lsm_app_process_recording_target(
            app, &recording_pid, &recording_instance_id))
        return;

    const LsmProcessInfo *found = NULL;
    for (size_t index = 0U; index < count; index++) {
        if (processes[index].pid == recording_pid &&
            processes[index].instance_id == recording_instance_id) {
            found = &processes[index];
            break;
        }
    }
    if (!found) {
        lsm_process_record_stop(app);
        return;
    }
    (void)lsm_process_record_append(app, found);
}

static gboolean consume_completed_process_snapshot(LsmApp *app)
{
    LsmProcessRuntimeView runtime;
    if (!lsm_app_process_runtime_view(app, &runtime) || runtime.paused)
        return FALSE;

    LsmProcessScanner *scanner = lsm_app_process_scanner(app);
    if (!scanner) return FALSE;

    LsmProcessInfo *processes = NULL;
    size_t count = 0U;
    if (!lsm_process_scanner_take(scanner, &processes, &count))
        return FALSE;

    append_record_if_needed(app, processes, count);
    lsm_app_history_ingest(app, processes, count);
    lsm_app_process_publish_snapshot(app, processes, count);
    lsm_overview_refresh(app);

    if (runtime.active_tab == LSM_TAB_PROCESSES && runtime.processes_built)
        lsm_processes_present_snapshot(app);
    if (runtime.active_tab == LSM_TAB_DETAILS && runtime.details_built)
        lsm_details_present_snapshot(app);
    return TRUE;
}

void lsm_process_workspace_present_ready_snapshot(LsmApp *app)
{
    (void)consume_completed_process_snapshot(app);
}

gboolean lsm_process_workspace_update(gpointer user_data)
{
    LsmApp *app = user_data;
    LsmProcessRuntimeView runtime;
    if (!lsm_app_process_runtime_view(app, &runtime) || runtime.shutting_down)
        return G_SOURCE_REMOVE;
    if (runtime.paused)
        return G_SOURCE_CONTINUE;

    LsmProcessScanner *scanner = lsm_app_process_scanner(app);
    if (!scanner) return G_SOURCE_CONTINUE;

    (void)consume_completed_process_snapshot(app);
    (void)lsm_process_scanner_request(scanner, process_scan_flags(app));
    return G_SOURCE_CONTINUE;
}

gboolean lsm_processes_update(gpointer user_data)
{
    return lsm_process_workspace_update(user_data);
}

void lsm_processes_present_ready_snapshot(LsmApp *app)
{
    lsm_process_workspace_present_ready_snapshot(app);
}
