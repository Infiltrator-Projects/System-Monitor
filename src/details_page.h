// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file details_page.h
 * @brief Technical Details page presentation interface.
 *
 * Shared process sampling, selection, actions, filtering and recording belong
 * to process_workspace.h. This header owns only the technical Details page.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_DETAILS_PAGE_H
#define INFILTRATOR_SYSTEM_MONITOR_DETAILS_PAGE_H

#include "process_workspace.h"

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Construct the technical Details tab and its advanced process table.
 *
 * @param [in,out] app Application that owns retained process state.
 * @param [in] container Empty GTK container receiving the Details view.
 */
void lsm_details_build(LsmApp *app, GtkWidget *container);

/**
 * Rebuild the visible technical process model from the retained snapshot.
 *
 * @param [in,out] app Application whose Details tree is presented.
 */
void lsm_details_present_snapshot(LsmApp *app);

/**
 * Return optional backend scan fields required by visible Details columns.
 *
 * @param [in] app Application containing the current Details column state.
 * @return Bitwise LSM_PROCESS_SCAN_* requirements for optional Details fields.
 */
unsigned lsm_details_process_scan_flags(const LsmApp *app);

/**
 * Save process-table visibility, order, widths, sort and view mode.
 *
 * @param [in] app Application whose current Details layout is persisted.
 */
void lsm_details_save_layout(const LsmApp *app);

/**
 * Present the process-column chooser from the View menu.
 *
 * @param [in,out] app Application whose visible Details columns may change.
 */
void lsm_details_show_columns(LsmApp *app);

/**
 * Release the Details page models' creator references.
 *
 * @param [in,out] app Application whose Details models are destroyed.
 */
void lsm_details_destroy(LsmApp *app);

#endif
