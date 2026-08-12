// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file hardware_topology.h
 * @brief Stable-ID sorting and state reconciliation for optional hardware.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_HARDWARE_TOPOLOGY_H
#define LINUX_SYSTEM_MONITOR_HARDWARE_TOPOLOGY_H

#include "monitor_types.h"

/**
 * Reconcile newly enumerated optional hardware with the published snapshot.
 *
 * Devices are matched by opaque stable identity. Removed devices are discarded;
 * surviving devices retain published metrics while native cumulative baselines
 * remain private to the active backend.
 *
 * @param [in,out] monitor Snapshot containing newly enumerated devices.
 * @param [in] old_gpus Previous GPU array.
 * @param [in] old_gpu_count Number of previous GPU records.
 * @param [in] old_batteries Previous battery array.
 * @param [in] old_battery_count Number of previous battery records.
 * @param [in] old_npus Previous accelerator array.
 * @param [in] old_npu_count Number of previous accelerator records.
 * @return true when user-visible topology changed.
 */
bool lsm_hardware_topology_reconcile(
    LsmMonitor *monitor, const LsmGpuInfo *old_gpus, size_t old_gpu_count,
    const LsmBatteryInfo *old_batteries, size_t old_battery_count,
    const LsmNpuInfo *old_npus, size_t old_npu_count);

#endif
