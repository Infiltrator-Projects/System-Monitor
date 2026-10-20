// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_export.h
 * @brief Copy and CSV export of the current process-table selection.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef SYSTEM_MONITOR_PROCESS_EXPORT_H
#define SYSTEM_MONITOR_PROCESS_EXPORT_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct LsmApp LsmApp;

/** Completed process rows plus immutable selection identity for export. */
typedef struct {
    const LsmProcessInfo *processes;
    size_t process_count;
    LsmProcessId selected_pid;
    LsmProcessInstanceId selected_instance_id;
    const LsmProcessId *selected_group_pids;
    const LsmProcessInstanceId *selected_group_instance_ids;
    size_t selected_group_count;
} LsmProcessExportView;

/**
 * Test whether a completed row is selected by exact (PID, instance) identity.
 *
 * Explicit group membership is authoritative even without a highlighted
 * representative PID. Missing identities or incomplete group arrays select
 * nothing. Shared by both clipboard and CSV projection.
 */
bool lsm_process_export_contains(const LsmProcessExportView *view,
                                 const LsmProcessInfo *process);

/** Count matching completed rows, or zero for an invalid/incomplete view. */
size_t lsm_process_export_selection_count(const LsmProcessExportView *view);

/**
 * Write selected process rows atomically as UTF-8 CSV from plain process data.
 *
 * This data-only API deliberately knows nothing about LsmApp or GTK ownership.
 *
 * @param [in] view Completed process rows and selection identity to export.
 * @param [in] path Destination path to replace atomically.
 * @param [out] error Optional human-readable failure buffer.
 * @param [in] error_size Capacity of @p error.
 * @return true only when at least one selected row was written completely.
 */
bool lsm_process_export_write_view(const LsmProcessExportView *view,
                                   const char *path, char *error,
                                   size_t error_size);

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
 * Adapt application-owned selection state and write selected rows as UTF-8 CSV.
 *
 * @param [in] app Application containing the process rows and selection.
 * @param [in] path Destination path to replace atomically.
 * @param [out] error Optional human-readable failure buffer.
 * @param [in] error_size Capacity of @p error.
 * @return true only when application data was available and fully written.
 */
bool lsm_process_export_selected_csv(const LsmApp *app, const char *path,
                                     char *error, size_t error_size);

#endif
