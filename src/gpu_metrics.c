// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file gpu_metrics.c
 * @brief Platform-neutral GPU metric selection for Performance graphs.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "gpu_metrics.h"

#include <math.h>

static const LsmGpuMetric detailed_metric_order[] = {
    LSM_GPU_METRIC_RENDER,
    LSM_GPU_METRIC_COMPUTE,
    LSM_GPU_METRIC_VIDEO,
    LSM_GPU_METRIC_VIDEO_ENHANCE,
    LSM_GPU_METRIC_COPY,
    LSM_GPU_METRIC_ENCODER,
    LSM_GPU_METRIC_DECODER,
    LSM_GPU_METRIC_MEMORY_BUSY
};

const char *lsm_gpu_metric_name(LsmGpuMetric metric)
{
    switch (metric) {
        case LSM_GPU_METRIC_OVERALL: return "Overall";
        case LSM_GPU_METRIC_RENDER: return "Render";
        case LSM_GPU_METRIC_COMPUTE: return "Compute";
        case LSM_GPU_METRIC_VIDEO: return "Video";
        case LSM_GPU_METRIC_VIDEO_ENHANCE: return "Video Enhance";
        case LSM_GPU_METRIC_COPY: return "Copy";
        case LSM_GPU_METRIC_MEMORY_BUSY: return "Memory Busy";
        case LSM_GPU_METRIC_ENCODER: return "Video Encode";
        case LSM_GPU_METRIC_DECODER: return "Video Decode";
        case LSM_GPU_METRIC_COUNT: break;
    }
    return "Unknown";
}

bool lsm_gpu_metric_available(const LsmGpuInfo *gpu, LsmGpuMetric metric)
{
    if (!gpu) return false;
    switch (metric) {
        case LSM_GPU_METRIC_OVERALL: return gpu->utilization_available;
        case LSM_GPU_METRIC_RENDER: return gpu->render_available;
        case LSM_GPU_METRIC_COMPUTE: return gpu->compute_available;
        case LSM_GPU_METRIC_VIDEO: return gpu->video_available;
        case LSM_GPU_METRIC_VIDEO_ENHANCE: return gpu->video_enhance_available;
        case LSM_GPU_METRIC_COPY: return gpu->copy_available;
        case LSM_GPU_METRIC_MEMORY_BUSY: return gpu->memory_busy_available;
        case LSM_GPU_METRIC_ENCODER: return gpu->encoder_available;
        case LSM_GPU_METRIC_DECODER: return gpu->decoder_available;
        case LSM_GPU_METRIC_COUNT: break;
    }
    return false;
}

static double raw_metric_value(const LsmGpuInfo *gpu, LsmGpuMetric metric)
{
    switch (metric) {
        case LSM_GPU_METRIC_OVERALL: return gpu->utilization_percent;
        case LSM_GPU_METRIC_RENDER: return gpu->render_percent;
        case LSM_GPU_METRIC_COMPUTE: return gpu->compute_percent;
        case LSM_GPU_METRIC_VIDEO: return gpu->video_percent;
        case LSM_GPU_METRIC_VIDEO_ENHANCE: return gpu->video_enhance_percent;
        case LSM_GPU_METRIC_COPY: return gpu->copy_percent;
        case LSM_GPU_METRIC_MEMORY_BUSY: return gpu->memory_busy_percent;
        case LSM_GPU_METRIC_ENCODER: return gpu->encoder_percent;
        case LSM_GPU_METRIC_DECODER: return gpu->decoder_percent;
        case LSM_GPU_METRIC_COUNT: break;
    }
    return 0.0;
}

double lsm_gpu_metric_value(const LsmGpuInfo *gpu, LsmGpuMetric metric)
{
    if (!lsm_gpu_metric_available(gpu, metric)) return 0.0;
    const double value = raw_metric_value(gpu, metric);
    if (!isfinite(value)) return 0.0;
    if (value < 0.0) return 0.0;
    if (value > 100.0) return 100.0;
    return value;
}


size_t lsm_gpu_selectable_metrics(const LsmGpuInfo *gpu,
                                  LsmGpuMetric *metrics, size_t capacity)
{
    if (!gpu) return 0U;

    size_t count = 0U;
    for (size_t index = 0U;
         index < sizeof(detailed_metric_order) / sizeof(detailed_metric_order[0]);
         index++) {
        const LsmGpuMetric metric = detailed_metric_order[index];
        if (!lsm_gpu_metric_available(gpu, metric)) continue;
        if (metrics && count < capacity) metrics[count] = metric;
        count++;
    }

    if (lsm_gpu_metric_available(gpu, LSM_GPU_METRIC_OVERALL)) {
        if (metrics && count < capacity) metrics[count] = LSM_GPU_METRIC_OVERALL;
        count++;
    }
    return count;
}

bool lsm_gpu_has_engine_metrics(const LsmGpuInfo *gpu)
{
    if (!gpu) return false;
    for (size_t index = 0U;
         index < sizeof(detailed_metric_order) / sizeof(detailed_metric_order[0]);
         index++) {
        if (lsm_gpu_metric_available(gpu, detailed_metric_order[index]))
            return true;
    }
    return false;
}

void lsm_gpu_default_metrics(const LsmGpuInfo *gpu, LsmGpuMetric *metrics,
                             size_t capacity)
{
    if (!metrics || capacity == 0U) return;

    size_t count = 0U;
    for (size_t index = 0U;
         index < sizeof(detailed_metric_order) / sizeof(detailed_metric_order[0]) &&
         count < capacity;
         index++) {
        if (lsm_gpu_metric_available(gpu, detailed_metric_order[index]))
            metrics[count++] = detailed_metric_order[index];
    }

    if (count < capacity && lsm_gpu_metric_available(gpu, LSM_GPU_METRIC_OVERALL))
        metrics[count++] = LSM_GPU_METRIC_OVERALL;

    const LsmGpuMetric fallback = count > 0U
        ? metrics[0] : LSM_GPU_METRIC_OVERALL;
    while (count < capacity) metrics[count++] = fallback;
}
