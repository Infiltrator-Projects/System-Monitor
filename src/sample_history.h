// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file sample_history.h
 * @brief Fixed-length circular sample history used by performance graphs.
 *
 * The name deliberately distinguishes short-lived graph samples from the
 * persistent application-history feature implemented in history.c.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_SAMPLE_HISTORY_H
#define INFILTRATOR_SYSTEM_MONITOR_SAMPLE_HISTORY_H

#include <stdbool.h>
#include <stddef.h>

#define LSM_HISTORY_LENGTH 100

/**
 * Fixed-capacity circular history backing one performance graph series.
 *
 * Slots are addressed through the logical oldest-to-newest API rather than by
 * exposing the physical ring position. A slot may be present in the retained
 * time window but marked unavailable when no finite sample existed for that
 * interval.
 */
typedef struct {
    double values[LSM_HISTORY_LENGTH]; /**< Retained numeric samples. */
    bool valid[LSM_HISTORY_LENGTH];    /**< Whether each physical slot is a real sample. */
    size_t count;                      /**< Number of retained logical samples. */
    size_t head;                       /**< Physical index of the next insertion slot. */
} LsmSampleHistory;

/**
 * Reset a fixed-capacity graph history to the empty state.
 *
 * The logical time window is retained immediately, but all slots begin as
 * unavailable rather than as synthetic zero measurements.
 *
 * @param [out] history History object to initialise.
 */
void lsm_sample_history_init(LsmSampleHistory *history);
/**
 * Append one sample while preserving the configured time direction.
 *
 * Non-finite values advance the history clock but mark the destination slot as
 * unavailable. Once capacity is reached, the oldest logical sample is
 * overwritten in O(1) time; no allocation or element shifting occurs.
 *
 * @param [in,out] history Circular history receiving the sample.
 * @param [in] value Numeric sample, or a non-finite value for no sample.
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
/**
 * Report whether a logical history slot contains a real sample.
 *
 * @param [in] history History to inspect.
 * @param [in] logical_index Zero-based logical index.
 * @return true when the slot contains a finite sample.
 */
bool lsm_sample_history_is_valid(const LsmSampleHistory *history,
                                 size_t logical_index);

#endif
