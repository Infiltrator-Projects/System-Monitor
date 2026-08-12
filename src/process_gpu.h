// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_gpu.h
 * @brief Native DRM per-process engine and graphics-memory accounting.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROCESS_GPU_H
#define LINUX_SYSTEM_MONITOR_PROCESS_GPU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "process_model.h"

/** Maximum distinct DRM engine classes retained for one process. */
#define LSM_PROCESS_GPU_MAX_ENGINES 32

/** One cumulative DRM engine-time counter. */
typedef struct {
    char name[256];
    uint64_t time_ns;
    unsigned capacity;
    bool time_available;
} LsmProcessGpuEngine;

/** Cumulative graphics counters collected from one process's DRM clients. */
typedef struct {
    LsmProcessGpuEngine engines[LSM_PROCESS_GPU_MAX_ENGINES];
    size_t engine_count;
    uint64_t memory_bytes;
    bool engine_counters_available;
    bool memory_available;
} LsmProcessGpuSnapshot;

/**
 * Read and deduplicate the DRM client counters owned by one process.
 *
 * @param [in] proc_root Procfs root; normally /proc and replaceable by tests.
 * @param [in] pid Process identifier.
 * @param [out] snapshot Cumulative engine-time and memory snapshot.
 * @return true when at least one supported graphics counter was readable.
 */
bool lsm_process_gpu_read(const char *proc_root, LsmProcessId pid,
                          LsmProcessGpuSnapshot *snapshot);

/**
 * Retain prior engine values while a driver reports a temporary decrease.
 *
 * The DRM accounting contract permits a counter to fall briefly while its
 * internal totals are being updated. Holding the previous high-water value
 * prevents a false reset and lets sampling resume when the counter catches up.
 *
 * @param [in,out] current Current snapshot to normalise.
 * @param [in] previous Prior snapshot for the same process instance.
 */
void lsm_process_gpu_normalise(LsmProcessGpuSnapshot *current,
                               const LsmProcessGpuSnapshot *previous);

/**
 * Convert two cumulative engine snapshots into peak-engine utilisation.
 *
 * Engine classes are aggregated across a process's DRM clients before their
 * deltas are compared. The reported value follows Task Manager's peak-engine
 * model and is clamped to the inclusive 0..100 percent range.
 *
 * @param [in] current Current cumulative snapshot.
 * @param [in] previous Prior snapshot for the same process instance.
 * @param [in] elapsed_seconds Monotonic time separating the snapshots.
 * @param [out] percent Calculated peak-engine utilisation.
 * @return true when at least one matching monotonic engine counter existed.
 */
bool lsm_process_gpu_calculate(const LsmProcessGpuSnapshot *current,
                               const LsmProcessGpuSnapshot *previous,
                               double elapsed_seconds, double *percent);

/**
 * Convert cumulative counters into peak utilisation and its engine name.
 *
 * @param [in] current Current cumulative snapshot.
 * @param [in] previous Prior snapshot for the same process instance.
 * @param [in] elapsed_seconds Monotonic time separating the snapshots.
 * @param [out] percent Calculated peak-engine utilisation.
 * @param [out] engine Destination for the matching DRM engine name.
 * @param [in] engine_size Size of @p engine.
 * @return true when at least one matching monotonic engine counter existed.
 */
bool lsm_process_gpu_calculate_engine(
    const LsmProcessGpuSnapshot *current,
    const LsmProcessGpuSnapshot *previous,
    double elapsed_seconds, double *percent,
    char *engine, size_t engine_size);

#endif
