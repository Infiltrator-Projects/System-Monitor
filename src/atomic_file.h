// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file atomic_file.h
 * @brief System Monitor names mapped directly to Common durable file publication.
 *
 * Durable replacement semantics, permission handling and cleanup are owned by
 * Infiltratr Common. These aliases retain the established System Monitor API
 * without a local implementation layer.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_ATOMIC_FILE_H
#define INFILTRATOR_SYSTEM_MONITOR_ATOMIC_FILE_H

#include <infiltratr/posix.h>

typedef InfiltratrAtomicFileMode LsmAtomicFileMode;
typedef InfiltratrAtomicFileWriter LsmAtomicFileWriter;

#define LSM_ATOMIC_FILE_PRIVATE INFILTRATR_ATOMIC_FILE_PRIVATE
#define LSM_ATOMIC_FILE_USER_DOCUMENT INFILTRATR_ATOMIC_FILE_PRESERVE_PERMISSIONS
#define lsm_atomic_file_write infiltratr_atomic_file_write
#define lsm_atomic_file_write_bytes infiltratr_atomic_file_write_bytes

#endif
