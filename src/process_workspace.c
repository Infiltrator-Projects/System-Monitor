// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_workspace.c
 * @brief Shared process sampling and cross-page process coordination.
 *
 * This is the single application-domain owner for process snapshot lifecycle.
 * Processes and Details remain presentation modules: neither page reaches into
 * the other's widgets or owns sampling cadence, recording ingestion or the
 * transition from a friendly process selection to the technical Details view.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_workspace.h"

#include "app_internal.h"
#include "details_page.h"
#include "history.h"
#include "monitor.h"
#include "overview.h"
#include "process_backend.h"
#include "process_scanner.h"
#include "processes_ui.h"

static unsigned process_scan_flags(const LsmApp *app)
{
    if (!app) return LSM_PROCESS_SCAN_EXECUTABLE;

    unsigned flags = LSM_PROCESS_SCAN_EXECUTABLE;
    switch ((LsmTabIndex)app->runtime.active_tab) {
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
    if (!app || !app->process.recorder ||
        app->process.recording_pid <= 1 ||
        app->process.recording_instance_id == 0U)
        return;

    const LsmProcessInfo *found = NULL;
    for (size_t index = 0U; index < count; index++) {
        if (processes[index].pid == app->process.recording_pid &&
            processes[index].instance_id ==
                app->process.recording_instance_id) {
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
    if (!app || app->runtime.paused || !app->process_scanner)
        return FALSE;

    LsmProcessInfo *processes = NULL;
    size_t count = 0U;
    if (!lsm_process_scanner_take(app->process_scanner, &processes, &count))
        return FALSE;

    lsm_monitor_set_process_totals(&app->monitor, processes, count);
    append_record_if_needed(app, processes, count);
    lsm_app_history_ingest(app, processes, count);

    lsm_process_list_free(app->process.process_snapshot);
    app->process.process_snapshot = processes;
    app->process.process_snapshot_count = count;
    app->process.process_snapshot_generation++;
    if (app->process.process_snapshot_generation == 0U)
        app->process.process_snapshot_generation = 1U;

    app->processes.processes_model_dirty = TRUE;
    app->details.details_model_dirty = TRUE;
    lsm_overview_refresh(app);

    if (app->runtime.active_tab == LSM_TAB_PROCESSES &&
        app->runtime.page_built[LSM_TAB_PROCESSES])
        lsm_processes_present_snapshot(app);
    if (app->runtime.active_tab == LSM_TAB_DETAILS &&
        app->runtime.page_built[LSM_TAB_DETAILS])
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
    if (!app || app->runtime.shutting_down)
        return G_SOURCE_REMOVE;
    if (app->runtime.paused || !app->process_scanner)
        return G_SOURCE_CONTINUE;

    (void)consume_completed_process_snapshot(app);
    (void)lsm_process_scanner_request(
        app->process_scanner, process_scan_flags(app));
    return G_SOURCE_CONTINUE;
}

void lsm_process_workspace_go_to_details(LsmApp *app)
{
    if (!app || app->process.selected_pid <= 0 || !app->shell.notebook)
        return;

    lsm_app_ensure_page_built(app, LSM_TAB_DETAILS);
    if (!app->details.details_search) return;

    gtk_entry_set_text(GTK_ENTRY(app->details.details_search), "");
    app->details.details_structure_valid = FALSE;
    app->details.details_model_dirty = TRUE;
    gtk_notebook_set_current_page(
        GTK_NOTEBOOK(app->shell.notebook), LSM_TAB_DETAILS);
    lsm_details_present_snapshot(app);
}

/* Established application-facing names remain stable while the implementation
 * ownership lives here rather than in either presentation module. */
gboolean lsm_processes_update(gpointer user_data)
{
    return lsm_process_workspace_update(user_data);
}

void lsm_processes_present_ready_snapshot(LsmApp *app)
{
    lsm_process_workspace_present_ready_snapshot(app);
}

void lsm_processes_go_to_details(LsmApp *app)
{
    lsm_process_workspace_go_to_details(app);
}

gboolean lsm_process_heatmap_enabled(const LsmApp *app)
{
    return app && app->details.process_heatmap;
}

void lsm_process_record_action_sync(LsmApp *app,
                                    gboolean ordinary_selection,
                                    gboolean grouped_selection)
{
    if (!app || !app->details.process_record_menu_item) return;
    const gboolean sensitive =
        (ordinary_selection && !grouped_selection) ||
        app->process.recorder != NULL;
    gtk_widget_set_sensitive(app->details.process_record_menu_item, sensitive);
}
