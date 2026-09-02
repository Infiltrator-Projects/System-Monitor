// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file details_page.h
 * @brief Technical Details page and shared process actions.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_DETAILS_PAGE_H
#define LINUX_SYSTEM_MONITOR_DETAILS_PAGE_H

#include "monitor_types.h"

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Construct the technical Details tab and its advanced process table.
 *
 * @param [in,out] app Application that owns retained process state.
 * @param [in] container Empty GTK container receiving the process view.
 */
void lsm_details_build(LsmApp *app, GtkWidget *container);
/**
 * Sample processes and conditionally rebuild the visible process model.
 *
 * Sampling continues while the tab is hidden so totals and App History remain
 * current; expensive GTK tree reconstruction is deferred until visible.
 *
 * @param [in,out] user_data Pointer to the owning LsmApp.
 * @return G_SOURCE_CONTINUE while periodic scans should remain scheduled;
 *         otherwise G_SOURCE_REMOVE during shutdown.
 */
gboolean lsm_processes_update(gpointer user_data);
/**
 * Rebuild the visible process model from the newest retained snapshot.
 *
 * @param [in,out] app Application whose process tree is presented.
 */
void lsm_details_present_snapshot(LsmApp *app);
/**
 * Save process-table visibility, order, widths, sort and view mode.
 *
 * @param [in] app Application whose current table layout is persisted.
 */
void lsm_details_save_layout(const LsmApp *app);
/**
 * Present the process-column chooser from the View menu.
 *
 * @param [in,out] app Application whose visible columns may change.
 */
void lsm_details_show_columns(LsmApp *app);
/**
 * Build the shared process-action context menu.
 *
 * @param [in,out] app Application containing the current process selection.
 * @param [in] include_columns TRUE on Details, FALSE on the grouped page.
 * @return Newly constructed menu owned by GTK after it is displayed.
 */
GtkWidget *lsm_process_actions_menu(LsmApp *app, gboolean include_columns);
/**
 * Retain a selected PID together with its current instance token.
 *
 * @param [in,out] app Application whose shared selection is updated.
 * @param [in] pid Process identifier from a visible model row, or zero.
 */
void lsm_process_selection_set(LsmApp *app, guint64 pid);
/**
 * Clear an application-group selection and retain no stale group PIDs.
 *
 * @param [in,out] app Application whose shared process selection is reset.
 */
void lsm_process_group_selection_clear(LsmApp *app);
/**
 * Start or stop recording the selected process from a menu or context action.
 *
 * @param [in,out] app Application containing selection and recording state.
 * @param [in] active TRUE to start recording; FALSE to stop it.
 */
void lsm_process_record_set(LsmApp *app, gboolean active);
/**
 * Append one current sample to the active process recording.
 *
 * The row is copied into the detached recording writer. Any asynchronous
 * write failure is detected on the next append and stops recording rather than
 * leaving the interface in a false recording state.
 *
 * @param [in,out] app Application containing recording state.
 * @param [in] process Current process sample matching the recorded instance.
 * @return TRUE when the row was queued; FALSE after a writer failure.
 */
gboolean lsm_process_record_append(LsmApp *app,
                                   const LsmProcessInfo *process);
/**
 * Open details for the currently selected process when one is available.
 *
 * @param [in,out] app Application containing the process selection.
 */
void lsm_processes_show_selected_details(LsmApp *app);
/**
 * Ask for confirmation and end the currently selected ordinary process.
 *
 * @param [in,out] app Application containing the process selection.
 */
void lsm_processes_end_selected(LsmApp *app);
/**
 * Load persisted process-filter rules into application-owned state.
 *
 * @param [in,out] app Application receiving the filter set.
 */
void lsm_process_filters_load(LsmApp *app);
/**
 * Present the graphical process-filter editor.
 *
 * @param [in,out] app Application whose filter rules may be modified.
 */
void lsm_process_filters_dialog(LsmApp *app);
/**
 * Stop process CSV recording and let the detached writer drain/close it.
 *
 * @param [in,out] app Application containing recording state.
 */
void lsm_process_record_stop(LsmApp *app);

/**
 * Release the Details page models' creator references.
 *
 * @param [in,out] app Application whose periodic work has already stopped.
 */
void lsm_details_destroy(LsmApp *app);

#endif
