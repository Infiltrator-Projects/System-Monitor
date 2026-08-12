// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file gpu_metrics_smoke.c
 * @brief Validate backend-neutral GPU graph capability selection.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "gpu_metrics.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmGpuInfo gpu;
    memset(&gpu, 0, sizeof(gpu));
    LsmGpuMetric defaults[LSM_GPU_GRAPH_SLOT_COUNT];

    lsm_gpu_default_metrics(&gpu, defaults, LSM_GPU_GRAPH_SLOT_COUNT);
    for (size_t index = 0U; index < LSM_GPU_GRAPH_SLOT_COUNT; index++)
        if (defaults[index] != LSM_GPU_METRIC_OVERALL) return 1;
    if (lsm_gpu_has_engine_metrics(&gpu)) return 2;

    gpu.utilization_available = true;
    gpu.utilization_percent = 37.5;
    gpu.render_available = true;
    gpu.render_percent = 21.0;
    gpu.video_available = true;
    gpu.video_percent = 8.0;
    gpu.encoder_available = true;
    gpu.encoder_percent = 4.0;
    lsm_gpu_default_metrics(&gpu, defaults, LSM_GPU_GRAPH_SLOT_COUNT);
    if (defaults[0] != LSM_GPU_METRIC_RENDER) return 3;
    if (defaults[1] != LSM_GPU_METRIC_VIDEO) return 4;
    if (defaults[2] != LSM_GPU_METRIC_ENCODER) return 5;
    if (defaults[3] != LSM_GPU_METRIC_OVERALL) return 6;
    if (!lsm_gpu_has_engine_metrics(&gpu)) return 7;
    if (fabs(lsm_gpu_metric_value(&gpu, LSM_GPU_METRIC_RENDER) - 21.0) > 0.001)
        return 8;
    if (strcmp(lsm_gpu_metric_name(LSM_GPU_METRIC_DECODER), "Video Decode") != 0)
        return 9;

    gpu.render_percent = 150.0;
    if (lsm_gpu_metric_value(&gpu, LSM_GPU_METRIC_RENDER) != 100.0) return 10;
    gpu.render_percent = NAN;
    if (lsm_gpu_metric_value(&gpu, LSM_GPU_METRIC_RENDER) != 0.0) return 11;

    /* Intel PMU style: native engine classes must be selectable while NVML-only
     * encode/decode metrics remain absent. */
    memset(&gpu, 0, sizeof(gpu));
    gpu.utilization_available = true;
    gpu.render_available = true;
    gpu.compute_available = true;
    gpu.video_available = true;
    gpu.video_enhance_available = true;
    gpu.copy_available = true;
    LsmGpuMetric selectable[LSM_GPU_METRIC_COUNT];
    size_t selectable_count = lsm_gpu_selectable_metrics(
        &gpu, selectable, LSM_GPU_METRIC_COUNT);
    const LsmGpuMetric intel_expected[] = {
        LSM_GPU_METRIC_RENDER, LSM_GPU_METRIC_COMPUTE,
        LSM_GPU_METRIC_VIDEO, LSM_GPU_METRIC_VIDEO_ENHANCE,
        LSM_GPU_METRIC_COPY, LSM_GPU_METRIC_OVERALL
    };
    if (selectable_count != sizeof(intel_expected) / sizeof(intel_expected[0]))
        return 12;
    for (size_t index = 0U; index < selectable_count; index++)
        if (selectable[index] != intel_expected[index]) return 13;

    /* NVML style: encoder/decoder and memory-busy metrics remain available
     * without inventing Intel PMU engine classes. */
    memset(&gpu, 0, sizeof(gpu));
    gpu.utilization_available = true;
    gpu.memory_busy_available = true;
    gpu.encoder_available = true;
    gpu.decoder_available = true;
    selectable_count = lsm_gpu_selectable_metrics(
        &gpu, selectable, LSM_GPU_METRIC_COUNT);
    const LsmGpuMetric nvml_expected[] = {
        LSM_GPU_METRIC_ENCODER, LSM_GPU_METRIC_DECODER,
        LSM_GPU_METRIC_MEMORY_BUSY, LSM_GPU_METRIC_OVERALL
    };
    if (selectable_count != sizeof(nvml_expected) / sizeof(nvml_expected[0]))
        return 14;
    for (size_t index = 0U; index < selectable_count; index++)
        if (selectable[index] != nvml_expected[index]) return 15;

    if (lsm_gpu_selectable_metrics(NULL, selectable, LSM_GPU_METRIC_COUNT) != 0U)
        return 16;

    puts("GPU graph metric capability selection passed.");
    return 0;
}
