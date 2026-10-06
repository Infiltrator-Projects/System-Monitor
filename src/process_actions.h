// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_actions.h
 * @brief Shared process selection, filtering, recording and user actions.
 *
 * Action/UI contracts are deliberately separate from process_workspace.h so
 * the sampling workspace remains independent of GTK action widgets.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_ACTIONS_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_ACTIONS_H

#include "monitor_types.h"

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Build the shared context menu for the current process selection.
 * @param app Application containing process selection state.
 * @param include_columns TRUE when Details column actions should be included.
 * @return Newly created GTK menu.
 */
GtkWidget *lsm_process_actions_menu(LsmApp *app, gboolean include_columns);

/**
 * Retain a selected PID together with its current instance identity.
 * @param app Application whose process selection is updated.
 * @param pid Selected PID, or zero to clear the ordinary selection.
 */
void lsm_process_selection_set(LsmApp *app, guint64 pid);

/**
 * Clear any retained grouped-process selection.
 * @param app Application whose grouped selection is cleared.
 */
void lsm_process_group_selection_clear(LsmApp *app);

/**
 * Start or stop recording the selected process.
 * @param app Application containing process selection and recording state.
 * @param active TRUE to start recording; FALSE to stop it.
 */
void lsm_process_record_set(LsmApp *app, gboolean active);

/**
 * Append a retained process sample to the active recording.
 * @param app Application owning the recorder.
 * @param process Current process sample matching the recorded instance.
 * @return TRUE when the row was accepted; FALSE after a recording failure.
 */
gboolean lsm_process_record_append(LsmApp *app,
                                   const LsmProcessInfo *process);

/**
 * Open the inspector for the currently selected process.
 * @param app Application containing the process selection.
 */
void lsm_processes_show_selected_details(LsmApp *app);

/**
 * Confirm as required and end the currently selected process.
 * @param app Application containing the process selection.
 */
void lsm_processes_end_selected(LsmApp *app);

/**
 * Load persisted process exclusion filters.
 * @param app Application receiving the filter rules.
 */
void lsm_process_filters_load(LsmApp *app);

/**
 * Present the graphical process-filter editor.
 * @param app Application whose process filters may change.
 */
void lsm_process_filters_dialog(LsmApp *app);

/**
 * Stop process recording and close its detached writer.
 * @param app Application owning the active recorder.
 */
void lsm_process_record_stop(LsmApp *app);

#endif
