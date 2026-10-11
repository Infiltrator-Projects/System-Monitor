// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_backend_windows_worker.c
 * @brief Opaque background process-sampling worker for the Win32 shell.
 *
 * The worker owns its backend, event, lock and completed-snapshot handoff.
 * Presentation code sees only request/take/destroy operations. If Windows
 * cannot cancel an in-flight native scan during shutdown, reference counting
 * retains this small worker object until the scan returns instead of retaining
 * the complete Win32 presentation state.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_backend.h"

#ifndef _WIN32
#error "process_backend_windows_worker.c is a Windows-only translation unit"
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>

#define LSM_WINDOWS_PROCESS_WORKER_SHUTDOWN_MS 500U

struct LsmWindowsProcessWorker {
    HANDLE thread;
    HANDLE request_event;
    CRITICAL_SECTION lock;
    bool lock_initialised;
    bool stop_requested;
    bool request_pending;
    bool backend_failed;
    unsigned requested_flags;
    LsmProcessInfo *completed;
    size_t completed_count;
    bool completed_ready;
    volatile LONG references;
};

static void windows_process_worker_release(LsmWindowsProcessWorker *worker)
{
    if (!worker || InterlockedDecrement(&worker->references) != 0L)
        return;

    lsm_process_list_free(worker->completed);
    worker->completed = NULL;
    if (worker->request_event) {
        CloseHandle(worker->request_event);
        worker->request_event = NULL;
    }
    if (worker->thread) {
        CloseHandle(worker->thread);
        worker->thread = NULL;
    }
    if (worker->lock_initialised) {
        DeleteCriticalSection(&worker->lock);
        worker->lock_initialised = false;
    }
    free(worker);
}

static DWORD WINAPI windows_process_worker_main(LPVOID user_data)
{
    LsmWindowsProcessWorker *worker = user_data;
    if (!worker) return 0U;

    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) {
        EnterCriticalSection(&worker->lock);
        worker->backend_failed = true;
        worker->request_pending = false;
        LeaveCriticalSection(&worker->lock);
        windows_process_worker_release(worker);
        return 0U;
    }

    for (;;) {
        (void)WaitForSingleObject(worker->request_event, INFINITE);

        EnterCriticalSection(&worker->lock);
        if (worker->stop_requested) {
            LeaveCriticalSection(&worker->lock);
            break;
        }
        if (!worker->request_pending) {
            LeaveCriticalSection(&worker->lock);
            continue;
        }
        const unsigned flags = worker->requested_flags;
        worker->request_pending = false;
        LeaveCriticalSection(&worker->lock);

        LsmProcessInfo *processes = NULL;
        const size_t count = lsm_process_scan(backend, &processes, flags);

        EnterCriticalSection(&worker->lock);
        if (worker->stop_requested) {
            LeaveCriticalSection(&worker->lock);
            lsm_process_list_free(processes);
            break;
        }
        if (processes) {
            lsm_process_list_free(worker->completed);
            worker->completed = processes;
            worker->completed_count = count;
            worker->completed_ready = true;
        }
        LeaveCriticalSection(&worker->lock);
    }

    lsm_process_backend_destroy(backend);
    windows_process_worker_release(worker);
    return 0U;
}

LsmWindowsProcessWorker *lsm_windows_process_worker_create(void)
{
    LsmWindowsProcessWorker *worker = calloc(1U, sizeof(*worker));
    if (!worker) return NULL;

    InitializeCriticalSection(&worker->lock);
    worker->lock_initialised = true;
    worker->request_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!worker->request_event) {
        worker->references = 1L;
        windows_process_worker_release(worker);
        return NULL;
    }

    worker->request_pending = true;
    worker->requested_flags = LSM_PROCESS_SCAN_NONE;
    worker->references = 2L; /* caller + worker thread */
    worker->thread = CreateThread(
        NULL, 0U, windows_process_worker_main, worker, 0U, NULL);
    if (!worker->thread) {
        worker->references = 1L;
        windows_process_worker_release(worker);
        return NULL;
    }

    (void)SetEvent(worker->request_event);
    return worker;
}

bool lsm_windows_process_worker_request(
    LsmWindowsProcessWorker *worker, unsigned scan_flags)
{
    if (!worker || !worker->lock_initialised) return false;

    bool accepted = false;
    EnterCriticalSection(&worker->lock);
    if (!worker->stop_requested && !worker->backend_failed) {
        worker->requested_flags = scan_flags;
        worker->request_pending = true;
        accepted = true;
    }
    LeaveCriticalSection(&worker->lock);

    if (accepted)
        (void)SetEvent(worker->request_event);
    return accepted;
}

bool lsm_windows_process_worker_take(
    LsmWindowsProcessWorker *worker, LsmProcessInfo **processes,
    size_t *count)
{
    if (!worker || !processes || !count || !worker->lock_initialised)
        return false;
    *processes = NULL;
    *count = 0U;

    EnterCriticalSection(&worker->lock);
    if (!worker->completed_ready) {
        LeaveCriticalSection(&worker->lock);
        return false;
    }
    *processes = worker->completed;
    *count = worker->completed_count;
    worker->completed = NULL;
    worker->completed_count = 0U;
    worker->completed_ready = false;
    LeaveCriticalSection(&worker->lock);
    return true;
}

void lsm_windows_process_worker_destroy(LsmWindowsProcessWorker *worker)
{
    if (!worker || !worker->lock_initialised) return;

    EnterCriticalSection(&worker->lock);
    worker->stop_requested = true;
    worker->request_pending = false;
    LeaveCriticalSection(&worker->lock);
    if (worker->request_event)
        (void)SetEvent(worker->request_event);

    if (worker->thread) {
        (void)CancelSynchronousIo(worker->thread);
        (void)WaitForSingleObject(
            worker->thread, LSM_WINDOWS_PROCESS_WORKER_SHUTDOWN_MS);
    }

    /*
     * Drop only the caller reference. If a native query ignored cancellation,
     * its thread reference keeps this worker alive until the query returns;
     * no presentation object is retained and no UI memory can be dereferenced.
     */
    windows_process_worker_release(worker);
}
