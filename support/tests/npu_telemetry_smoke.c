// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file npu_telemetry_smoke.c
 * @brief Regression test for Intel IVPU and generic accelerator telemetry.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
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
