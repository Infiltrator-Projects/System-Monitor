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

/**
 * Consume completed process work and request the next background scan.
 * @param user_data Owning LsmApp passed by the GLib timer.
 * @return G_SOURCE_CONTINUE while sampling remains scheduled, otherwise
 *         G_SOURCE_REMOVE during shutdown.
 */
gboolean lsm_process_workspace_update(gpointer user_data);

/**
 * Consume a completed process snapshot without requesting another scan.
 * @param app Application receiving the completed snapshot.
 */
void lsm_process_workspace_present_ready_snapshot(LsmApp *app);

/**
 * Stable application-facing alias for periodic process workspace updates.
 * @param user_data Owning LsmApp passed by the GLib timer.
 * @return G_SOURCE_CONTINUE while sampling remains scheduled, otherwise
 *         G_SOURCE_REMOVE during shutdown.
 */
gboolean lsm_processes_update(gpointer user_data);

/**
 * Stable application-facing alias for completed snapshot presentation.
 * @param app Application receiving the completed snapshot.
 */
void lsm_processes_present_ready_snapshot(LsmApp *app);

#endif
