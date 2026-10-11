// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system_snapshot.h
 * @brief Native human-readable diagnostic snapshot export.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_SYSTEM_SNAPSHOT_H
#define INFILTRATOR_SYSTEM_MONITOR_SYSTEM_SNAPSHOT_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>

/** Completed plain-C state consumed by the diagnostic snapshot writer. */
typedef struct {
    const LsmMonitor *monitor;
    const LsmProcessInfo *processes;
    size_t process_count;
} LsmSystemSnapshotView;

/**
 * Write one completed system and process snapshot atomically to a text file.
 *
 * The writer deliberately knows nothing about LsmApp or GTK ownership. Its
 * caller supplies only the completed monitor and process data to serialize.
 *
 * @param [in] view Completed retained data to export.
 * @param [in] path Destination path to replace atomically.
 * @param [out] error Optional human-readable failure buffer.
 * @param [in] error_size Capacity of @p error.
 * @return true only when the complete file was synchronised and renamed.
 */
bool lsm_system_snapshot_write(const LsmSystemSnapshotView *view,
                               const char *path, char *error,
                               size_t error_size);

#endif
