// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_export.h
 * @brief Copy and CSV export of the current process-table selection.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROCESS_EXPORT_H
#define LINUX_SYSTEM_MONITOR_PROCESS_EXPORT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct LsmApp LsmApp;

/**
 * Copy selected process rows as tab-separated text to the GUI clipboard.
 *
 * @param [in] app Application containing the current process selection.
 */
void lsm_process_export_copy_selected(const LsmApp *app);

/**
 * Present a save dialog and write selected process rows as UTF-8 CSV.
 *
 * @param [in,out] app Parent window, process snapshot and selection state.
 */
void lsm_process_export_selected_dialog(LsmApp *app);

/**
 * Write selected process rows atomically as UTF-8 CSV.
 *
 * @param [in] app Application containing the process rows and selection.
 * @param [in] path Destination path to replace atomically.
 * @param [out] error Optional human-readable failure buffer.
 * @param [in] error_size Capacity of @p error.
 * @return true only when at least one selected row was written completely.
 */
bool lsm_process_export_selected_csv(const LsmApp *app, const char *path,
                                     char *error, size_t error_size);

#endif
