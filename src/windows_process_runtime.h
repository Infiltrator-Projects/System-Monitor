// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_process_runtime.h
 * @brief Native process-worker lifetime contract for the Win32 shell.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_WINDOWS_PROCESS_RUNTIME_H
#define INFILTRATOR_SYSTEM_MONITOR_WINDOWS_PROCESS_RUNTIME_H

#include "process_backend.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    LsmProcessBackend *process_backend;
    HANDLE process_thread;
    HANDLE process_event;
    CRITICAL_SECTION process_lock;
    bool process_lock_initialised;
    bool process_worker_abandoned;
    bool process_stop_requested;
    bool process_request_pending;
    unsigned process_requested_flags;
    LsmProcessInfo *process_completed;
    size_t process_completed_count;
    bool process_completed_ready;
} LsmWindowsProcessRuntime;

/**
 * Start the native process worker if it is not already running.
 * @param [in,out] runtime Process runtime to initialise.
 * @return true when a worker is available.
 */
bool lsm_windows_process_runtime_start(LsmWindowsProcessRuntime *runtime);
/**
 * Queue a process snapshot request without blocking the UI thread.
 * @param [in,out] runtime Process runtime receiving the request.
 * @param [in] scan_flags Backend scan flags for the requested snapshot.
 */
void lsm_windows_process_runtime_request(LsmWindowsProcessRuntime *runtime,
                                         unsigned scan_flags);
/**
 * Transfer ownership of a completed process snapshot to the caller.
 * @param [in,out] runtime Process runtime containing a completed snapshot.
 * @param [out] processes Receives the owned process array.
 * @param [out] count Receives the process count.
 * @return true when a completed snapshot was transferred.
 */
bool lsm_windows_process_runtime_take(LsmWindowsProcessRuntime *runtime,
                                      LsmProcessInfo **processes,
                                      size_t *count);
/**
 * Stop the process worker and release normal-path synchronization state.
 * @param [in,out] runtime Process runtime to stop.
 * @return true when the worker stopped; false when a native query could not be cancelled.
 */
bool lsm_windows_process_runtime_stop(LsmWindowsProcessRuntime *runtime);
/**
 * Return whether the runtime currently owns a live worker thread.
 * @param [in] runtime Process runtime to inspect.
 * @return true while a usable worker is owned.
 */
bool lsm_windows_process_runtime_available(
    const LsmWindowsProcessRuntime *runtime);
/**
 * Return whether shutdown had to leave the worker storage alive until exit.
 * @param [in] runtime Process runtime to inspect.
 * @return true when cancellation failed and storage must survive until process exit.
 */
bool lsm_windows_process_runtime_abandoned(
    const LsmWindowsProcessRuntime *runtime);

#endif
