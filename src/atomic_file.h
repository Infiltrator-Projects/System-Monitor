// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file atomic_file.h
 * @brief Platform-neutral contract for durable application file replacement.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_ATOMIC_FILE_H
#define LINUX_SYSTEM_MONITOR_ATOMIC_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/** Permission handling for a replacement file. */
typedef enum {
    /** Always restrict the completed file to its owning user. */
    LSM_ATOMIC_FILE_PRIVATE,
    /** Preserve an existing regular file's permissions; otherwise use 0600. */
    LSM_ATOMIC_FILE_USER_DOCUMENT
} LsmAtomicFileMode;

/**
 * Stream content into an atomic replacement file.
 *
 * The callback must not close the stream. Returning false abandons the
 * replacement and preserves the existing destination.
 */
typedef bool (*LsmAtomicFileWriter)(FILE *stream, const void *user_data);

/**
 * Durably replace a file with content supplied by a callback.
 *
 * The platform provider creates an unpredictable temporary file beside the
 * destination, syncs it, renames it and syncs the containing directory. If the
 * final directory sync fails, the replacement has occurred but its survival
 * across a power loss is not guaranteed.
 *
 * @param path Destination path.
 * @param mode Permission policy for the completed file.
 * @param writer Callback that writes the complete replacement.
 * @param user_data Opaque value passed to @p writer.
 * @return Zero on durable success, otherwise an errno-compatible error value.
 */
int lsm_atomic_file_write(const char *path, LsmAtomicFileMode mode,
                          LsmAtomicFileWriter writer, const void *user_data);

/**
 * Durably replace a file with an in-memory byte sequence.
 *
 * @param path Destination path.
 * @param mode Permission policy for the completed file.
 * @param data Bytes to write; may be NULL only when @p length is zero.
 * @param length Number of bytes to write.
 * @return Zero on durable success, otherwise an errno-compatible error value.
 */
int lsm_atomic_file_write_bytes(const char *path, LsmAtomicFileMode mode,
                                const void *data, size_t length);

#endif
