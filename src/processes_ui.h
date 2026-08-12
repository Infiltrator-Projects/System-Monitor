// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file processes_ui.h
 * @brief Friendly Windows 10-style grouped Processes page.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROCESSES_UI_H
#define LINUX_SYSTEM_MONITOR_PROCESSES_UI_H

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Construct the grouped Processes page.
 *
 * @param [in,out] app Application that owns the shared process snapshot.
 * @param [in] container Empty GTK container receiving the page.
 */
void lsm_processes_build(LsmApp *app, GtkWidget *container);

/**
 * Present the newest shared process snapshot when the grouped page is dirty.
 *
 * @param [in,out] app Application whose grouped model is rebuilt.
 */
void lsm_processes_present_snapshot(LsmApp *app);

/**
 * Report whether the grouped Processes page is currently visible.
 *
 * @param [in] app Application containing the top-level notebook.
 * @return TRUE when Processes is visible or the notebook is not built yet.
 */
gboolean lsm_processes_page_visible(const LsmApp *app);

/**
 * Move the current friendly-page selection to the matching Details row.
 *
 * @param [in,out] app Application containing the selected representative PID.
 */
void lsm_processes_go_to_details(LsmApp *app);

/**
 * Release the grouped Processes model's creator reference.
 *
 * @param [in,out] app Application whose periodic work has already stopped.
 */
void lsm_processes_destroy(LsmApp *app);

#endif
