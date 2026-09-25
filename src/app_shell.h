// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_shell.h
 * @brief Internal global-window, menu and navigation coordination API.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_SHELL_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_SHELL_H

#include "app.h"

/**
 * Apply the selected appearance mode.
 *
 * Follow-system mode keeps the host GTK palette authoritative. Day and Night
 * use the semantic palettes supplied by Infiltratr Common.
 *
 * @param [in,out] app Active application context.
 */
void lsm_app_shell_apply_theme(LsmApp *app);

/**
 * Connect top-level window state, keyboard and close handlers.
 *
 * @param [in,out] app Application whose toplevel window is already constructed.
 */
void lsm_app_shell_connect_window(LsmApp *app);

/**
 * Connect notebook navigation and on-demand refresh handling.
 *
 * @param [in,out] app Application whose notebook is already constructed.
 */
void lsm_app_shell_connect_notebook(LsmApp *app);

/**
 * Build the persistent graphical primary navigation rail.
 *
 * The rail replaces the notebook's visible text tabs while retaining the
 * notebook as the stable internal page container. Performance resource classes
 * are promoted into first-class destinations without changing device identity.
 *
 * @param [in,out] app Active application context.
 * @return GTK widget owning the primary navigation rail.
 */
GtkWidget *lsm_app_shell_build_navigation(LsmApp *app);

/**
 * Synchronise the graphical rail with the active tab/resource.
 *
 * @param [in,out] app Active application context.
 */
void lsm_app_shell_sync_navigation(LsmApp *app);

/**
 * Apply compact-summary visibility and window geometry policy.
 *
 * @param [in,out] app Active application context.
 */
void lsm_app_shell_apply_compact_summary(LsmApp *app);

/**
 * Persist one page's current vertical scroll position in runtime state.
 *
 * @param [in,out] app Active application context.
 * @param [in] page Current tab index to snapshot.
 */
void lsm_app_shell_save_page_scroll(LsmApp *app, gint page);

/**
 * Cancel deferred shell-only callbacks during shutdown.
 *
 * @param [in,out] app Application context being shut down.
 */
void lsm_app_shell_cancel_pending(LsmApp *app);

#endif
