// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file disk_accounting.h
 * @brief Testable physical-disk rate, activity and latency accounting.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_DISK_ACCOUNTING_H
#define LINUX_SYSTEM_MONITOR_DISK_ACCOUNTING_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stdint.h>

/** Retained cumulative baselines for one disk accounting stream. */
typedef struct {
    uint64_t previous_read_sectors;
    uint64_t previous_write_sectors;
    uint64_t previous_read_operations;
    uint64_t previous_write_operations;
    uint64_t previous_read_ms;
    uint64_t previous_write_ms;
    uint64_t previous_io_ms;
    uint64_t previous_weighted_io_ms;
    bool initialized;
} LsmDiskAccountingState;

/** One cumulative kernel disk-statistics sample. */
typedef struct {
    uint64_t read_operations;
    uint64_t read_sectors;
    uint64_t read_ms;
    uint64_t write_operations;
    uint64_t write_sectors;
    uint64_t write_ms;
    uint64_t in_progress_operations;
    uint64_t io_ms;
    uint64_t weighted_io_ms;
} LsmDiskCounters;

/**
 * Apply one cumulative counter sample to a retained disk snapshot.
 *
 * Counter resets produce zero rates and response times rather than spikes.
 * Throughput uses the Linux diskstats 512-byte sector ABI, while response time
 * is the elapsed operation time divided by completed operations.
 *
 * @param [in,out] disk Published disk metrics.
 * @param [in,out] state Private retained counter baselines.
 * @param [in] counters Current cumulative kernel counters.
 * @param [in] elapsed_seconds Monotonic sample interval.
 */
void lsm_disk_accounting_update(LsmDiskInfo *disk,
                                LsmDiskAccountingState *state,
                                const LsmDiskCounters *counters,
                                double elapsed_seconds);

#endif
