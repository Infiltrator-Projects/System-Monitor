// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file gpu_metrics.h
 * @brief Platform-neutral GPU metric selection for Performance graphs.
 *
 * The collector backend publishes capability/value pairs through LsmGpuInfo.
 * This module turns those pairs into named graph metrics without exposing the
 * operating-system or driver mechanism that produced them.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_GPU_METRICS_H
#define LINUX_SYSTEM_MONITOR_GPU_METRICS_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>

#define LSM_GPU_GRAPH_SLOT_COUNT 4U

/** GPU utilisation series that may be selected for an engine graph. */
typedef enum {
    LSM_GPU_METRIC_OVERALL = 0,
    LSM_GPU_METRIC_RENDER,
    LSM_GPU_METRIC_COMPUTE,
    LSM_GPU_METRIC_VIDEO,
    LSM_GPU_METRIC_VIDEO_ENHANCE,
    LSM_GPU_METRIC_COPY,
    LSM_GPU_METRIC_MEMORY_BUSY,
    LSM_GPU_METRIC_ENCODER,
    LSM_GPU_METRIC_DECODER,
    LSM_GPU_METRIC_COUNT
} LsmGpuMetric;

/**
 * Return the stable user-facing name for a graph metric.
 *
 * @param [in] metric Metric whose label is required.
 * @return Static display string that remains valid for the process lifetime.
 */
const char *lsm_gpu_metric_name(LsmGpuMetric metric);
/**
 * Return whether the current snapshot contains a meaningful sample.
 *
 * @param [in] gpu Current GPU snapshot, or NULL.
 * @param [in] metric Metric whose capability flag is queried.
 * @return true when the backend published a valid sample for @p metric.
 */
bool lsm_gpu_metric_available(const LsmGpuInfo *gpu, LsmGpuMetric metric);
/**
 * Return the current percentage value, clamped to the graph range.
 *
 * @param [in] gpu Current GPU snapshot, or NULL.
 * @param [in] metric Metric whose current value is required.
 * @return Percentage from 0 through 100, or zero when unavailable.
 */
double lsm_gpu_metric_value(const LsmGpuInfo *gpu, LsmGpuMetric metric);
/**
 * Return whether at least one independently sampled engine is available.
 *
 * @param [in] gpu Current GPU snapshot, or NULL.
 * @return true when at least one detailed engine counter is available.
 */
bool lsm_gpu_has_engine_metrics(const LsmGpuInfo *gpu);
/**
 * Choose useful initial metrics for the four Task-Manager-style graph slots.
 *
 * Available dedicated engines are preferred, then overall utilisation fills
 * any remaining slots. The output is always fully initialised when capacity is
 * non-zero, allowing the presentation layer to stay independent of backend
 * type or driver name.
 *
 * @param [in] gpu Current GPU snapshot, or NULL.
 * @param [out] metrics Destination array receiving selected metrics.
 * @param [in] capacity Number of entries available in @p metrics.
 */
void lsm_gpu_default_metrics(const LsmGpuInfo *gpu, LsmGpuMetric *metrics,
                             size_t capacity);
/**
 * List only graph metrics currently supplied by the active GPU backend.
 *
 * Detailed engine metrics retain the same stable ordering used for default
 * graph selection. Overall utilisation is appended when available. Unsupported
 * backend-specific metrics are omitted entirely, preventing the presentation
 * layer from offering selectors that can only display N/A.
 *
 * @param [in] gpu Current GPU snapshot, or NULL.
 * @param [out] metrics Optional destination array receiving metric identifiers.
 * @param [in] capacity Number of entries available in @p metrics.
 * @return Total number of selectable metrics exposed by the snapshot. When the
 *         return value exceeds @p capacity, only the first @p capacity entries
 *         are written.
 */
size_t lsm_gpu_selectable_metrics(const LsmGpuInfo *gpu,
                                  LsmGpuMetric *metrics, size_t capacity);

#endif
