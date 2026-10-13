// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_process_runtime.c
 * @brief Native process-worker lifetime and snapshot handoff for Win32.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "windows_process_runtime.h"

static DWORD WINAPI process_worker_main(LPVOID user_data)
{
    LsmWindowsProcessRuntime *state = user_data;
    if (!state) return 0U;

    LsmProcessBackend *backend = lsm_process_backend_create();
    EnterCriticalSection(&state->process_lock);
    state->process_backend = backend;
    LeaveCriticalSection(&state->process_lock);
    if (!backend) return 0U;

    for (;;) {
        (void)WaitForSingleObject(state->process_event, INFINITE);

        EnterCriticalSection(&state->process_lock);
        if (state->process_stop_requested) {
            LeaveCriticalSection(&state->process_lock);
            break;
        }
        if (!state->process_request_pending) {
            LeaveCriticalSection(&state->process_lock);
            continue;
        }
        const unsigned flags = state->process_requested_flags;
        state->process_request_pending = false;
        LeaveCriticalSection(&state->process_lock);

        LsmProcessInfo *processes = NULL;
        const size_t count = lsm_process_scan(backend, &processes, flags);

        EnterCriticalSection(&state->process_lock);
        if (state->process_stop_requested) {
            LeaveCriticalSection(&state->process_lock);
            lsm_process_list_free(processes);
            break;
        }
        if (processes) {
            lsm_process_list_free(state->process_completed);
            state->process_completed = processes;
            state->process_completed_count = count;
            state->process_completed_ready = true;
        }
        LeaveCriticalSection(&state->process_lock);
    }

    lsm_process_backend_destroy(backend);
    EnterCriticalSection(&state->process_lock);
    state->process_backend = NULL;
    LeaveCriticalSection(&state->process_lock);
    return 0U;
}

bool lsm_windows_process_runtime_start(LsmWindowsProcessRuntime *state)
{
    if (!state) return false;
    if (state->process_thread) return true;

    InitializeCriticalSection(&state->process_lock);
    state->process_lock_initialised = true;
    state->process_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!state->process_event) {
        DeleteCriticalSection(&state->process_lock);
        state->process_lock_initialised = false;
        return false;
    }

    state->process_request_pending = true;
    state->process_requested_flags = LSM_PROCESS_SCAN_NONE;
    state->process_thread = CreateThread(
        NULL, 0U, process_worker_main, state, 0U, NULL);
    if (!state->process_thread) {
        CloseHandle(state->process_event);
        state->process_event = NULL;
        DeleteCriticalSection(&state->process_lock);
        state->process_lock_initialised = false;
        return false;
    }
    SetEvent(state->process_event);
    return true;
}

void lsm_windows_process_runtime_request(LsmWindowsProcessRuntime *state,
                                     unsigned scan_flags)
{
    if (!state || !state->process_thread || !state->process_lock_initialised)
        return;
    EnterCriticalSection(&state->process_lock);
    if (!state->process_stop_requested) {
        state->process_requested_flags = scan_flags;
        state->process_request_pending = true;
    }
    LeaveCriticalSection(&state->process_lock);
    SetEvent(state->process_event);
}

bool lsm_windows_process_runtime_take(LsmWindowsProcessRuntime *state,
                                  LsmProcessInfo **processes,
                                  size_t *count)
{
    if (!state || !processes || !count || !state->process_lock_initialised)
        return false;
    *processes = NULL;
    *count = 0U;

    EnterCriticalSection(&state->process_lock);
    if (!state->process_completed_ready) {
        LeaveCriticalSection(&state->process_lock);
        return false;
    }
    *processes = state->process_completed;
    *count = state->process_completed_count;
    state->process_completed = NULL;
    state->process_completed_count = 0U;
    state->process_completed_ready = false;
    LeaveCriticalSection(&state->process_lock);
    return true;
}

bool lsm_windows_process_runtime_stop(LsmWindowsProcessRuntime *state)
{
    if (!state || !state->process_lock_initialised) return true;

    EnterCriticalSection(&state->process_lock);
    state->process_stop_requested = true;
    LeaveCriticalSection(&state->process_lock);
    if (state->process_event) SetEvent(state->process_event);

    if (state->process_thread) {
        (void)CancelSynchronousIo(state->process_thread);
        const DWORD stopped = WaitForSingleObject(state->process_thread, 500U);
        if (stopped != WAIT_OBJECT_0) {
            /*
             * Some native queries cannot be cancelled. Keep the process-worker
             * synchronization objects and containing UI state alive until
             * process exit instead of freeing memory that the worker can still
             * dereference. The normal path below remains leak-free.
             */
            state->process_worker_abandoned = true;
            return false;
        }
        CloseHandle(state->process_thread);
        state->process_thread = NULL;
    }
    if (state->process_event) {
        CloseHandle(state->process_event);
        state->process_event = NULL;
    }

    lsm_process_list_free(state->process_completed);
    state->process_completed = NULL;
    state->process_completed_count = 0U;
    state->process_completed_ready = false;
    DeleteCriticalSection(&state->process_lock);
    state->process_lock_initialised = false;
    return true;
}

bool lsm_windows_process_runtime_available(
    const LsmWindowsProcessRuntime *runtime)
{
    if (!runtime || !runtime->process_thread ||
        !runtime->process_lock_initialised)
        return false;

    /*
     * A Windows thread handle remains valid after its thread exits. Backend
     * creation happens inside the worker after start() has returned, so a
     * failed backend can leave a valid handle referring to a terminated
     * worker. The worker always returns zero; STILL_ACTIVE therefore
     * unambiguously represents the one state in which requests can make
     * progress.
     */
    DWORD exit_code = 0U;
    return GetExitCodeThread(runtime->process_thread, &exit_code) != FALSE &&
           exit_code == STILL_ACTIVE;
}

bool lsm_windows_process_runtime_abandoned(
    const LsmWindowsProcessRuntime *runtime)
{
    return runtime && runtime->process_worker_abandoned;
}
