// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file nvml_smoke.c
 * @brief Native NVML adapter smoke test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "nvml.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void assert_first_device(const LsmGpuInfo *gpu)
{
    assert(strcmp(gpu->name, "Mock NVIDIA GPU 0") == 0);
    assert(strstr(gpu->driver, "999.1") != NULL);
    assert(fabs(gpu->utilization_percent - 42.0) < 0.01);
    assert(gpu->memory_busy_available &&
           fabs(gpu->memory_busy_percent - 17.0) < 0.01);
    assert(gpu->encoder_available && fabs(gpu->encoder_percent - 5.0) < 0.01);
    assert(gpu->decoder_available && fabs(gpu->decoder_percent - 7.0) < 0.01);
    assert(gpu->memory_total_bytes == (8ULL << 30));
    assert(gpu->memory_used_bytes == (2ULL << 30));
    assert(gpu->temperature_available && fabs(gpu->temperature_c - 65.0) < 0.01);
    assert(gpu->core_clock_available && fabs(gpu->core_clock_mhz - 2100.0) < 0.01);
    assert(gpu->memory_clock_available && fabs(gpu->memory_clock_mhz - 9000.0) < 0.01);
    assert(gpu->power_available && fabs(gpu->power_watts - 125.0) < 0.01);
    assert(gpu->fan_available && fabs(gpu->fan_percent - 33.0) < 0.01);
    assert(gpu->supported_metrics);
}

int main(void)
{
    LsmMonitor monitor = {0};
    monitor.gpu_count = 2;

    /* Deliberately reverse DRM order. PCI identity, not enumeration order,
     * must select the destination for each NVML sample. */
    snprintf(monitor.gpus[0].driver, sizeof(monitor.gpus[0].driver), "nvidia");
    snprintf(monitor.gpus[0].display_identifier, sizeof(monitor.gpus[0].display_identifier), "card1");
    snprintf(monitor.gpus[0].platform_identity,
             sizeof(monitor.gpus[0].platform_identity),
             "/sys/devices/pci0000:00/0000:02:00.0");
    snprintf(monitor.gpus[1].driver, sizeof(monitor.gpus[1].driver), "nvidia");
    snprintf(monitor.gpus[1].display_identifier, sizeof(monitor.gpus[1].display_identifier), "card0");
    snprintf(monitor.gpus[1].platform_identity,
             sizeof(monitor.gpus[1].platform_identity),
             "/sys/devices/pci0000:00/0000:01:00.0");

    lsm_nvml_refresh(&monitor);
    assert_first_device(&monitor.gpus[1]);
    assert(strcmp(monitor.gpus[0].name, "Mock NVIDIA GPU 1") == 0);
    assert(fabs(monitor.gpus[0].utilization_percent - 84.0) < 0.01);
    assert(monitor.gpu_count == 2);

    /* A valid but currently unmatched PCI device must remain stable and must
     * never be attached to an unrelated GPU by ordinal position. */
    memset(&monitor, 0, sizeof(monitor));
    lsm_nvml_refresh(&monitor);
    assert(monitor.gpu_count == 2);
    assert(strcmp(monitor.gpus[0].display_identifier,
                  "nvml-00000000:01:00.0") == 0);
    assert(strcmp(monitor.gpus[1].display_identifier,
                  "nvml-00000000:02:00.0") == 0);
    lsm_nvml_refresh(&monitor);
    assert(monitor.gpu_count == 2);

    lsm_nvml_shutdown();
    puts("NVML adapter PCI-identity smoke test passed.");
    return 0;
}
