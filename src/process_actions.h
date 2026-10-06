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

GtkWidget *lsm_process_actions_menu(LsmApp *app, gboolean include_columns);
void lsm_process_selection_set(LsmApp *app, guint64 pid);
void lsm_process_group_selection_clear(LsmApp *app);
void lsm_process_record_set(LsmApp *app, gboolean active);
gboolean lsm_process_record_append(LsmApp *app,
                                   const LsmProcessInfo *process);
void lsm_processes_show_selected_details(LsmApp *app);
void lsm_processes_end_selected(LsmApp *app);
void lsm_process_filters_load(LsmApp *app);
void lsm_process_filters_dialog(LsmApp *app);
void lsm_process_record_stop(LsmApp *app);

#endif
