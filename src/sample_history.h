// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file sample_history.h
 * @brief Fixed-length circular sample history used by performance graphs.
 *
 * The name deliberately distinguishes short-lived graph samples from the
 * persistent application-history feature implemented in history.c.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_SAMPLE_HISTORY_H
#define LINUX_SYSTEM_MONITOR_SAMPLE_HISTORY_H

#include <stdbool.h>
#include <stddef.h>

#define LSM_HISTORY_LENGTH 100

typedef struct {
    double values[LSM_HISTORY_LENGTH];
    size_t count;
    size_t head;
} LsmSampleHistory;

/**
 * Reset a fixed-capacity graph history to the empty state.
 *
 * @param [out] history History object to initialise.
 */
void lsm_sample_history_init(LsmSampleHistory *history);
/**
 * Append one sample while preserving the configured time direction.
 *
 * Once capacity is reached, the oldest logical sample is overwritten in O(1)
 * time; no allocation or element shifting occurs.
 *
 * @param [in,out] history Circular history receiving the sample.
 * @param [in] value Numeric sample to append.
 * @param [in] newer_on_right true for oldest-to-newest display order.
 */
void lsm_sample_history_push(LsmSampleHistory *history, double value,
                             bool newer_on_right);
/**
 * Read a sample by logical oldest-to-newest index.
 *
 * @param [in] history History to inspect.
 * @param [in] logical_index Zero-based logical index.
 * @return Stored value, or 0.0 when the history or index is invalid.
 */
double lsm_sample_history_get(const LsmSampleHistory *history,
                              size_t logical_index);

#endif
