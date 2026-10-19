// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file scanner_coalescing_smoke.c
 * @brief Deterministic last-request-wins validation of scanner scheduling.
 *
 * A controlled fake backend blocks each scan at its entry boundary. All
 * scanner threads, mutexes, snapshot handoffs and request handling remain
 * production code; only operating-system sampling is replaced. This protects
 * a concurrency invariant without depending on procfs timing or live PIDs.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_backend.h"
#include "process_scanner.h"
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define FIXTURE_MAX_SCANS 8U
static pthread_mutex_t fixture_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fixture_cond = PTHREAD_COND_INITIALIZER;
static unsigned scan_count;
static unsigned released_count;
static unsigned observed_flags[FIXTURE_MAX_SCANS];
static bool overflow;

LsmProcessBackend *lsm_process_backend_create(void)
{
    return (LsmProcessBackend *)malloc(1U);
}

void lsm_process_backend_destroy(LsmProcessBackend *backend)
{
    free(backend);
}

size_t lsm_process_scan(LsmProcessBackend *backend,
                        LsmProcessInfo **out_processes, unsigned flags)
{
    if (!backend || !out_processes) return 0U;
    pthread_mutex_lock(&fixture_lock);
    const unsigned number = ++scan_count;
    if (number > FIXTURE_MAX_SCANS)
        overflow = true;
    else
        observed_flags[number - 1U] = flags;
    pthread_cond_broadcast(&fixture_cond);
    while (released_count < number)
        pthread_cond_wait(&fixture_cond, &fixture_lock);
    pthread_mutex_unlock(&fixture_lock);
    /* Non-NULL with zero rows represents a valid completed scan. */
    *out_processes = calloc(1U, sizeof(**out_processes));
    return 0U;
}

void lsm_process_list_free(LsmProcessInfo *processes)
{
    free(processes);
}

static bool wait_for_count(unsigned wanted, long timeout_ms)
{
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) return false;
    deadline.tv_sec += timeout_ms / 1000L;
    deadline.tv_nsec += (timeout_ms % 1000L) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    pthread_mutex_lock(&fixture_lock);
    while (scan_count < wanted && !overflow) {
        if (pthread_cond_timedwait(&fixture_cond, &fixture_lock,
                                   &deadline) == ETIMEDOUT) break;
    }
    const bool completed = !overflow && scan_count >= wanted;
    pthread_mutex_unlock(&fixture_lock);
    return completed;
}

static unsigned scan_flags_at(unsigned index)
{
    pthread_mutex_lock(&fixture_lock);
    const unsigned flags = index < scan_count && index < FIXTURE_MAX_SCANS
        ? observed_flags[index] : 0U;
    pthread_mutex_unlock(&fixture_lock);
    return flags;
}

static void release_through(unsigned count)
{
    pthread_mutex_lock(&fixture_lock);
    if (count > released_count) released_count = count;
    pthread_cond_broadcast(&fixture_cond);
    pthread_mutex_unlock(&fixture_lock);
}

static int fail(LsmProcessScanner *scanner, const char *reason)
{
    release_through(FIXTURE_MAX_SCANS + 1U);
    lsm_process_scanner_destroy(scanner);
    fprintf(stderr, "Process scanner coalescing: %s\n", reason);
    return EXIT_FAILURE;
}

int main(void)
{
    const unsigned base = LSM_PROCESS_SCAN_EXECUTABLE;
    const unsigned gpu = base | LSM_PROCESS_SCAN_GPU;
    const unsigned handles = base | LSM_PROCESS_SCAN_HANDLE_COUNT;
    LsmProcessScanner *scanner = lsm_process_scanner_create();
    if (!scanner) return EXIT_FAILURE;
    if (!wait_for_count(1U, 3000L) || scan_flags_at(0U) != base)
        return fail(scanner, "initial scan did not start");

    /* GPU was queued, then the latest request returned to the active flags.
     * The in-flight scan must satisfy it without performing a stale follow-up. */
    if (!lsm_process_scanner_request(scanner, gpu) ||
        !lsm_process_scanner_request(scanner, base))
        return fail(scanner, "unable to replace initial request");
    release_through(1U);
    if (wait_for_count(2U, 200L))
        return fail(scanner, "obsolete GPU scan was executed");

    if (!lsm_process_scanner_request(scanner, gpu) ||
        !wait_for_count(2U, 3000L) || scan_flags_at(1U) != gpu)
        return fail(scanner, "distinct GPU request was not executed");

    /* Only the latest distinct flag set may survive this blocked scan. */
    if (!lsm_process_scanner_request(scanner, base) ||
        !lsm_process_scanner_request(scanner, handles))
        return fail(scanner, "unable to replace pending flags");
    release_through(2U);
    if (!wait_for_count(3U, 3000L) || scan_flags_at(2U) != handles)
        return fail(scanner, "newest pending enrichment was lost");

    release_through(3U);
    lsm_process_scanner_destroy(scanner);
    puts("Last-request-wins scanner coalescing passed.");
    return EXIT_SUCCESS;
}
