// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_workspace.h
 * @brief Shared process sampling and snapshot lifecycle.
 *
 * The workspace is application-domain coordination shared by the friendly
 * Processes page and the technical Details page. GTK actions, cross-page
 * navigation and page widget state live behind separate presentation contracts.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_WORKSPACE_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_WORKSPACE_H

#include <glib.h>

typedef struct LsmApp LsmApp;

gboolean lsm_process_workspace_update(gpointer user_data);
void lsm_process_workspace_present_ready_snapshot(LsmApp *app);

/* Stable application-facing names retained for existing callers. */
gboolean lsm_processes_update(gpointer user_data);
void lsm_processes_present_ready_snapshot(LsmApp *app);

#endif
