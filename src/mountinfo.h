// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mountinfo.h
 * @brief Native parser for the Linux /proc/PID/mountinfo interface.
 *
 * The parser is intentionally independent of libmount. It exposes only the
 * fields required by the monitor and preserves the kernel block-device number
 * used to associate aliases, UUID mounts and bind mounts with sysfs devices.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_MOUNTINFO_H
#define INFILTRATOR_SYSTEM_MONITOR_MOUNTINFO_H

#include <stddef.h>

#include "monitor_types.h"

/** One successfully parsed kernel mount record. */
typedef struct {
    unsigned major_number;             /**< Kernel block-device major number. */
    unsigned minor_number;             /**< Kernel block-device minor number. */
    char source[LSM_PATH_LEN];          /**< Filesystem source after kernel unescaping. */
    char target[LSM_PATH_LEN];          /**< Mount point after kernel unescaping. */
    char filesystem[64];                /**< Filesystem type. */
} LsmMountInfoEntry;

/** Visitor invoked once for each valid mountinfo record. */
typedef bool (*LsmMountInfoVisitor)(const LsmMountInfoEntry *entry, void *user_data);

/**
 * Parse a mountinfo stream while preserving the distinction between a valid
 * empty namespace and an I/O failure.
 *
 * @param [in] path Mountinfo file to parse.
 * @param [in] visitor Callback invoked once for each valid record.
 * @param [in,out] user_data Opaque value forwarded to @p visitor.
 * @param [out] out_count Number of valid records delivered to the visitor.
 * @return true when the file was opened and read to completion; false on
 *         stream, close, or argument failure.
 */
bool lsm_mountinfo_visit_file_checked(const char *path,
                                      LsmMountInfoVisitor visitor,
                                      void *user_data,
                                      size_t *out_count);

#endif
