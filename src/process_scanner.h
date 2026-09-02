// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_scanner.h
 * @brief Coalescing background process-snapshot worker.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROCESS_SCANNER_H
#define LINUX_SYSTEM_MONITOR_PROCESS_SCANNER_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct LsmProcessScanner LsmProcessScanner;

/**
 * Create a detached scanner that exclusively owns its process backend.
 *
 * @return New scanner, or NULL when allocation/backend/thread creation fails.
 */
LsmProcessScanner *lsm_process_scanner_create(void);

/**
 * Request a process scan, coalescing with any request already in progress.
 *
 * The most recent flags replace older pending flags. A scan already executing
 * is allowed to finish and the newest pending request runs immediately after it.
 *
 * @param [in,out] scanner Scanner worker.
 * @param [in] scan_flags LSM_PROCESS_SCAN_* flags required by the active UI.
 * @return true when the request was accepted; false after shutdown.
 */
bool lsm_process_scanner_request(LsmProcessScanner *scanner,
                                 unsigned scan_flags);

/**
 * Transfer the newest completed snapshot to the caller without blocking.
 *
 * @param [in,out] scanner Scanner worker.
 * @param [out] processes Caller-owned process array on success.
 * @param [out] count Number of rows in @p processes.
 * @return true when a completed snapshot was transferred.
 */
bool lsm_process_scanner_take(LsmProcessScanner *scanner,
                              LsmProcessInfo **processes,
                              size_t *count);

/**
 * Begin non-blocking scanner shutdown and release the caller's ownership.
 *
 * The detached worker owns its backend until any in-flight native/NSS scan
 * returns, so a pathological lookup cannot prevent the GUI from exiting.
 *
 * @param [in,out] scanner Scanner to stop. NULL is accepted.
 */
void lsm_process_scanner_destroy(LsmProcessScanner *scanner);

#endif
