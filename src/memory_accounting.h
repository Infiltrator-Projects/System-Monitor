// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file memory_accounting.h
 * @brief Testable Linux meminfo parsing and byte-accounting contract.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_MEMORY_ACCOUNTING_H
#define LINUX_SYSTEM_MONITOR_MEMORY_ACCOUNTING_H

#include "monitor_types.h"

#include <stdbool.h>

/**
 * Apply current procfs memory fields to a retained memory snapshot.
 *
 * MemAvailable and commit accounting are read on every call. Cached, kernel
 * slab, page-table and corrupted-memory details are refreshed only when
 * @p refresh_details is true. Kernel `kB`
 * values are converted with exactly 1024 bytes per KB, and all intermediate
 * additions and multiplications are overflow-safe.
 *
 * @param [in] path Path to a Linux meminfo-formatted file.
 * @param [in,out] memory Memory snapshot receiving parsed byte quantities.
 * @param [in] refresh_details Whether to refresh the slower detail fields.
 * @return true when a valid MemAvailable field was found, including a genuine
 *         zero value.
 */
bool lsm_memory_accounting_read(const char *path, LsmMemoryInfo *memory,
                                bool refresh_details);

#endif
