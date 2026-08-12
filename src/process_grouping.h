// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_grouping.h
 * @brief Overflow-safe application-group metric aggregation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROCESS_GROUPING_H
#define LINUX_SYSTEM_MONITOR_PROCESS_GROUPING_H

#include "monitor_types.h"

/** Resource and state totals for one friendly application/process group. */
typedef struct {
    double cpu_percent;       /**< Sum of valid total-computer CPU shares. */
    uint64_t memory_bytes;    /**< Saturating sum of resident memory. */
    double disk_bytes_per_sec;/**< Saturating sum of valid read/write rates. */
    double gpu_percent;       /**< Sum of readable peak-engine process usage. */
    double gpu_engine_peak;   /**< Highest contributing process GPU share. */
    char gpu_engine[256];     /**< Engine belonging to the peak contributor. */
    bool gpu_available;       /**< At least one process exposed DRM counters. */
    size_t process_count;     /**< Number of incorporated processes. */
    bool all_stopped;         /**< True when every incorporated row is stopped. */
    bool all_efficient;       /**< True when every row has efficiency priority. */
} LsmProcessGroupMetrics;

/**
 * Reset group metrics to an empty identity value.
 *
 * @param [out] metrics Aggregate to initialise; NULL is ignored.
 */
void lsm_process_group_metrics_init(LsmProcessGroupMetrics *metrics);

/**
 * Incorporate one process into an aggregate using saturating arithmetic.
 *
 * Non-finite or negative rate values are treated as unavailable and do not
 * contaminate the remaining group. The process count and resident-memory sum
 * saturate rather than wrapping.
 *
 * @param [in,out] metrics Aggregate receiving the process.
 * @param [in] process Process snapshot row to incorporate.
 */
void lsm_process_group_metrics_add(LsmProcessGroupMetrics *metrics,
                                   const LsmProcessInfo *process);

#endif
