// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_inspector.h
 * @brief Graphical detailed-process inspection and file-owner search.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROCESS_INSPECTOR_H
#define LINUX_SYSTEM_MONITOR_PROCESS_INSPECTOR_H

#include "app.h"
#include "process_model.h"

/**
 * Open a non-modal Process Inspector window for one process identity.
 *
 * The window tracks the process identifier and opaque instance identity captured
 * from the current process snapshot. If the platform recycles the identifier,
 * live updates stop rather than displaying data from the replacement process.
 *
 * @param [in,out] app Owning GUI application, used only on the GTK main thread.
 * @param [in] pid Process selected by the user.
 * @param [in] instance_id Opaque identity captured with the selected process.
 */
void lsm_process_inspector_show(LsmApp *app, LsmProcessId pid,
                                LsmProcessInstanceId instance_id);

/**
 * Open a graphical file chooser and show processes using the selected file.
 *
 * The search is implemented by the active process-inspection backend and does
 * not invoke lsof, fuser or another executable.
 *
 * @param [in,out] app Owning GUI application, used only on the GTK main thread.
 */
void lsm_process_file_users_show(LsmApp *app);

#endif
