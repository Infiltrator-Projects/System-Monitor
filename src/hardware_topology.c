// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file hardware_topology.c
 * @brief Stable optional-hardware topology reconciliation.
 *
 * Enumeration and live sampling remain in monitor_hardware.c. This module owns
 * ordering and membership comparison for the platform-neutral public snapshot.
 * Retained cumulative baselines live exclusively in the native backend state.
 *
 * Reconciliation is a model transformation, not a hardware operation. Inputs
 * are sorted by stable identity and surviving records retain published metrics
 * while refreshed identity/capability fields replace stale discovery data.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "hardware_topology.h"

#include "common.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *gpu_identity(const LsmGpuInfo *gpu)
{
    return gpu->platform_identity[0] ? gpu->platform_identity : gpu->display_identifier;
}

static const char *npu_identity(const LsmNpuInfo *npu)
{
    return npu->platform_identity[0] ? npu->platform_identity : npu->display_identifier;
}

/* Topology reconciliation preserves cumulative baselines across reorderings. */
static bool same_gpu_identity(const LsmGpuInfo *left,
                              const LsmGpuInfo *right)
{
    if (left->platform_identity[0] && right->platform_identity[0])
        return strcmp(left->platform_identity, right->platform_identity) == 0;
    return strcmp(left->display_identifier, right->display_identifier) == 0;
}

static bool same_npu_identity(const LsmNpuInfo *left,
                              const LsmNpuInfo *right)
{
    if (left->platform_identity[0] && right->platform_identity[0])
        return strcmp(left->platform_identity, right->platform_identity) == 0;
    return strcmp(left->display_identifier, right->display_identifier) == 0;
}

static int compare_gpu_identity(const void *left, const void *right)
{
    const LsmGpuInfo *a = left;
    const LsmGpuInfo *b = right;
    return strcmp(gpu_identity(a), gpu_identity(b));
}

static int compare_battery_names(const void *left, const void *right)
{
    const LsmBatteryInfo *a = left;
    const LsmBatteryInfo *b = right;
    return strcmp(a->name, b->name);
}

static int compare_npu_identity(const void *left, const void *right)
{
    const LsmNpuInfo *a = left;
    const LsmNpuInfo *b = right;
    return strcmp(npu_identity(a), npu_identity(b));
}

static const LsmGpuInfo *find_old_gpu(const LsmGpuInfo *gpus, size_t count,
                                      const LsmGpuInfo *identity)
{
    for (size_t index = 0; index < count; index++)
        if (same_gpu_identity(&gpus[index], identity)) return &gpus[index];
    return NULL;
}

static const LsmBatteryInfo *find_old_battery(const LsmBatteryInfo *batteries,
                                              size_t count, const char *name)
{
    for (size_t index = 0; index < count; index++)
        if (strcmp(batteries[index].name, name) == 0) return &batteries[index];
    return NULL;
}

static const LsmNpuInfo *find_old_npu(const LsmNpuInfo *npus, size_t count,
                                      const LsmNpuInfo *identity)
{
    for (size_t index = 0; index < count; index++)
        if (same_npu_identity(&npus[index], identity)) return &npus[index];
    return NULL;
}

static bool hardware_membership_changed(const LsmGpuInfo *old_gpus,
                                        size_t old_gpu_count,
                                        const LsmBatteryInfo *old_batteries,
                                        size_t old_battery_count,
                                        const LsmNpuInfo *old_npus,
                                        size_t old_npu_count,
                                        const LsmMonitor *monitor)
{
    if (old_gpu_count != monitor->gpu_count ||
        old_battery_count != monitor->battery_count ||
        old_npu_count != monitor->npu_count) return true;
    for (size_t index = 0; index < monitor->gpu_count; index++)
        if (!find_old_gpu(old_gpus, old_gpu_count, &monitor->gpus[index]))
            return true;
    for (size_t index = 0; index < monitor->battery_count; index++)
        if (!find_old_battery(old_batteries, old_battery_count,
                              monitor->batteries[index].name))
            return true;
    for (size_t index = 0; index < monitor->npu_count; index++)
        if (!find_old_npu(old_npus, old_npu_count, &monitor->npus[index]))
            return true;
    return false;
}


bool lsm_hardware_topology_reconcile(
    LsmMonitor *monitor, const LsmGpuInfo *old_gpus, size_t old_gpu_count,
    const LsmBatteryInfo *old_batteries, size_t old_battery_count,
    const LsmNpuInfo *old_npus, size_t old_npu_count)
{
    if (!monitor) return false;
    qsort(monitor->gpus, monitor->gpu_count, sizeof(monitor->gpus[0]),
          compare_gpu_identity);
    qsort(monitor->batteries, monitor->battery_count,
          sizeof(monitor->batteries[0]), compare_battery_names);
    qsort(monitor->npus, monitor->npu_count, sizeof(monitor->npus[0]),
          compare_npu_identity);

    const bool changed = hardware_membership_changed(
        old_gpus, old_gpu_count, old_batteries, old_battery_count,
        old_npus, old_npu_count, monitor);

    for (size_t index = 0; index < monitor->gpu_count; index++) {
        const LsmGpuInfo identity = monitor->gpus[index];
        const LsmGpuInfo *old = find_old_gpu(old_gpus, old_gpu_count, &identity);
        if (!old) continue;
        monitor->gpus[index] = *old;
        lsm_copy_string(monitor->gpus[index].display_identifier,
                        sizeof(monitor->gpus[index].display_identifier), identity.display_identifier);
        lsm_copy_string(monitor->gpus[index].platform_identity,
                        sizeof(monitor->gpus[index].platform_identity),
                        identity.platform_identity);
        if (identity.name[0])
            lsm_copy_string(monitor->gpus[index].name,
                            sizeof(monitor->gpus[index].name), identity.name);
        if (identity.driver[0])
            lsm_copy_string(monitor->gpus[index].driver,
                            sizeof(monitor->gpus[index].driver), identity.driver);
        lsm_copy_string(monitor->gpus[index].driver_version,
                        sizeof(monitor->gpus[index].driver_version),
                        identity.driver_version);
        lsm_copy_string(monitor->gpus[index].pci_location,
                        sizeof(monitor->gpus[index].pci_location),
                        identity.pci_location);
        /* Backend classification belongs to the current driver discovery, not
         * to the retained sample history. This matters after a driver rebind
         * or hot-plug transition while the stable PCI identity remains equal. */
        monitor->gpus[index].engine_metrics_capable =
            identity.engine_metrics_capable;
        monitor->gpus[index].shared_system_memory =
            identity.shared_system_memory;
        monitor->gpus[index].integrated_cooling =
            identity.integrated_cooling;
        if (identity.metrics_source[0])
            lsm_copy_string(monitor->gpus[index].metrics_source,
                            sizeof(monitor->gpus[index].metrics_source),
                            identity.metrics_source);
    }
    for (size_t index = 0; index < monitor->battery_count; index++) {
        const LsmBatteryInfo identity = monitor->batteries[index];
        const LsmBatteryInfo *old = find_old_battery(
            old_batteries, old_battery_count, identity.name);
        if (!old) continue;
        monitor->batteries[index] = *old;
        lsm_copy_string(monitor->batteries[index].name,
                        sizeof(monitor->batteries[index].name), identity.name);
        lsm_copy_string(monitor->batteries[index].model,
                        sizeof(monitor->batteries[index].model), identity.model);
        lsm_copy_string(monitor->batteries[index].manufacturer,
                        sizeof(monitor->batteries[index].manufacturer),
                        identity.manufacturer);
        lsm_copy_string(monitor->batteries[index].technology,
                        sizeof(monitor->batteries[index].technology),
                        identity.technology);
        lsm_copy_string(monitor->batteries[index].scope,
                        sizeof(monitor->batteries[index].scope), identity.scope);
        lsm_copy_string(monitor->batteries[index].serial,
                        sizeof(monitor->batteries[index].serial), identity.serial);
        lsm_copy_string(monitor->batteries[index].connection,
                        sizeof(monitor->batteries[index].connection),
                        identity.connection);
        lsm_copy_string(monitor->batteries[index].battery_source,
                        sizeof(monitor->batteries[index].battery_source),
                        identity.battery_source);
        lsm_copy_string(monitor->batteries[index].device_type,
                        sizeof(monitor->batteries[index].device_type),
                        identity.device_type);
        lsm_copy_string(monitor->batteries[index].modalias,
                        sizeof(monitor->batteries[index].modalias),
                        identity.modalias);
        monitor->batteries[index].supplemental_capacity_percent =
            identity.supplemental_capacity_percent;
        monitor->batteries[index].has_supplemental_capacity =
            identity.has_supplemental_capacity;
        monitor->batteries[index].is_peripheral = identity.is_peripheral;
        monitor->batteries[index].paired = identity.paired;
        monitor->batteries[index].trusted = identity.trusted;
        monitor->batteries[index].services_resolved =
            identity.services_resolved;
        monitor->batteries[index].bluetooth_details_available =
            identity.bluetooth_details_available;
        monitor->batteries[index].present = identity.present;
        if (isfinite(identity.capacity_percent))
            monitor->batteries[index].capacity_percent =
                identity.capacity_percent;
        if (identity.status[0])
            lsm_copy_string(monitor->batteries[index].status,
                            sizeof(monitor->batteries[index].status),
                            identity.status);
    }
    for (size_t index = 0; index < monitor->npu_count; index++) {
        const LsmNpuInfo identity = monitor->npus[index];
        const LsmNpuInfo *old = find_old_npu(old_npus, old_npu_count, &identity);
        if (!old) continue;
        monitor->npus[index] = *old;
        lsm_copy_string(monitor->npus[index].display_identifier,
                        sizeof(monitor->npus[index].display_identifier),
                        identity.display_identifier);
        lsm_copy_string(monitor->npus[index].platform_identity,
                        sizeof(monitor->npus[index].platform_identity),
                        identity.platform_identity);
        lsm_copy_string(monitor->npus[index].device_identifier,
                        sizeof(monitor->npus[index].device_identifier),
                        identity.device_identifier);
        if (identity.name[0])
            lsm_copy_string(monitor->npus[index].name,
                            sizeof(monitor->npus[index].name), identity.name);
        if (identity.driver[0])
            lsm_copy_string(monitor->npus[index].driver,
                            sizeof(monitor->npus[index].driver), identity.driver);
    }

    return changed;
}
