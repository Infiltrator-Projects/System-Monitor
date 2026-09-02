// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_scan_benchmark.c
 * @brief Informational benchmarks for base and enriched native process scans.
 *
 * These developer benchmarks deliberately have no pass/fail timing threshold:
 * procfs, NSS and DRM costs depend on process count, identity services, device
 * state, namespaces and host scheduling. Reporting both the ordinary snapshot
 * and the most expensive Details/Processes enrichment makes regressions visible
 * without turning environmental noise into a portability failure.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_backend.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define LSM_BENCHMARK_WARMUP_SCANS 5U
#define LSM_BENCHMARK_BASE_SCANS 100U
#define LSM_BENCHMARK_ENRICHED_SCANS 25U

static double monotonic_seconds(void)
{
    struct timespec time_value;
    if (clock_gettime(CLOCK_MONOTONIC, &time_value) != 0) return 0.0;
    return (double)time_value.tv_sec +
           (double)time_value.tv_nsec / 1000000000.0;
}

static size_t run_scan(LsmProcessBackend *backend, unsigned flags)
{
    LsmProcessInfo *processes = NULL;
    const size_t count = lsm_process_scan(backend, &processes, flags);
    lsm_process_list_free(processes);
    return count;
}

static bool benchmark_configuration(const char *name, unsigned flags,
                                    unsigned measured_scans)
{
    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) {
        fprintf(stderr, "Unable to create %s process backend.\n", name);
        return false;
    }

    for (unsigned index = 0U; index < LSM_BENCHMARK_WARMUP_SCANS; index++)
        (void)run_scan(backend, flags);

    const double started = monotonic_seconds();
    size_t total_rows = 0U;
    for (unsigned index = 0U; index < measured_scans; index++)
        total_rows += run_scan(backend, flags);
    const double elapsed = monotonic_seconds() - started;
    lsm_process_backend_destroy(backend);

    if (!(elapsed > 0.0)) {
        fprintf(stderr, "%s benchmark clock did not advance.\n", name);
        return false;
    }

    printf("Process scan benchmark [%s]: scans=%u, mean rows=%.1f, "
           "total=%.3f ms, mean=%.3f ms/scan\n",
           name, measured_scans,
           (double)total_rows / (double)measured_scans,
           elapsed * 1000.0,
           elapsed * 1000.0 / (double)measured_scans);
    return true;
}

int main(void)
{
    const unsigned enriched =
        LSM_PROCESS_SCAN_EXECUTABLE |
        LSM_PROCESS_SCAN_HANDLE_COUNT |
        LSM_PROCESS_SCAN_GPU;
    if (!benchmark_configuration(
            "base", LSM_PROCESS_SCAN_NONE, LSM_BENCHMARK_BASE_SCANS))
        return EXIT_FAILURE;
    if (!benchmark_configuration(
            "enriched", enriched, LSM_BENCHMARK_ENRICHED_SCANS))
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
