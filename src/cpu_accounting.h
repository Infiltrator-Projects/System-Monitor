// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file cpu_accounting.h
 * @brief Testable Linux scheduler-counter parsing and rate calculation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_CPU_ACCOUNTING_H
#define LINUX_SYSTEM_MONITOR_CPU_ACCOUNTING_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


/** Raw aggregate or per-CPU scheduler counters. */
typedef struct {
    uint64_t total; /**< Total scheduler ticks. */
    uint64_t idle;  /**< Idle plus I/O-wait ticks. */
    uint64_t user;  /**< User and nice scheduler ticks. */
    uint64_t kernel; /**< System, IRQ, soft-IRQ and steal scheduler ticks. */
} LsmCpuCounters;

/** Retained scheduler baselines owned by the native collector. */
typedef struct {
    LsmCpuCounters previous[LSM_MAX_CPUS + 1U];
    uint64_t previous_interrupt_count;
    uint64_t previous_context_switch_count;
    bool scheduler_events_initialized;
} LsmCpuAccountingState;

/** One bounded snapshot parsed from procfs stat data. */
typedef struct {
    LsmCpuCounters cpus[LSM_MAX_CPUS + 1U];
    size_t cpu_count;
    uint64_t interrupts;
    uint64_t context_switches;
} LsmCpuAccountingSample;

/**
 * Parse an in-memory /proc/stat payload without reading the filesystem.
 *
 * @param [in] text Text payload, which need not end with a newline.
 * @param [out] sample Fully initialised bounded result.
 * @return true when a valid aggregate CPU row was present.
 */
bool lsm_cpu_accounting_parse(const char *text,
                              LsmCpuAccountingSample *sample);

/**
 * Read and parse one procfs scheduler snapshot.
 *
 * @param [in] path Procfs stat path; normally /proc/stat.
 * @param [out] sample Fully initialised bounded result.
 * @return true when the file contained a valid aggregate CPU row.
 */
bool lsm_cpu_accounting_read(const char *path,
                             LsmCpuAccountingSample *sample);

/**
 * Apply a parsed scheduler sample to retained CPU state.
 *
 * @param [in,out] cpu Published CPU metrics.
 * @param [in,out] state Private retained counter baselines.
 * @param [in] sample Current cumulative scheduler sample.
 * @param [in] initial Establish baselines without publishing rates.
 * @param [in] elapsed_seconds Monotonic interval for event rates.
 */
void lsm_cpu_accounting_apply(LsmCpuInfo *cpu,
                              LsmCpuAccountingState *state,
                              const LsmCpuAccountingSample *sample,
                              bool initial, double elapsed_seconds);

#endif
