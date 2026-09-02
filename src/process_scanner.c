// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_scanner.c
 * @brief Coalescing background process-snapshot worker.
 *
 * The worker is deliberately self-owned after start. Shutdown sets a stop flag
 * and releases the application reference without joining. If procfs or NSS is
 * pathological, the operating system can still terminate the process cleanly
 * while the detached worker retains all state it may still touch.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_scanner.h"

#include "process_backend.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

struct LsmProcessScanner {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    LsmProcessBackend *backend;
    LsmProcessInfo *completed;
    size_t completed_count;
    unsigned requested_flags;
    atomic_uint references;
    bool request_pending;
    bool result_ready;
    bool stop_requested;
};

static void scanner_release(LsmProcessScanner *scanner)
{
    if (!scanner ||
        atomic_fetch_sub_explicit(&scanner->references, 1U,
                                  memory_order_acq_rel) != 1U)
        return;
    lsm_process_list_free(scanner->completed);
    lsm_process_backend_destroy(scanner->backend);
    (void)pthread_cond_destroy(&scanner->condition);
    (void)pthread_mutex_destroy(&scanner->mutex);
    free(scanner);
}

static void *scanner_thread_main(void *user_data)
{
    LsmProcessScanner *scanner = user_data;
    if (!scanner) return NULL;

    for (;;) {
        unsigned flags = LSM_PROCESS_SCAN_NONE;
        (void)pthread_mutex_lock(&scanner->mutex);
        while (!scanner->request_pending && !scanner->stop_requested)
            (void)pthread_cond_wait(&scanner->condition, &scanner->mutex);
        if (scanner->stop_requested) {
            (void)pthread_mutex_unlock(&scanner->mutex);
            break;
        }
        flags = scanner->requested_flags;
        scanner->request_pending = false;
        (void)pthread_mutex_unlock(&scanner->mutex);

        LsmProcessInfo *processes = NULL;
        const size_t count = lsm_process_scan(
            scanner->backend, &processes, flags);

        (void)pthread_mutex_lock(&scanner->mutex);
        if (scanner->stop_requested) {
            (void)pthread_mutex_unlock(&scanner->mutex);
            lsm_process_list_free(processes);
            break;
        }
        lsm_process_list_free(scanner->completed);
        scanner->completed = processes;
        scanner->completed_count = count;
        scanner->result_ready = true;
        (void)pthread_mutex_unlock(&scanner->mutex);
    }

    scanner_release(scanner);
    return NULL;
}

LsmProcessScanner *lsm_process_scanner_create(void)
{
    LsmProcessScanner *scanner = calloc(1U, sizeof(*scanner));
    if (!scanner) return NULL;
    if (pthread_mutex_init(&scanner->mutex, NULL) != 0) {
        free(scanner);
        return NULL;
    }
    if (pthread_cond_init(&scanner->condition, NULL) != 0) {
        (void)pthread_mutex_destroy(&scanner->mutex);
        free(scanner);
        return NULL;
    }
    scanner->backend = lsm_process_backend_create();
    if (!scanner->backend) {
        (void)pthread_cond_destroy(&scanner->condition);
        (void)pthread_mutex_destroy(&scanner->mutex);
        free(scanner);
        return NULL;
    }

    scanner->request_pending = true;
    scanner->requested_flags = LSM_PROCESS_SCAN_NONE;
    atomic_init(&scanner->references, 2U);
    const int thread_error = pthread_create(
        &scanner->thread, NULL, scanner_thread_main, scanner);
    if (thread_error != 0) {
        atomic_store_explicit(&scanner->references, 1U, memory_order_release);
        scanner_release(scanner);
        return NULL;
    }
    if (pthread_detach(scanner->thread) != 0) {
        (void)pthread_mutex_lock(&scanner->mutex);
        scanner->stop_requested = true;
        (void)pthread_cond_signal(&scanner->condition);
        (void)pthread_mutex_unlock(&scanner->mutex);
        (void)pthread_join(scanner->thread, NULL);
        atomic_store_explicit(&scanner->references, 1U, memory_order_release);
        scanner_release(scanner);
        return NULL;
    }
    return scanner;
}

bool lsm_process_scanner_request(LsmProcessScanner *scanner,
                                 unsigned scan_flags)
{
    if (!scanner) return false;
    (void)pthread_mutex_lock(&scanner->mutex);
    if (scanner->stop_requested) {
        (void)pthread_mutex_unlock(&scanner->mutex);
        return false;
    }
    scanner->requested_flags = scan_flags;
    scanner->request_pending = true;
    (void)pthread_cond_signal(&scanner->condition);
    (void)pthread_mutex_unlock(&scanner->mutex);
    return true;
}

bool lsm_process_scanner_take(LsmProcessScanner *scanner,
                              LsmProcessInfo **processes,
                              size_t *count)
{
    if (!scanner || !processes || !count) return false;
    *processes = NULL;
    *count = 0U;
    (void)pthread_mutex_lock(&scanner->mutex);
    if (!scanner->result_ready) {
        (void)pthread_mutex_unlock(&scanner->mutex);
        return false;
    }
    *processes = scanner->completed;
    *count = scanner->completed_count;
    scanner->completed = NULL;
    scanner->completed_count = 0U;
    scanner->result_ready = false;
    (void)pthread_mutex_unlock(&scanner->mutex);
    return true;
}

void lsm_process_scanner_destroy(LsmProcessScanner *scanner)
{
    if (!scanner) return;
    (void)pthread_mutex_lock(&scanner->mutex);
    scanner->stop_requested = true;
    scanner->request_pending = false;
    (void)pthread_cond_signal(&scanner->condition);
    (void)pthread_mutex_unlock(&scanner->mutex);
    scanner_release(scanner);
}
