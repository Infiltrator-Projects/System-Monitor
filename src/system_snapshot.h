// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system_snapshot.h
 * @brief Native human-readable diagnostic snapshot export.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef SYSTEM_MONITOR_SYSTEM_SNAPSHOT_H
#define SYSTEM_MONITOR_SYSTEM_SNAPSHOT_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct LsmApp LsmApp;

/** Completed plain-C state consumed by the diagnostic snapshot writer. */
typedef struct LsmSystemSnapshotView {
    const LsmMonitor *monitor;
    const LsmProcessInfo *processes;
    size_t process_count;
} LsmSystemSnapshotView;

/**
 * Write one completed data snapshot atomically to a text file.
 *
 * This data-only API deliberately knows nothing about LsmApp or GTK ownership.
 *
 * @param [in] view Completed retained data to export.
 * @param [in] path Destination path to replace atomically.
 * @param [out] error Optional human-readable failure buffer.
 * @param [in] error_size Capacity of @p error.
 * @return true only when the complete file was synchronised and renamed.
 */
bool lsm_system_snapshot_write_view(const LsmSystemSnapshotView *view,
                                    const char *path, char *error,
                                    size_t error_size);

/**
 * Adapt application-owned completed state and write a diagnostic snapshot.
 *
 * The application adapter is implemented separately from the data serializer
 * so the serializer and its regression test remain independent of LsmApp.
 *
 * @param [in] app Application containing the retained completed snapshots.
 * @param [in] path Destination path to replace atomically.
 * @param [out] error Optional human-readable failure buffer.
 * @param [in] error_size Capacity of @p error.
 * @return true only when application data was available and fully written.
 */
bool lsm_system_snapshot_write(const LsmApp *app, const char *path,
                               char *error, size_t error_size);

#endif
