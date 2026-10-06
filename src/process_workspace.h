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

/** Sample processes and conditionally rebuild visible process models. */
gboolean lsm_processes_update(gpointer user_data);

/** Consume and present a completed background process snapshot immediately. */
void lsm_processes_present_ready_snapshot(LsmApp *app);

/** Build the shared process-action context menu. */
GtkWidget *lsm_process_actions_menu(LsmApp *app, gboolean include_columns);

/** Retain a selected PID together with its current instance token. */
void lsm_process_selection_set(LsmApp *app, guint64 pid);

/** Clear an application-group selection and retain no stale group PIDs. */
void lsm_process_group_selection_clear(LsmApp *app);

/** Start or stop recording the selected process. */
void lsm_process_record_set(LsmApp *app, gboolean active);

/** Append one current sample to the active process recording. */
gboolean lsm_process_record_append(LsmApp *app,
                                   const LsmProcessInfo *process);

/** Open the selected process in the technical inspector. */
void lsm_processes_show_selected_details(LsmApp *app);

/** Move the current process selection to the matching Details-page row. */
void lsm_processes_go_to_details(LsmApp *app);

/** Ask for confirmation and end the currently selected ordinary process. */
void lsm_processes_end_selected(LsmApp *app);

/** Load persisted process-filter rules into application-owned state. */
void lsm_process_filters_load(LsmApp *app);

/** Present the graphical process-filter editor. */
void lsm_process_filters_dialog(LsmApp *app);

/** Stop process CSV recording and let the detached writer drain and close it. */
void lsm_process_record_stop(LsmApp *app);

/** Return the shared process heatmap preference without exposing Details state. */
gboolean lsm_process_heatmap_enabled(const LsmApp *app);

/**
 * Synchronise the global recording action with a presentation selection.
 *
 * @param [in,out] app Application containing recording state.
 * @param ordinary_selection TRUE when an ordinary process can be recorded.
 * @param grouped_selection TRUE when the selection represents a process group.
 */
void lsm_process_record_action_sync(LsmApp *app,
                                    gboolean ordinary_selection,
                                    gboolean grouped_selection);

#endif
