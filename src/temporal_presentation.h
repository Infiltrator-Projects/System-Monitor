// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file temporal_presentation.h
 * @brief Human-facing civil-time presentation under system temporal policy.
 *
 * Canonical timestamps and measured elapsed seconds remain unchanged. These
 * helpers affect presentation only: civil timestamps and human-facing elapsed
 * values follow temporal-v3 policy wherever the selected clock defines a real
 * interval representation. Internal sampling, accounting and persistence stay
 * in canonical SI/Unix units.
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
/**
 * Format an abstract/accumulated duration under the active system policy.
 *
 * Use this for quantities such as accumulated CPU or active time that are not
 * one contiguous civil interval. The stored/measured value remains SI seconds.
 *
 * @param elapsed_seconds Canonical accumulated SI seconds.
 * @param buffer Destination text buffer.
 * @param capacity Destination capacity including the terminating NUL.
 * @return true when the complete presentation was produced.
 */
bool lsm_temporal_format_duration_seconds(uint64_t elapsed_seconds,
                                          char *buffer,
                                          size_t capacity);

/**
 * Format a contiguous elapsed interval ending now, such as system uptime or a
 * process age. Date-dependent modes receive the real civil endpoint.
 *
 * @param elapsed_seconds Canonical elapsed SI seconds ending at the current instant.
 * @param buffer Destination text buffer.
 * @param capacity Destination capacity including the terminating NUL.
 * @return true when the complete presentation was produced.
 */
bool lsm_temporal_format_elapsed_seconds(uint64_t elapsed_seconds,
                                         char *buffer,
                                         size_t capacity);

/**
 * Format a future duration estimate beginning now. Without System Settings,
 * the historical compact remaining-time presentation is preserved.
 *
 * @param elapsed_seconds Canonical estimated SI seconds remaining.
 * @param buffer Destination text buffer.
 * @param capacity Destination capacity including the terminating NUL.
 * @return true when the complete presentation was produced or zero maps to N/A.
 */
bool lsm_temporal_format_remaining_seconds(uint64_t elapsed_seconds,
                                           char *buffer,
                                           size_t capacity);
#ifdef LSM_TEMPORAL_PRESENTATION_TEST_API
/** Clear the short-lived policy cache for deterministic regression tests. */
void lsm_temporal_presentation_reset_cache_for_test(void);
#endif
#endif
