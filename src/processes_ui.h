// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file processes_ui.h
 * @brief Friendly grouped Processes page.
 *
 * Presentation depends explicitly on process actions, navigation and the shared
 * snapshot workspace rather than receiving those APIs transitively from one
 * oversized process header.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESSES_UI_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESSES_UI_H

#include "process_actions.h"
#include "process_navigation.h"
#include "process_ui_bridge.h"
#include "process_workspace.h"

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Construct the grouped Processes page.
 * @param app Application owning the shared process snapshot.
 * @param container Empty GTK container receiving the page.
 */
void lsm_processes_build(LsmApp *app, GtkWidget *container);

/**
 * Present the newest retained snapshot in the grouped Processes model.
 * @param app Application whose grouped model is refreshed.
 */
void lsm_processes_present_snapshot(LsmApp *app);

/**
 * Report whether the grouped Processes page is currently visible.
 * @param app Application containing top-level page state.
 * @return TRUE when Processes is visible or before the notebook is built.
 */
gboolean lsm_processes_page_visible(const LsmApp *app);

/**
 * Release grouped Processes models and caches.
 * @param app Application whose Processes page is being destroyed.
 */
void lsm_processes_destroy(LsmApp *app);

#endif
