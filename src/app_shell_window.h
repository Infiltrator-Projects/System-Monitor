// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_shell_window.h
 * @brief Page-neutral top-level window event mechanics.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_SHELL_WINDOW_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_SHELL_WINDOW_H

typedef struct LsmApp LsmApp;

/**
 * Connect page-neutral close, geometry and window-state event handlers.
 * @param app Application whose top-level GTK window is already constructed.
 */
void lsm_app_shell_window_connect(LsmApp *app);

#endif
