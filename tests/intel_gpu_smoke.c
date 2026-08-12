// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file intel_gpu_smoke.c
 * @brief Regression test for native Intel PMU discovery and metric mapping.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "intel_gpu.h"
#include "common.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool make_directory(const char *path)
{
    return mkdir(path, 0700) == 0 || errno == EEXIST;
}

static bool write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    if (!file) return false;
    const bool okay = fputs(text, file) >= 0 && fclose(file) == 0;
    return okay;
}

static bool near_value(double value, double expected)
{
    return fabs(value - expected) < 0.01;
}

int main(void)
{
    char root_template[] = "/tmp/lsm-intel-gpu-XXXXXX";
    char *root = mkdtemp(root_template);
    if (!root) return 1;

    char events[4096];
    char values[4096];
    char device[4096];
    char hwmon[4096];
    char hwmon0[4096];
    if (snprintf(events, sizeof(events), "%s/events", root) < 0 ||
        snprintf(values, sizeof(values), "%s/mock-values", root) < 0 ||
        snprintf(device, sizeof(device), "%s/device", root) < 0 ||
        snprintf(hwmon, sizeof(hwmon), "%s/hwmon", device) < 0 ||
        snprintf(hwmon0, sizeof(hwmon0), "%s/hwmon0", hwmon) < 0)
        return 2;
    if (!make_directory(events) || !make_directory(values) ||
        !make_directory(device) || !make_directory(hwmon) ||
        !make_directory(hwmon0))
        return 3;

    char path[4096];
#define WRITE(relative, text) \
    do { \
        if (snprintf(path, sizeof(path), "%s/%s", root, relative) < 0 || \
            !write_text(path, text)) return 4; \
    } while (0)
    WRITE("type", "17\n");
    WRITE("cpumask", "0\n");
    WRITE("events/rcs0-busy", "config=0x0\n");
    WRITE("events/ccs0-busy", "config=0x1000\n");
    WRITE("events/vcs0-busy", "config=0x2000\n");
    WRITE("events/vecs0-busy", "config=0x3000\n");
    WRITE("events/bcs0-busy", "config=0x4000\n");
    WRITE("events/actual-frequency-gt0", "config=0x100000\n");
    WRITE("events/actual-frequency-gt1", "config=0x100001\n");
    WRITE("mock-values/rcs0-busy", "0\n");
    WRITE("mock-values/ccs0-busy", "0\n");
    WRITE("mock-values/vcs0-busy", "0\n");
    WRITE("mock-values/vecs0-busy", "0\n");
    WRITE("mock-values/bcs0-busy", "0\n");
    WRITE("mock-values/actual-frequency-gt0", "0\n");
    WRITE("mock-values/actual-frequency-gt1", "0\n");
#undef WRITE

    if (snprintf(path, sizeof(path), "%s/temp1_input", hwmon0) < 0 ||
        !write_text(path, "45000\n")) return 5;
    char energy_path[4096];
    if (snprintf(energy_path, sizeof(energy_path), "%s/energy1_input", hwmon0) < 0 ||
        !write_text(energy_path, "1000000\n")) return 5;

    if (setenv("LSM_INTEL_GPU_PMU_ROOT", root, 1) != 0) return 6;
    LsmGpuInfo gpu;
    memset(&gpu, 0, sizeof(gpu));
    snprintf(gpu.driver, sizeof(gpu.driver), "i915");
    lsm_copy_string(gpu.platform_identity, sizeof(gpu.platform_identity), device);

    LsmIntelGpuBackend *backend = lsm_intel_gpu_create(&gpu);
    if (!backend) return 7;
    if (!lsm_intel_gpu_refresh(backend, &gpu, 1.0)) return 8;

#define WRITE_VALUE(name, text) \
    do { \
        if (snprintf(path, sizeof(path), "%s/mock-values/%s", root, name) < 0 || \
            !write_text(path, text)) return 9; \
    } while (0)
    WRITE_VALUE("rcs0-busy", "500000000\n");
    WRITE_VALUE("ccs0-busy", "250000000\n");
    WRITE_VALUE("vcs0-busy", "100000000\n");
    WRITE_VALUE("vecs0-busy", "50000000\n");
    WRITE_VALUE("bcs0-busy", "20000000\n");
    WRITE_VALUE("actual-frequency-gt0", "900\n");
    WRITE_VALUE("actual-frequency-gt1", "600\n");
#undef WRITE_VALUE
    if (!write_text(energy_path, "2000000\n")) return 10;

    if (!lsm_intel_gpu_refresh(backend, &gpu, 1.0)) return 11;
    if (!near_value(gpu.utilization_percent, 50.0) ||
        !near_value(gpu.render_percent, 50.0) ||
        !near_value(gpu.compute_percent, 25.0) ||
        !near_value(gpu.video_percent, 10.0) ||
        !near_value(gpu.video_enhance_percent, 5.0) ||
        !near_value(gpu.copy_percent, 2.0) ||
        !near_value(gpu.core_clock_mhz, 900.0) ||
        !near_value(gpu.memory_clock_mhz, 600.0) ||
        !near_value(gpu.temperature_c, 45.0) ||
        !near_value(gpu.power_watts, 1.0))
        return 12;
    if (!gpu.engine_metrics_capable || !gpu.shared_system_memory ||
        !gpu.integrated_cooling || !gpu.supported_metrics ||
        !gpu.render_available || !gpu.compute_available ||
        !gpu.video_available || !gpu.video_enhance_available ||
        !gpu.copy_available || !gpu.core_clock_available ||
        !gpu.memory_clock_available || !gpu.temperature_available ||
        !gpu.power_available || strcmp(gpu.metrics_source, "Native Intel PMU") != 0)
        return 13;

    /* A failed PMU read must become unavailable, never a false zero. */
    static const char *const engine_names[] = {
        "rcs0-busy", "ccs0-busy", "vcs0-busy", "vecs0-busy", "bcs0-busy"
    };
    for (size_t index = 0U; index < LSM_ARRAY_LENGTH(engine_names); index++) {
        if (snprintf(path, sizeof(path), "%s/mock-values/%s", root,
                     engine_names[index]) < 0 || unlink(path) != 0)
            return 14;
    }
    gpu.utilization_available = false;
    gpu.render_available = false;
    gpu.compute_available = false;
    gpu.video_available = false;
    gpu.video_enhance_available = false;
    gpu.copy_available = false;
    if (!lsm_intel_gpu_refresh(backend, &gpu, 1.0) ||
        gpu.utilization_available || gpu.render_available ||
        gpu.compute_available || gpu.video_available ||
        gpu.video_enhance_available || gpu.copy_available ||
        strcmp(gpu.metrics_source, "Native Intel driver telemetry") != 0)
        return 15;

    LsmGpuInfo amd;
    memset(&amd, 0, sizeof(amd));
    snprintf(amd.driver, sizeof(amd.driver), "amdgpu");
    if (lsm_intel_gpu_driver_supported(amd.driver) ||
        lsm_intel_gpu_create(&amd) != NULL)
        return 16;

    LsmGpuInfo nvidia;
    memset(&nvidia, 0, sizeof(nvidia));
    snprintf(nvidia.driver, sizeof(nvidia.driver), "nvidia");
    if (lsm_intel_gpu_driver_supported(nvidia.driver) ||
        lsm_intel_gpu_create(&nvidia) != NULL)
        return 17;
    if (!lsm_intel_gpu_driver_supported("xe")) return 18;

    lsm_intel_gpu_destroy(backend);
    unsetenv("LSM_INTEL_GPU_PMU_ROOT");
    puts("Intel PMU engine, frequency, hwmon and isolation mapping passed.");
    return 0;
}
