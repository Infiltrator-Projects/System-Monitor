// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file overview_history.h
 * @brief Bounded history of completed monitor snapshots for Overview.
 *
 * This module is toolkit-neutral. It records only snapshots that a platform
 * backend has explicitly marked complete, preserves missing data as missing,
 * inserts an explicit gap marker when completed generations are skipped, and
 * retains stable device identities so hotplug/reordering cannot redirect
 * navigation to a different device.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_OVERVIEW_HISTORY_H
#define INFILTRATOR_SYSTEM_MONITOR_OVERVIEW_HISTORY_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Maximum completed Overview samples retained in memory. */
#define LSM_OVERVIEW_HISTORY_CAPACITY 120U
/** Number of busiest CPU processes presented by Overview. */
#define LSM_OVERVIEW_TOP_PROCESS_COUNT 5U

/** Overview metric cards shared by native front ends. */
typedef enum {
    LSM_OVERVIEW_CPU = 0,
    LSM_OVERVIEW_MEMORY,
    LSM_OVERVIEW_DISK,
    LSM_OVERVIEW_NETWORK,
    LSM_OVERVIEW_GPU,
    LSM_OVERVIEW_TEMPERATURE,
    LSM_OVERVIEW_CPU_PRESSURE,
    LSM_OVERVIEW_MEMORY_PRESSURE,
    LSM_OVERVIEW_IO_PRESSURE,
    LSM_OVERVIEW_METRIC_COUNT
} LsmOverviewMetric;

/** Resource that supplied the hottest available temperature in a sample. */
typedef enum {
    LSM_OVERVIEW_TEMPERATURE_NONE = 0,
    LSM_OVERVIEW_TEMPERATURE_CPU,
    LSM_OVERVIEW_TEMPERATURE_GPU
} LsmOverviewTemperatureSource;

/** One derived Overview sample produced from one completed monitor snapshot. */
typedef struct {
    bool gap;                         /**< Explicit missing-generation marker. */
    uint64_t generation;              /**< Backend completed-snapshot generation. */
    double monotonic_seconds;         /**< Completion timestamp from the backend. */

    bool cpu_available;
    double cpu_percent;
    bool cpu_breakdown_available;       /**< User/non-kernel and kernel shares are both valid. */
    double cpu_user_percent;            /**< User plus nice scheduler time. */
    double cpu_kernel_percent;          /**< System, IRQ and soft-IRQ scheduler time. */

    bool memory_available;
    double memory_percent;
    bool memory_breakdown_available;
    double memory_available_percent;

    bool disk_available;
    double disk_percent;              /**< Mean active time across measured physical disks. */
    double disk_read_bytes_per_sec;   /**< Aggregate physical-disk read throughput. */
    double disk_write_bytes_per_sec;  /**< Aggregate physical-disk write throughput. */
    size_t disk_index;                /**< SIZE_MAX for aggregate Overview samples. */
    char disk_identity[LSM_IDENTITY_LEN]; /**< Reserved for retained single-device resolution. */
    char disk_name[LSM_NAME_LEN];     /**< Reserved for retained single-device resolution. */

    bool network_available;
    double network_bytes_per_sec;
    double network_receive_bytes_per_sec;
    double network_send_bytes_per_sec;
    size_t network_index;
    char network_identity[LSM_IDENTITY_LEN];
    char network_name[LSM_NAME_LEN];

    bool gpu_available;
    double gpu_percent;
    size_t gpu_index;
    char gpu_identity[LSM_IDENTITY_LEN];
    char gpu_name[LSM_NAME_LEN];

    bool temperature_available;
    double temperature_c;
    LsmOverviewTemperatureSource temperature_source;
    size_t temperature_gpu_index;
    char temperature_identity[LSM_IDENTITY_LEN];
    char temperature_name[LSM_NAME_LEN];

    bool cpu_pressure_available;
    double cpu_pressure_percent;
    bool memory_pressure_available;
    double memory_pressure_percent;
    bool io_pressure_available;
    double io_pressure_percent;
} LsmOverviewSample;

/** Opaque fixed-capacity completed-snapshot history. */
typedef struct LsmOverviewHistory LsmOverviewHistory;

/**
 * Allocate an empty completed-snapshot history.
 *
 * @return New history owned by the caller, or NULL on allocation failure.
 */
LsmOverviewHistory *lsm_overview_history_create(void);

/**
 * Release a history created by lsm_overview_history_create().
 *
 * @param [in,out] history History to release, or NULL.
 */
void lsm_overview_history_destroy(LsmOverviewHistory *history);

/**
 * Remove every retained sample and completed-generation baseline.
 *
 * @param [in,out] history History to clear, or NULL.
 */
void lsm_overview_history_reset(LsmOverviewHistory *history);

/**
 * Record a newly completed monitor snapshot.
 *
 * Snapshots with generation zero, duplicate generations, invalid completion
 * timestamps or time that moves backwards are ignored. A skipped generation
 * inserts one explicit gap record before the newest sample.
 *
 * @param [in,out] history Destination bounded history.
 * @param [in] monitor Completed public monitor snapshot.
 * @return true when a new completed sample was retained.
 */
bool lsm_overview_history_record(LsmOverviewHistory *history,
                                 const LsmMonitor *monitor);

/**
 * Return the number of logical samples currently retained.
 *
 * @param [in] history History to inspect.
 * @return Sample count from zero through LSM_OVERVIEW_HISTORY_CAPACITY.
 */
size_t lsm_overview_history_count(const LsmOverviewHistory *history);

/**
 * Read one retained sample in oldest-to-newest order.
 *
 * @param [in] history History to inspect.
 * @param index Logical oldest-to-newest index.
 * @param [out] sample Receives a complete copy when present.
 * @return true when @p index names a retained sample.
 */
bool lsm_overview_history_get(const LsmOverviewHistory *history,
                              size_t index, LsmOverviewSample *sample);

/**
 * Read the newest retained sample.
 *
 * @param [in] history History to inspect.
 * @param [out] sample Receives the newest sample.
 * @return true when at least one sample exists.
 */
bool lsm_overview_history_latest(const LsmOverviewHistory *history,
                                 LsmOverviewSample *sample);

/**
 * Resolve the retained busiest disk against the current topology.
 *
 * Stable identity is preferred and user-visible name is the fallback.
 *
 * @param [in] monitor Current monitor topology.
 * @param [in] sample Retained sample containing disk identity.
 * @return Current disk index, or SIZE_MAX when it no longer exists.
 */
size_t lsm_overview_resolve_disk(const LsmMonitor *monitor,
                                 const LsmOverviewSample *sample);

/**
 * Resolve the retained busiest network adapter against current topology.
 *
 * @param [in] monitor Current monitor topology.
 * @param [in] sample Retained sample containing network identity.
 * @return Current adapter index, or SIZE_MAX when it no longer exists.
 */
size_t lsm_overview_resolve_network(const LsmMonitor *monitor,
                                    const LsmOverviewSample *sample);

/**
 * Resolve the retained busiest GPU against current topology.
 *
 * @param [in] monitor Current monitor topology.
 * @param [in] sample Retained sample containing GPU identity.
 * @return Current GPU index, or SIZE_MAX when it no longer exists.
 */
size_t lsm_overview_resolve_gpu(const LsmMonitor *monitor,
                                const LsmOverviewSample *sample);

/**
 * Select the busiest processes by CPU without mutating the source snapshot.
 *
 * Non-finite CPU values are ignored. Ties are deterministic by PID.
 *
 * @param [in] processes Current process snapshot.
 * @param count Number of entries in @p processes.
 * @param [out] indices Receives up to LSM_OVERVIEW_TOP_PROCESS_COUNT indices.
 * @return Number of valid indices written.
 */
size_t lsm_overview_top_cpu_processes(
    const LsmProcessInfo *processes, size_t count,
    size_t indices[LSM_OVERVIEW_TOP_PROCESS_COUNT]);

#endif
