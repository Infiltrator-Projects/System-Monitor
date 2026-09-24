// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal_presentation.h
 * @brief Human-facing civil-time presentation under system temporal policy.
 *
 * Canonical timestamps and elapsed durations remain unchanged. These helpers
 * affect presentation only: when System Settings publishes temporal-v3 policy,
 * visible civil timestamps use that clock mode; otherwise native OS locale
 * formatting remains authoritative.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LSM_TEMPORAL_PRESENTATION_H
#define LSM_TEMPORAL_PRESENTATION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/**
 * Format a canonical Unix-microsecond instant for human display.
 * @param unix_microseconds Canonical Unix instant in microseconds.
 * @param include_date Prepend the native local-calendar date when true.
 * @param include_zone Append the native numeric UTC offset when true.
 * @param native_show_seconds Fallback precision when no authority is active.
 * @param buffer Destination text buffer.
 * @param capacity Destination capacity including the terminating NUL.
 * @return true when the complete presentation was produced.
 */
bool lsm_temporal_format_epoch_microseconds(int64_t unix_microseconds,
                                            bool include_date,
                                            bool include_zone,
                                            bool native_show_seconds,
                                            char *buffer,
                                            size_t capacity);
/**
 * Format a whole-second Unix instant for human display.
 * @param unix_seconds Canonical Unix instant in whole seconds.
 * @param include_date Prepend the native local-calendar date when true.
 * @param include_zone Append the native numeric UTC offset when true.
 * @param native_show_seconds Fallback precision when no authority is active.
 * @param buffer Destination text buffer.
 * @param capacity Destination capacity including the terminating NUL.
 * @return true when the complete presentation was produced.
 */
bool lsm_temporal_format_epoch_seconds(int64_t unix_seconds,
                                       bool include_date,
                                       bool include_zone,
                                       bool native_show_seconds,
                                       char *buffer,
                                       size_t capacity);
#ifdef LSM_TEMPORAL_PRESENTATION_TEST_API
/** Clear the short-lived policy cache for deterministic regression tests. */
void lsm_temporal_presentation_reset_cache_for_test(void);
#endif
#endif
