// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_shell.h
 * @brief Internal global-window, menu and navigation coordination API.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_APP_SHELL_H
#define LINUX_SYSTEM_MONITOR_APP_SHELL_H

#include "app.h"

/**
 * Build the global menu bar and bind application actions.
 *
 * @param [in,out] app Active application context receiving menu widget handles.
 * @return Newly created GTK menu bar owned by the receiving widget hierarchy.
 */
GtkWidget *lsm_app_shell_build_menu(LsmApp *app);

/** Install the application CSS provider on the default screen. */
void lsm_app_shell_apply_css(void);

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
