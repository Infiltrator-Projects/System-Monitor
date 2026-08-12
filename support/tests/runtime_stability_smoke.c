// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file runtime_stability_smoke.c
 * @brief Repeated-refresh, lifecycle, descriptor and memory-growth checks.
 *
 * The test exercises the complete native monitor backend as the same single
 * unprivileged process used by the GUI. It deliberately forces topology scans,
 * performs process scans, then repeats monitor creation and destruction to
 * catch file-descriptor leaks, stranded worker threads and unbounded resident
 * memory growth.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "monitor.h"
#include "process_backend.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LSM_STABILITY_WARMUP_UPDATES 20U
#define LSM_STABILITY_MEASURED_UPDATES 120U
#define LSM_STABILITY_LIFECYCLE_CYCLES 6U
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
#    define LSM_STABILITY_ADDRESS_SANITIZED 1
#  endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(LSM_STABILITY_ADDRESS_SANITIZED)
#  define LSM_STABILITY_ADDRESS_SANITIZED 1
#endif
#ifdef LSM_STABILITY_ADDRESS_SANITIZED
#  define LSM_STABILITY_RSS_ALLOWANCE_BYTES (64ULL * 1024ULL * 1024ULL)
#else
#  define LSM_STABILITY_RSS_ALLOWANCE_BYTES (16ULL * 1024ULL * 1024ULL)
#endif
#define LSM_STABILITY_FD_ALLOWANCE 2U
#define LSM_STABILITY_THREAD_ALLOWANCE 1U

static size_t count_directory_entries(const char *path)
{
    DIR *directory = opendir(path);
    if (!directory) return SIZE_MAX;
    size_t count = 0U;
    struct dirent *entry = NULL;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        count++;
    }
    closedir(directory);
    return count;
}

static uint64_t resident_bytes(void)
{
    FILE *file = fopen("/proc/self/status", "r");
    if (!file) return 0U;
    char line[256];
    uint64_t rss_kb = 0U;
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "VmRSS: %" SCNu64 " kB", &rss_kb) == 1) break;
    }
    fclose(file);
    return rss_kb * 1024U;
}

static double timespec_seconds(const struct timespec *value)
{
    return (double)value->tv_sec + (double)value->tv_nsec / 1000000000.0;
}

static void sleep_milliseconds(long milliseconds)
{
    struct timespec request = {
        .tv_sec = milliseconds / 1000L,
        .tv_nsec = (milliseconds % 1000L) * 1000000L
    };
    while (nanosleep(&request, &request) != 0 && errno == EINTR) {
    }
}

static bool update_and_scan(LsmMonitor *monitor, LsmProcessBackend *backend,
                            unsigned iteration)
{
    if (iteration % 20U == 0U) lsm_monitor_request_topology_refresh(monitor);
    if (!lsm_monitor_update(monitor)) return false;
    if (iteration % 10U != 0U) return true;

    LsmProcessInfo *processes = NULL;
    const size_t count = lsm_process_scan(
        backend, &processes, LSM_PROCESS_SCAN_NONE);
    lsm_monitor_set_process_totals(monitor, processes, count);
    lsm_process_list_free(processes);
    return count > 0U;
}

static bool lifecycle_check(size_t initial_fds, size_t initial_threads)
{
    for (unsigned cycle = 0U; cycle < LSM_STABILITY_LIFECYCLE_CYCLES; cycle++) {
        LsmMonitor *monitor = calloc(1U, sizeof(*monitor));
        if (!monitor) return false;
        if (!lsm_monitor_init(monitor)) {
            free(monitor);
            return false;
        }
        for (unsigned update = 0U; update < 3U; update++) {
            if (update == 1U) lsm_monitor_request_topology_refresh(monitor);
            sleep_milliseconds(5L);
            if (!lsm_monitor_update(monitor)) {
                lsm_monitor_destroy(monitor);
                free(monitor);
                return false;
            }
        }
        lsm_monitor_destroy(monitor);
        free(monitor);
        sleep_milliseconds(25L);

        const size_t fds = count_directory_entries("/proc/self/fd");
        const size_t threads = count_directory_entries("/proc/self/task");
        if (fds == SIZE_MAX || threads == SIZE_MAX ||
            fds > initial_fds + LSM_STABILITY_FD_ALLOWANCE ||
            threads > initial_threads + LSM_STABILITY_THREAD_ALLOWANCE) {
            fprintf(stderr,
                    "Lifecycle leak after cycle %u: fds=%zu (base=%zu), "
                    "threads=%zu (base=%zu)\n",
                    cycle + 1U, fds, initial_fds, threads, initial_threads);
            return false;
        }
    }
    return true;
}

int main(void)
{
    if (setenv("LSM_DISABLE_HARDWARE_BROKER", "1", 1) != 0) return 1;

    const size_t initial_fds = count_directory_entries("/proc/self/fd");
    const size_t initial_threads = count_directory_entries("/proc/self/task");
    if (initial_fds == SIZE_MAX || initial_threads == SIZE_MAX) return 2;

    LsmMonitor *monitor = calloc(1U, sizeof(*monitor));
    if (!monitor) return 3;
    if (!lsm_monitor_init(monitor)) {
        free(monitor);
        return 3;
    }
    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) {
        lsm_monitor_destroy(monitor);
        free(monitor);
        return 4;
    }

    for (unsigned index = 0U; index < LSM_STABILITY_WARMUP_UPDATES; index++) {
        sleep_milliseconds(5L);
        if (!update_and_scan(monitor, backend, index)) {
            lsm_process_backend_destroy(backend);
            lsm_monitor_destroy(monitor);
            free(monitor);
            return 5;
        }
    }

    const size_t baseline_fds = count_directory_entries("/proc/self/fd");
    const size_t baseline_threads = count_directory_entries("/proc/self/task");
    const uint64_t baseline_rss = resident_bytes();
    size_t peak_fds = baseline_fds;
    size_t peak_threads = baseline_threads;

    struct timespec cpu_start;
    struct timespec cpu_end;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_start) != 0) return 6;

    for (unsigned index = 0U; index < LSM_STABILITY_MEASURED_UPDATES; index++) {
        sleep_milliseconds(5L);
        if (!update_and_scan(monitor, backend, index)) return 7;
        const size_t fds = count_directory_entries("/proc/self/fd");
        const size_t threads = count_directory_entries("/proc/self/task");
        if (fds == SIZE_MAX || threads == SIZE_MAX) return 8;
        if (fds > peak_fds) peak_fds = fds;
        if (threads > peak_threads) peak_threads = threads;
    }
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_end) != 0) return 9;

    const size_t final_fds = count_directory_entries("/proc/self/fd");
    const size_t final_threads = count_directory_entries("/proc/self/task");
    const uint64_t final_rss = resident_bytes();
    const uint64_t rss_growth = final_rss > baseline_rss
        ? final_rss - baseline_rss : 0U;
    const double cpu_seconds = timespec_seconds(&cpu_end) - timespec_seconds(&cpu_start);
    const double milliseconds_per_update =
        1000.0 * cpu_seconds / (double)LSM_STABILITY_MEASURED_UPDATES;

    bool stable = final_fds <= baseline_fds + LSM_STABILITY_FD_ALLOWANCE &&
        final_threads <= baseline_threads + LSM_STABILITY_THREAD_ALLOWANCE &&
        rss_growth <= LSM_STABILITY_RSS_ALLOWANCE_BYTES &&
        milliseconds_per_update < 100.0;

    printf("Runtime stability: updates=%u, CPU=%.3f ms/update, "
           "fds=%zu->%zu (peak %zu), threads=%zu->%zu (peak %zu), "
           "RSS growth=%" PRIu64 " KB\n",
           LSM_STABILITY_MEASURED_UPDATES, milliseconds_per_update,
           baseline_fds, final_fds, peak_fds,
           baseline_threads, final_threads, peak_threads,
           rss_growth / 1024U);

    lsm_process_backend_destroy(backend);
    lsm_monitor_destroy(monitor);
    free(monitor);
    sleep_milliseconds(100L);

    const size_t shutdown_fds = count_directory_entries("/proc/self/fd");
    const size_t shutdown_threads = count_directory_entries("/proc/self/task");
    if (shutdown_fds == SIZE_MAX || shutdown_threads == SIZE_MAX) stable = false;
    else {
        printf("Post-shutdown runtime infrastructure: fds=%zu (initial %zu), "
               "threads=%zu (initial %zu)\n",
               shutdown_fds, initial_fds, shutdown_threads, initial_threads);
    }

    /* GLib/GIO may retain one process-wide worker and wake-up descriptors after
     * first use. That bounded infrastructure is not a lifecycle leak. Repeated
     * monitor construction must remain stable relative to this warmed state. */
    if (stable) stable = lifecycle_check(shutdown_fds, shutdown_threads);
    if (stable) puts("Runtime stability and lifecycle checks passed.");
    return stable ? 0 : 10;
}
