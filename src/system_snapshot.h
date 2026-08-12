// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system_snapshot.h
 * @brief Native human-readable diagnostic snapshot export.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_SYSTEM_SNAPSHOT_H
#define LINUX_SYSTEM_MONITOR_SYSTEM_SNAPSHOT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct LsmApp LsmApp;

/**
 * Write the retained system and process snapshot atomically to a text file.
 *
 * @param [in] app Application containing the retained live snapshot.
 * @param [in] path Destination path to replace atomically.
 * @param [out] error Optional human-readable failure buffer.
 * @param [in] error_size Capacity of @p error.
 * @return true only when the complete file was synchronised and renamed.
 */
bool lsm_system_snapshot_write(const LsmApp *app, const char *path,
                               char *error, size_t error_size);

#endif
