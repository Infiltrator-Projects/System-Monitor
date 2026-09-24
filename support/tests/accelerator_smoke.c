// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file accelerator_smoke.c
 * @brief Consolidated accelerator regression smoke suite.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <stddef.h>
#include <stdio.h>

int smoke_case_hardware_topology(void);
int smoke_case_intel_gpu(void);
int smoke_case_npu_telemetry(void);

/* ---- hardware_topology ---- */
#define main smoke_case_hardware_topology
/**
 * @file hardware_topology_smoke.c
 * @brief Verify stable-ID topology reconciliation preserves live baselines.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "hardware_topology.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmMonitor monitor = {0};
    LsmGpuInfo old_gpus[LSM_MAX_GPUS] = {0};
    LsmBatteryInfo old_batteries[LSM_MAX_BATTERIES] = {0};
    LsmNpuInfo old_npus[LSM_MAX_NPUS] = {0};

    strcpy(old_gpus[0].platform_identity, "/devices/pci0/gpu0");
    strcpy(old_gpus[0].display_identifier, "card0");
    old_gpus[0].utilization_percent = 37.0;
    old_gpus[0].utilization_available = true;

    monitor.gpu_count = 1U;
    strcpy(monitor.gpus[0].platform_identity, "/devices/pci0/gpu0");
    strcpy(monitor.gpus[0].display_identifier, "card2");
    strcpy(monitor.gpus[0].driver, "xe");
    monitor.gpus[0].engine_metrics_capable = true;

    strcpy(old_npus[0].platform_identity, "opaque-platform-npu-0");
    strcpy(old_npus[0].display_identifier, "old-npu");
    old_npus[0].utilization_percent = 42.0;
    old_npus[0].utilization_available = true;
    monitor.npu_count = 1U;
    strcpy(monitor.npus[0].platform_identity, "opaque-platform-npu-0");
    strcpy(monitor.npus[0].display_identifier, "NPU 0");
    strcpy(monitor.npus[0].device_identifier, "platform-device-id-0");
    strcpy(monitor.npus[0].driver, "test-driver");

    if (lsm_hardware_topology_reconcile(&monitor, old_gpus, 1U,
                                         old_batteries, 0U,
                                         old_npus, 1U))
        return 1;
    if (!monitor.gpus[0].utilization_available ||
        monitor.gpus[0].utilization_percent != 37.0)
        return 2;
    if (strcmp(monitor.gpus[0].display_identifier, "card2") != 0 ||
        strcmp(monitor.gpus[0].driver, "xe") != 0 ||
        !monitor.gpus[0].engine_metrics_capable)
        return 3;
    if (!monitor.npus[0].utilization_available ||
        monitor.npus[0].utilization_percent != 42.0 ||
        strcmp(monitor.npus[0].display_identifier, "NPU 0") != 0 ||
        strcmp(monitor.npus[0].device_identifier, "platform-device-id-0") != 0 ||
        strcmp(monitor.npus[0].driver, "test-driver") != 0)
        return 4;

    monitor.gpu_count = 0U;
    if (!lsm_hardware_topology_reconcile(&monitor, old_gpus, 1U,
                                          old_batteries, 0U,
                                          old_npus, 1U))
        return 5;
    puts("Stable hardware-topology reconciliation passed.");
    return 0;
}

#undef main

/* ---- intel_gpu ---- */
#define main smoke_case_intel_gpu
#define make_directory lsm_test_intel_gpu_make_directory
#define write_text lsm_test_intel_gpu_write_text
#define near_value lsm_test_intel_gpu_near_value
#define engine_names lsm_test_intel_gpu_engine_names
/**
 * @file intel_gpu_smoke.c
 * @brief Regression test for native Intel PMU discovery and metric mapping.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
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

#undef main
#undef engine_names
#undef near_value
#undef write_text
#undef make_directory
#undef _POSIX_C_SOURCE
#undef WRITE
#undef WRITE_VALUE

/* ---- npu_telemetry ---- */
#define main smoke_case_npu_telemetry
#define make_directory lsm_test_npu_telemetry_make_directory
#define write_text lsm_test_npu_telemetry_write_text
#define near_value lsm_test_npu_telemetry_near_value
/**
 * @file npu_telemetry_smoke.c
 * @brief Regression test for Intel IVPU and generic accelerator telemetry.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "npu_telemetry.h"

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
    char root_template[] = "/tmp/lsm-npu-telemetry-XXXXXX";
    char *root = mkdtemp(root_template);
    if (!root) return 1;

    char freq[4096];
    if (snprintf(freq, sizeof(freq), "%s/freq", root) < 0 ||
        !make_directory(freq))
        return 2;

    char path[4096];
#define WRITE(relative, text) \
    do { \
        if (snprintf(path, sizeof(path), "%s/%s", root, relative) < 0 || \
            !write_text(path, text)) return 3; \
    } while (0)
    WRITE("npu_busy_time_us", "100000\n");
    WRITE("npu_memory_utilization", "134217728\n");
    WRITE("freq/current_freq", "800\n");
#undef WRITE

    if (setenv("LSM_NPU_SYSFS_ROOT", root, 1) != 0) return 4;
    LsmNpuInfo npu;
    memset(&npu, 0, sizeof(npu));
    snprintf(npu.driver, sizeof(npu.driver), "intel_vpu");
    snprintf(npu.platform_identity, sizeof(npu.platform_identity), "%s", root);

    LsmNpuTelemetry *telemetry = lsm_npu_telemetry_create(&npu);
    if (!telemetry) return 5;
    if (!lsm_npu_telemetry_refresh(telemetry, &npu, 1.0)) return 6;
    if (npu.utilization_available) return 7;

    if (snprintf(path, sizeof(path), "%s/npu_busy_time_us", root) < 0 ||
        !write_text(path, "700000\n"))
        return 8;
    if (!lsm_npu_telemetry_refresh(telemetry, &npu, 1.0)) return 9;
    if (!npu.utilization_available ||
        !near_value(npu.utilization_percent, 60.0) ||
        !npu.memory_used_available ||
        npu.memory_used_bytes != 134217728U ||
        !npu.clock_available || !near_value(npu.clock_mhz, 800.0) ||
        strcmp(npu.metrics_source, "Native Intel IVPU sysfs") != 0)
        return 10;

    /* Removed or unreadable attributes must clear current availability. */
    if (snprintf(path, sizeof(path), "%s/npu_busy_time_us", root) < 0 ||
        unlink(path) != 0 ||
        snprintf(path, sizeof(path), "%s/npu_memory_utilization", root) < 0 ||
        unlink(path) != 0 ||
        snprintf(path, sizeof(path), "%s/freq/current_freq", root) < 0 ||
        unlink(path) != 0)
        return 11;
    if (lsm_npu_telemetry_refresh(telemetry, &npu, 1.0) ||
        npu.utilization_available || npu.memory_used_available ||
        npu.memory_total_available || npu.clock_available ||
        npu.temperature_available || npu.power_available ||
        npu.supported_metrics || npu.metrics_source[0] != '\0')
        return 12;

    lsm_npu_telemetry_destroy(telemetry);

    /* Unknown drivers must not guess units from ambiguous attribute names. */
    if (snprintf(path, sizeof(path), "%s/frequency", root) < 0 ||
        !write_text(path, "800000000\n"))
        return 13;
    memset(&npu, 0, sizeof(npu));
    snprintf(npu.driver, sizeof(npu.driver), "unknown_npu");
    snprintf(npu.platform_identity, sizeof(npu.platform_identity), "%s", root);
    telemetry = lsm_npu_telemetry_create(&npu);
    if (telemetry != NULL) return 14;

    unsetenv("LSM_NPU_SYSFS_ROOT");
    puts("NPU cumulative busy, explicit-unit and stale-value handling passed.");
    return 0;
}

#undef main
#undef near_value
#undef write_text
#undef make_directory
#undef _POSIX_C_SOURCE
#undef WRITE

typedef int (*LsmMergedSmokeCaseFunction)(void);
typedef struct { const char *name; LsmMergedSmokeCaseFunction function; } LsmMergedSmokeCase;

int main(void)
{
    static const LsmMergedSmokeCase cases[] = {
        {"hardware_topology", smoke_case_hardware_topology},
        {"intel_gpu", smoke_case_intel_gpu},
        {"npu_telemetry", smoke_case_npu_telemetry},
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0U; i < count; ++i) {
        const int status = cases[i].function();
        if (status != 0) {
            fprintf(stderr, "accelerator smoke suite: %s failed with status %d\n", cases[i].name, status);
            return status;
        }
    }
    printf("accelerator smoke suite passed (%zu cases).\n", count);
    return 0;
}
