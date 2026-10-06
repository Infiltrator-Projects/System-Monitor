// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file details_page.h
 * @brief Technical Details page presentation interface.
 *
 * Sampling lifecycle is owned by process_workspace.h; process mutation,
 * filtering and recording actions are owned by process_actions.h. This header
 * owns only the technical Details presentation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_DETAILS_PAGE_H
#define INFILTRATOR_SYSTEM_MONITOR_DETAILS_PAGE_H

#include "process_actions.h"
#include "process_workspace.h"

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

void lsm_details_build(LsmApp *app, GtkWidget *container);
void lsm_details_present_snapshot(LsmApp *app);
unsigned lsm_details_process_scan_flags(const LsmApp *app);
void lsm_details_save_layout(const LsmApp *app);
void lsm_details_show_columns(LsmApp *app);
void lsm_details_destroy(LsmApp *app);

#endif
