// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_grouping_smoke.c
 * @brief Grouped-process arithmetic and state regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_grouping.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmProcessGroupMetrics metrics = {0};
    LsmProcessInfo first = {
        .cpu_percent = 12.5,
        .rss_bytes = 28ULL * 1024ULL * 1024ULL * 1024ULL,
        .read_bytes_per_sec = 1024.0,
        .write_bytes_per_sec = 1048576.0,
        .gpu_percent = 18.0,
        .gpu_available = true,
        .efficiency_mode = true
    };
    strcpy(first.state, "Stopped");
    strcpy(first.gpu_engine, "render");
    lsm_process_group_metrics_add(&metrics, &first);
    if (metrics.process_count != 1U ||
        fabs(metrics.cpu_percent - 12.5) > 0.0001 ||
        metrics.memory_bytes != 30064771072ULL ||
        fabs(metrics.disk_bytes_per_sec - 1049600.0) > 0.0001 ||
        fabs(metrics.gpu_percent - 18.0) > 0.0001 ||
        !metrics.gpu_available ||
        strcmp(metrics.gpu_engine, "render") != 0 ||
        !metrics.all_stopped || !metrics.all_efficient)
        return 1;

    LsmProcessInfo second = {
        .cpu_percent = NAN,
        .rss_bytes = UINT64_MAX,
        .read_bytes_per_sec = INFINITY,
        .write_bytes_per_sec = -5.0,
        .efficiency_mode = false
    };
    strcpy(second.state, "Running");
    lsm_process_group_metrics_add(&metrics, &second);
    if (metrics.process_count != 2U ||
        fabs(metrics.cpu_percent - 12.5) > 0.0001 ||
        metrics.memory_bytes != UINT64_MAX ||
        fabs(metrics.disk_bytes_per_sec - 1049600.0) > 0.0001 ||
        metrics.all_stopped || metrics.all_efficient)
        return 2;

    second.gpu_available = true;
    second.gpu_percent = 42.0;
    strcpy(second.gpu_engine, "copy");
    lsm_process_group_metrics_add(&metrics, &second);
    if (fabs(metrics.gpu_percent - 60.0) > 0.0001 ||
        strcmp(metrics.gpu_engine, "copy") != 0)
        return 3;

    metrics.disk_bytes_per_sec = DBL_MAX;
    second.read_bytes_per_sec = 1.0;
    second.write_bytes_per_sec = 1.0;
    lsm_process_group_metrics_add(&metrics, &second);
    if (metrics.disk_bytes_per_sec != DBL_MAX) return 4;
    puts("Grouped process arithmetic, saturation and state policy passed.");
    return 0;
}
