// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_workspace.h
 * @brief Shared process sampling, selection, actions, filtering and recording.
 *
 * The process workspace is application-domain state shared by the friendly
 * Processes page and the technical Details page. Keeping these operations in a
 * neutral interface prevents either presentation module from becoming the
 * owner of process lifecycle, cross-page navigation or selection semantics.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_WORKSPACE_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_WORKSPACE_H

#include "monitor_types.h"

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Sample processes and conditionally rebuild visible process models.
 *
 * @param [in,out] user_data Pointer to the owning LsmApp.
 * @return G_SOURCE_CONTINUE while sampling remains scheduled, otherwise
 *         G_SOURCE_REMOVE during shutdown.
 */
gboolean lsm_process_workspace_update(gpointer user_data);

/**
 * Consume and present a completed background process snapshot immediately.
 *
 * @param [in,out] app Application receiving the completed snapshot.
 */
void lsm_process_workspace_present_ready_snapshot(LsmApp *app);

/**
 * Move the current process selection to the matching Details-page row.
 *
 * @param [in,out] app Application containing the shared process selection.
 */
void lsm_process_workspace_go_to_details(LsmApp *app);

#ifndef LSM_PROCESS_WORKSPACE_NO_ALIASES
#define lsm_processes_update lsm_process_workspace_update
#define lsm_processes_present_ready_snapshot \
    lsm_process_workspace_present_ready_snapshot
#define lsm_processes_go_to_details lsm_process_workspace_go_to_details
#endif

/**
 * Build the shared process-action context menu.
 *
 * @param [in,out] app Application containing the current process selection.
 * @param [in] include_columns TRUE on Details, FALSE on Processes.
 * @return Newly constructed GTK menu.
 */
GtkWidget *lsm_process_actions_menu(LsmApp *app, gboolean include_columns);

/**
 * Retain a selected PID together with its current process-instance token.
 *
 * @param [in,out] app Application whose selection is updated.
 * @param [in] pid Process identifier from a visible row, or zero.
 */
void lsm_process_selection_set(LsmApp *app, guint64 pid);

/**
 * Clear the retained application-group selection.
 *
 * @param [in,out] app Application whose grouped selection is cleared.
 */
void lsm_process_group_selection_clear(LsmApp *app);

/**
 * Start or stop recording the selected process.
 *
 * @param [in,out] app Application containing recording state.
 * @param [in] active TRUE to start recording, FALSE to stop.
 */
void lsm_process_record_set(LsmApp *app, gboolean active);

/**
 * Append one current sample to the active process recording.
 *
 * @param [in,out] app Application containing recording state.
 * @param [in] process Current sample matching the recorded process instance.
 * @return TRUE when the sample was queued, otherwise FALSE.
 */
gboolean lsm_process_record_append(LsmApp *app,
                                   const LsmProcessInfo *process);

/**
 * Open the technical inspector for the current process selection.
 *
 * @param [in,out] app Application containing the current selection.
 */
void lsm_processes_show_selected_details(LsmApp *app);

/**
 * Ask for confirmation and end the currently selected process or group.
 *
 * @param [in,out] app Application containing the current selection.
 */
void lsm_processes_end_selected(LsmApp *app);

/**
 * Load persisted process-filter rules.
 *
 * @param [in,out] app Application receiving the filter set.
 */
void lsm_process_filters_load(LsmApp *app);

/**
 * Present the graphical process-filter editor.
 *
 * @param [in,out] app Application whose filter rules may change.
 */
void lsm_process_filters_dialog(LsmApp *app);

/**
 * Stop process CSV recording and release the detached writer.
 *
 * @param [in,out] app Application containing recording state.
 */
void lsm_process_record_stop(LsmApp *app);

/**
 * Read the shared process heatmap preference without exposing Details state.
 *
 * @param [in] app Application containing process presentation preferences.
 * @return TRUE when process heatmaps are enabled, otherwise FALSE.
 */
gboolean lsm_process_heatmap_enabled(const LsmApp *app);

/**
 * Synchronise the global recording action with a presentation selection.
 *
 * @param [in,out] app Application containing recording state.
 * @param [in] ordinary_selection TRUE when an ordinary process is selected.
 * @param [in] grouped_selection TRUE when the selection is a process group.
 */
void lsm_process_record_action_sync(LsmApp *app,
                                    gboolean ordinary_selection,
                                    gboolean grouped_selection);

#endif
