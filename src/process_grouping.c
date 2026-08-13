// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_grouping.c
 * @brief Overflow-safe application-group metric aggregation.
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

static double positive_finite_sum(double total, double value)
{
    if (!isfinite(value) || value <= 0.0) return total;
    return value <= DBL_MAX - total ? total + value : DBL_MAX;
}


void lsm_process_group_metrics_add(LsmProcessGroupMetrics *metrics,
                                   const LsmProcessInfo *process)
{
    if (!metrics || !process) return;
    const bool first = metrics->process_count == 0U;
    metrics->cpu_percent =
        positive_finite_sum(metrics->cpu_percent, process->cpu_percent);
    if (UINT64_MAX - metrics->memory_bytes < process->rss_bytes)
        metrics->memory_bytes = UINT64_MAX;
    else
        metrics->memory_bytes += process->rss_bytes;
    metrics->disk_bytes_per_sec = positive_finite_sum(
        metrics->disk_bytes_per_sec, process->read_bytes_per_sec);
    metrics->disk_bytes_per_sec = positive_finite_sum(
        metrics->disk_bytes_per_sec, process->write_bytes_per_sec);
    if (process->gpu_available) {
        if (process->gpu_engine[0] &&
            (!metrics->gpu_available ||
             process->gpu_percent > metrics->gpu_engine_peak)) {
            metrics->gpu_engine_peak = process->gpu_percent;
            (void)snprintf(metrics->gpu_engine,
                           sizeof(metrics->gpu_engine), "%s",
                           process->gpu_engine);
        }
        metrics->gpu_percent = positive_finite_sum(
            metrics->gpu_percent, process->gpu_percent);
        metrics->gpu_available = true;
    }
    metrics->all_stopped = first
        ? strcmp(process->state, "Stopped") == 0
        : metrics->all_stopped && strcmp(process->state, "Stopped") == 0;
    metrics->all_efficient = first
        ? process->efficiency_mode
        : metrics->all_efficient && process->efficiency_mode;
    if (metrics->process_count < SIZE_MAX) metrics->process_count++;
}
