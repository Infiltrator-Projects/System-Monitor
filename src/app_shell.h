// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_shell.h
 * @brief Internal global-window, menu and navigation coordination API.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_APP_SHELL_H
#define LINUX_SYSTEM_MONITOR_APP_SHELL_H

#include "app.h"

/** Build the global menu bar and bind application actions. */
GtkWidget *lsm_app_shell_build_menu(LsmApp *app);
/** Install the application CSS provider. */
void lsm_app_shell_apply_css(void);
/** Connect top-level window state, keyboard and close handlers. */
void lsm_app_shell_connect_window(LsmApp *app);
/** Connect notebook navigation and on-demand refresh handling. */
void lsm_app_shell_connect_notebook(LsmApp *app);
/** Apply compact-summary visibility and window geometry policy. */
void lsm_app_shell_apply_compact_summary(LsmApp *app);
/** Persist one page's current vertical scroll position. */
void lsm_app_shell_save_page_scroll(LsmApp *app, gint page);
/** Cancel deferred shell-only callbacks during shutdown. */
void lsm_app_shell_cancel_pending(LsmApp *app);

#endif
