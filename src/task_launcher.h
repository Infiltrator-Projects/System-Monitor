// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file task_launcher.h
 * @brief Explicit graphical Run new task workflow.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_TASK_LAUNCHER_H
#define LINUX_SYSTEM_MONITOR_TASK_LAUNCHER_H

typedef struct LsmApp LsmApp;

/**
 * Present the Run new task dialog and launch an accepted argument vector.
 *
 * No shell is involved. Quoting is parsed by GLib and the first argument is
 * resolved through the user's normal executable search path.
 *
 * @param [in,out] app Parent application and window.
 */
void lsm_task_launcher_show(LsmApp *app);

#endif
