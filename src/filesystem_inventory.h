// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file filesystem_inventory.h
 * @brief Native, toolkit-independent inventory of mounted Linux filesystems.
 *
 * This module owns the operating-system boundary for the graphical File
 * Systems page. It parses the kernel mountinfo ABI, queries capacity with
 * statvfs(3), classifies implementation mounts, and returns one coherent
 * caller-owned snapshot. Keeping those operations outside GTK presentation
 * code preserves testability and prevents view refresh logic from acquiring
 * filesystem-policy responsibilities.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_FILESYSTEM_INVENTORY_H
#define LINUX_SYSTEM_MONITOR_FILESYSTEM_INVENTORY_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** One mounted filesystem and its most recent capacity snapshot. */
typedef struct {
    char source[LSM_PATH_LEN];       /**< Kernel-reported device or remote source. */
    char target[LSM_PATH_LEN];       /**< Decoded mount point. */
    char filesystem[64];             /**< Kernel filesystem type. */
    uint64_t total_bytes;            /**< Total addressable bytes when available. */
    uint64_t used_bytes;             /**< Bytes unavailable to all users. */
    uint64_t available_bytes;        /**< Bytes available to an unprivileged user. */
    unsigned used_percent;           /**< Clamped percentage in the inclusive range 0..100. */
    bool capacity_available;         /**< True only when statvfs(3) succeeded. */
    bool normally_visible;           /**< True for ordinary local or network storage. */
} LsmFilesystemInfo;

/**
 * Collect all visible mount records from the current process mount namespace.
 *
 * The result is sorted by mount point and then source for deterministic GUI
 * presentation. Failure to query one mount's capacity does not discard the
 * mount record; @ref LsmFilesystemInfo::capacity_available distinguishes that
 * partial result. The function performs no allocation when @p out_items is
 * NULL and never invokes another executable.
 *
 * @param [out] out_items Receives a heap array owned by the caller, or NULL.
 * @return Number of records in the returned snapshot; zero on failure or when
 *         the namespace contains no parseable mounts.
 * Complexity: O(M log M), where M is the number of mountinfo records.
 * Thread safety: safe for concurrent callers; the function retains no globals.
 * Blocking: statvfs(3) may wait on stale remote filesystems, so interactive
 * callers should execute collection away from their presentation thread.
 */
size_t lsm_filesystem_inventory_collect(LsmFilesystemInfo **out_items);

/**
 * Release an array returned by lsm_filesystem_inventory_collect().
 *
 * @param [in,out] items Array to release, or NULL.
 */
void lsm_filesystem_inventory_free(LsmFilesystemInfo *items);

#endif
