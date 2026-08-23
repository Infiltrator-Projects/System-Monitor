// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_linux_internal.h
 * @brief Private collector boundaries used by monitor.c.
 *
 * These functions are intentionally not installed as a public API. Each
 * module owns one coherent Linux subsystem and mutates only its portion of the
 * current LsmMonitor snapshot.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_MONITOR_INTERNAL_H
#define LINUX_SYSTEM_MONITOR_MONITOR_INTERNAL_H

#include "cpu_accounting.h"
#include "disk_accounting.h"
#include "logitech_hidpp.h"
#include "monitor_types.h"
#include "system_sources.h"
#include "wifi_metadata.h"

#include <stdint.h>

typedef struct LsmLinuxHardwareState LsmLinuxHardwareState;

/** Retained Linux disk baselines keyed independently of the public snapshot. */
typedef struct {
    char name[64];
    LsmDiskAccountingState accounting;
} LsmLinuxDiskState;

/** Retained Linux network baselines keyed independently of the public snapshot. */
typedef struct {
    char name[64];
    uint64_t previous_rx;
    uint64_t previous_tx;
    bool initialized;
} LsmLinuxNetworkState;

/** Linux-only GPU identity and cumulative-counter state. */
typedef struct {
    char platform_identity[LSM_IDENTITY_LEN];
    uint64_t previous_engine_busy_ns;
    bool engine_busy_initialized;
    bool intel_native_backend;
} LsmLinuxGpuState;

/** Linux-only battery transport identity. */
typedef struct {
    char name[64];
    char hidraw_path[LSM_PATH_LEN];
    bool bluez_record;
} LsmLinuxBatteryState;

/** Private state owned by the active platform monitoring backend. */
typedef struct {
    LsmSystemSources *system_sources;
    LsmWifiMetadata *wifi_metadata;
    LsmLinuxHardwareState *hardware_state;
    void *cpu_frequency_source;
    LsmCpuAccountingState cpu_accounting;
    LsmLinuxDiskState disks[LSM_MAX_DISKS];
    size_t disk_count;
    LsmLinuxNetworkState networks[LSM_MAX_NETS];
    size_t network_count;
    LsmLinuxGpuState gpus[LSM_MAX_GPUS];
    size_t gpu_count;
    LsmLinuxBatteryState batteries[LSM_MAX_BATTERIES];
    size_t battery_count;
    double last_update_monotonic;
    double last_topology_scan_monotonic;
    double last_battery_update_monotonic;
    double last_memory_detail_monotonic;
    bool topology_refresh_requested;
} LsmLinuxMonitorBackendState;

/** Return the mutable private backend state for an initialised monitor. */
static inline LsmLinuxMonitorBackendState *monitor_backend_state(LsmMonitor *monitor)
{
    return monitor ? (LsmLinuxMonitorBackendState *)monitor->backend_state : NULL;
}

/** Return the read-only private backend state for an initialised monitor. */
static inline const LsmLinuxMonitorBackendState *monitor_backend_state_const(
    const LsmMonitor *monitor)
{
    return monitor ? (const LsmLinuxMonitorBackendState *)monitor->backend_state : NULL;
}

/** Return the active native source context, or NULL before backend setup. */
static inline LsmSystemSources *monitor_system_sources(LsmMonitor *monitor)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    return state ? state->system_sources : NULL;
}

/**
 * Initialise CPU/memory counter baselines and static CPU identity.
 *
 * @param [in,out] monitor Retained monitor snapshot.
 * @return true when mandatory CPU and memory sources were initialised.
 */
bool lsm_cpu_memory_initialise(LsmMonitor *monitor);
/**
 * Refresh CPU scheduler rates, frequencies, temperatures and memory values.
 *
 * @param [in,out] monitor Retained monitor snapshot.
 */
void lsm_cpu_memory_update(LsmMonitor *monitor, double elapsed_seconds);
/**
 * Release CPU/memory-specific retained resources.
 *
 * @param [in,out] monitor Monitor being destroyed.
 */
void lsm_cpu_memory_shutdown(LsmMonitor *monitor);

/**
 * Discover storage/network topology and establish cumulative-counter baselines.
 *
 * @param [in,out] monitor Retained monitor snapshot.
 * @return true when the native source context was created successfully.
 */
bool lsm_storage_initialise(LsmMonitor *monitor);
/**
 * Refresh storage and network rates, optionally reconciling device topology.
 *
 * @param [in,out] monitor Snapshot to update.
 * @param [in] elapsed Monotonic seconds since the previous refresh.
 * @param [in] refresh_topology Re-enumerate devices before sampling counters.
 */
void lsm_storage_update(LsmMonitor *monitor, double elapsed,
                        bool refresh_topology);

/**
 * Start in-process battery and peripheral snapshot workers.
 */
void lsm_battery_start(void);
/**
 * Rebuild the bounded battery inventory from native driver interfaces.
 *
 * @param [in,out] monitor Snapshot whose battery topology is replaced.
 */
void lsm_battery_enumerate(LsmMonitor *monitor);
/**
 * Apply current system and peripheral battery telemetry to the snapshot.
 *
 * @param [in,out] monitor Snapshot to update.
 */
void lsm_battery_update(LsmMonitor *monitor);
/**
 * Stop and join in-process battery and peripheral workers.
 */
void lsm_battery_shutdown(void);

/**
 * Discover GPU, NPU and temperature sources and initialise retained adapters.
 *
 * @param [in,out] monitor Snapshot receiving hardware topology.
 */
void lsm_hardware_initialise(LsmMonitor *monitor);
/**
 * Refresh hardware telemetry with independently controlled slow-path work.
 *
 * @param [in,out] monitor Snapshot to update.
 * @param [in] elapsed Monotonic seconds since the previous update.
 * @param [in] refresh_topology Reconcile hardware before sampling.
 * @param [in] refresh_batteries Perform full battery presentation this cycle.
 */
void lsm_hardware_update(LsmMonitor *monitor, double elapsed,
                         bool refresh_topology, bool refresh_batteries);
/**
 * Release dynamic hardware adapters and stop associated in-process workers.
 *
 * @param [in,out] monitor Monitor whose retained hardware state is released.
 */
void lsm_hardware_shutdown(LsmMonitor *monitor);

/**
 * Merge an authoritative direct HID++ reading into a battery snapshot.
 *
 * @param [in,out] battery Destination peripheral record.
 * @param [in] reading Direct HID++ reading to apply.
 * @return true when @p reading was valid and applied.
 */
bool lsm_battery_apply_hidpp_reading(
    LsmBatteryInfo *battery, const LsmHidppBatteryReading *reading);

#endif
