// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file hardware_topology_smoke.c
 * @brief Verify stable-ID topology reconciliation preserves live baselines.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
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
