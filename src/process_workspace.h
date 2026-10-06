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

/** Authoritative process sampling callback owned by the workspace. */
gboolean lsm_process_workspace_update(gpointer user_data);

/** Authoritative completed-snapshot handoff owned by the workspace. */
void lsm_process_workspace_present_ready_snapshot(LsmApp *app);

/** Move the current process selection to the matching Details-page row. */
void lsm_process_workspace_go_to_details(LsmApp *app);

/*
 * Preserve the established application-facing names while moving ownership to
 * this neutral translation unit. Details can opt out while its legacy local
 * definitions are being retired; all composition/runtime callers resolve to
 * the workspace owner.
 */
#ifndef LSM_PROCESS_WORKSPACE_NO_ALIASES
#define lsm_processes_update lsm_process_workspace_update
#define lsm_processes_present_ready_snapshot \
    lsm_process_workspace_present_ready_snapshot
#define lsm_processes_go_to_details lsm_process_workspace_go_to_details
#endif

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

gboolean lsm_process_heatmap_enabled(const LsmApp *app);
void lsm_process_record_action_sync(LsmApp *app,
                                    gboolean ordinary_selection,
                                    gboolean grouped_selection);

#endif
