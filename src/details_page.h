// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file details_page.h
 * @brief Technical Details page presentation interface.
 *
 * Sampling lifecycle is owned by process_workspace.h; process mutation,
 * filtering and recording actions are owned by process_actions.h. Shared
 * process-presentation state is reached through process_ui_bridge.h. This
 * header owns only the technical Details presentation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_DETAILS_PAGE_H
#define INFILTRATOR_SYSTEM_MONITOR_DETAILS_PAGE_H

#include "process_actions.h"
#include "process_ui_bridge.h"
#include "process_workspace.h"

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Construct the technical Details page.
 * @param app Application owning retained process state.
 * @param container Empty GTK container receiving the page.
 */
void lsm_details_build(LsmApp *app, GtkWidget *container);

/**
 * Present the retained process snapshot in the technical Details model.
 * @param app Application whose Details presentation is refreshed.
 */
void lsm_details_present_snapshot(LsmApp *app);

/**
 * Return optional process fields required by visible Details columns.
 * @param app Application containing current Details column state.
 * @return Bitwise LSM_PROCESS_SCAN_* requirements.
 */
unsigned lsm_details_process_scan_flags(const LsmApp *app);

/**
 * Save Details column visibility, order, widths and view mode.
 * @param app Application whose current Details layout is persisted.
 */
void lsm_details_save_layout(const LsmApp *app);

/**
 * Present the Details column chooser.
 * @param app Application whose visible columns may change.
 */
void lsm_details_show_columns(LsmApp *app);

/**
 * Release Details models and presentation resources.
 * @param app Application whose Details page is being destroyed.
 */
void lsm_details_destroy(LsmApp *app);

#endif
