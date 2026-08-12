// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mountinfo.h
 * @brief Native parser for the Linux /proc/<pid>/mountinfo interface.
 *
 * The parser is intentionally independent of libmount. It exposes only the
 * fields required by the monitor and preserves the kernel block-device number
 * used to associate aliases, UUID mounts and bind mounts with sysfs devices.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_MOUNTINFO_H
#define LINUX_SYSTEM_MONITOR_MOUNTINFO_H

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
 * Parse a Linux mountinfo file and invoke a visitor for each valid record.
 *
 * The parser decodes mountinfo escaping and ignores malformed individual lines
 * so one corrupt record cannot discard the complete mount inventory.
 *
 * @param [in] path Mountinfo file to parse.
 * @param [in] visitor Callback invoked synchronously for each valid record.
 * @param [in,out] user_data Opaque value forwarded to @p visitor.
 * @return Number of valid records delivered to the visitor.
 */
size_t lsm_mountinfo_visit_file(const char *path,
                                LsmMountInfoVisitor visitor,
                                void *user_data);

#endif
