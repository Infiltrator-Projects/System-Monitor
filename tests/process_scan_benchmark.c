// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_scan_benchmark.c
 * @brief Informational benchmark for the retained native process scanner.
 *
 * This developer benchmark deliberately has no pass/fail timing threshold:
 * procfs cost depends on process count, storage pressure, namespaces and host
 * scheduling. It reports a stable workload so release engineers can compare
 * revisions on the same machine without turning environmental noise into a
 * portability failure.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_backend.h"

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define LSM_BENCHMARK_WARMUP_SCANS 5U
#define LSM_BENCHMARK_MEASURED_SCANS 100U

static double monotonic_seconds(void)
{
    struct timespec time_value;
    if (clock_gettime(CLOCK_MONOTONIC, &time_value) != 0) return 0.0;
    return (double)time_value.tv_sec +
           (double)time_value.tv_nsec / 1000000000.0;
}

static bool run_scan(LsmProcessBackend *backend, size_t *row_count)
{
    LsmProcessInfo *processes = NULL;
    const size_t count = lsm_process_scan(
        backend, &processes, LSM_PROCESS_SCAN_NONE);
    lsm_process_list_free(processes);
    if (row_count) *row_count = count;
    return processes != NULL || count > 0U;
}

int main(void)
{
    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) {
        fputs("Unable to create process backend.\n", stderr);
        return EXIT_FAILURE;
    }

    size_t rows = 0U;
    for (unsigned index = 0U; index < LSM_BENCHMARK_WARMUP_SCANS; index++)
        (void)run_scan(backend, &rows);

    const double started = monotonic_seconds();
    size_t total_rows = 0U;
    for (unsigned index = 0U; index < LSM_BENCHMARK_MEASURED_SCANS; index++) {
        (void)run_scan(backend, &rows);
        total_rows += rows;
    }
    const double elapsed = monotonic_seconds() - started;
    lsm_process_backend_destroy(backend);

    if (!(elapsed > 0.0)) {
        fputs("Monotonic benchmark clock did not advance.\n", stderr);
        return EXIT_FAILURE;
    }

    printf("Process scan benchmark: scans=%u, mean rows=%.1f, "
           "total=%.3f ms, mean=%.3f ms/scan\n",
           LSM_BENCHMARK_MEASURED_SCANS,
           (double)total_rows / (double)LSM_BENCHMARK_MEASURED_SCANS,
           elapsed * 1000.0,
           elapsed * 1000.0 / (double)LSM_BENCHMARK_MEASURED_SCANS);
    return EXIT_SUCCESS;
}
