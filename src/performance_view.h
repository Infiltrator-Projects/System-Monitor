// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_view.h
 * @brief Platform-neutral formatted CPU and memory Performance view models.
 *
 * Native collectors publish plain monitor data. This layer converts that data
 * into one canonical user-facing representation before GTK or Win32 renders it,
 * preventing operating-system front ends from independently deciding whether a
 * value is available or how it is formatted.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PERFORMANCE_VIEW_H
#define INFILTRATOR_SYSTEM_MONITOR_PERFORMANCE_VIEW_H

#include "monitor_types.h"
#include "presentation_contract.h"

#define LSM_PERFORMANCE_VIEW_VALUE_LEN 128
#define LSM_PERFORMANCE_VIEW_RAIL_LEN 128
#define LSM_MEMORY_MODULE_VIEW_LEN 8192

/** Canonical formatted CPU presentation consumed by every native renderer. */
typedef struct {
    char subtitle[LSM_NAME_LEN];
    char rail_value[LSM_PERFORMANCE_VIEW_RAIL_LEN];
    char metrics[LSM_CPU_METRIC_COUNT][LSM_PERFORMANCE_VIEW_VALUE_LEN];
    char details[LSM_CPU_DETAIL_COUNT][LSM_PERFORMANCE_VIEW_VALUE_LEN];
} LsmCpuPerformanceView;

/** Canonical formatted memory presentation consumed by every native renderer. */
typedef struct {
    char subtitle[LSM_PERFORMANCE_VIEW_VALUE_LEN];
    char rail_value[LSM_PERFORMANCE_VIEW_RAIL_LEN];
    char metrics[LSM_MEMORY_METRIC_COUNT][LSM_PERFORMANCE_VIEW_VALUE_LEN];
    char details[LSM_MEMORY_DETAIL_COUNT][LSM_MEMORY_MODULE_VIEW_LEN];
} LsmMemoryPerformanceView;

/**
 * Project one monitor snapshot into the canonical CPU presentation.
 *
 * @param monitor Current platform-neutral monitoring snapshot.
 * @param view Caller-owned output that is fully initialised by the function.
 */
void lsm_cpu_performance_view(const LsmMonitor *monitor,
                              LsmCpuPerformanceView *view);

/**
 * Project one monitor snapshot into the canonical memory presentation.
 *
 * @param monitor Current platform-neutral monitoring snapshot.
 * @param view Caller-owned output that is fully initialised by the function.
 */
void lsm_memory_performance_view(const LsmMonitor *monitor,
                                 LsmMemoryPerformanceView *view);

#endif
