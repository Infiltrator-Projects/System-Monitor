// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_file_users.h
 * @brief Graphical exact-file process-owner search.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_FILE_USERS_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_FILE_USERS_H

#include "app.h"

/**
 * Open a graphical file chooser and show processes using the selected file.
 *
 * The native /proc search runs on a worker and does not invoke lsof, fuser or
 * another executable.
 *
 * @param [in,out] app Owning GUI application, used only on the GTK main thread.
 */
void lsm_process_file_users_show(LsmApp *app);

#endif
