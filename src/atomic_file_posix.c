// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file atomic_file_posix.c
 * @brief Linux System Monitor compatibility facade over Common durable I/O.
 *
 * The reusable POSIX replacement algorithm lives in Infiltratr Common. This
 * file preserves the established `lsm_` API and maps only application-facing
 * permission names to the shared contract.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "atomic_file.h"

#include <errno.h>
#include <infiltratr/posix.h>

static bool common_mode(LsmAtomicFileMode mode,
                        InfiltratrAtomicFileMode *shared_mode)
{
    if (!shared_mode) return false;
    switch (mode) {
        case LSM_ATOMIC_FILE_PRIVATE:
            *shared_mode = INFILTRATR_ATOMIC_FILE_PRIVATE;
            return true;
        case LSM_ATOMIC_FILE_USER_DOCUMENT:
            *shared_mode = INFILTRATR_ATOMIC_FILE_PRESERVE_PERMISSIONS;
            return true;
    }
    return false;
}

int lsm_atomic_file_write(const char *path, LsmAtomicFileMode mode,
                          LsmAtomicFileWriter writer, const void *user_data)
{
    InfiltratrAtomicFileMode shared_mode;
    if (!common_mode(mode, &shared_mode)) return EINVAL;
    return infiltratr_atomic_file_write(path, shared_mode, writer, user_data);
}

int lsm_atomic_file_write_bytes(const char *path, LsmAtomicFileMode mode,
                                const void *data, size_t length)
{
    InfiltratrAtomicFileMode shared_mode;
    if (!common_mode(mode, &shared_mode)) return EINVAL;
    return infiltratr_atomic_file_write_bytes(path, shared_mode, data, length);
}
