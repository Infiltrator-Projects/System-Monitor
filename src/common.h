// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file common.h
 * @brief Stable application facade over the shared Infiltratr Common library.
 *
 * This compatibility surface preserves established `lsm_` call sites while
 * the reusable implementation lives in `src/infiltratr-common`. That keeps
 * collectors stable and lets Calendar Plus and future C programs consume the
 * same tested code without adopting Linux System Monitor naming.
 *
 * Unless stated otherwise, functions are re-entrant and do not retain pointers
 * supplied by the caller. Output buffers are always NUL-terminated when their
 * size is non-zero.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_COMMON_H
#define LINUX_SYSTEM_MONITOR_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <infiltratr/core.h>

/** Return the compile-time element count of a true C array. */
#define LSM_ARRAY_LENGTH(array) INFILTRATR_ARRAY_LENGTH(array)

/**
 * Copy a possibly-null string into a fixed-size destination.
 *
 * The value is truncated when necessary. Source and destination must not
 * overlap; use memmove directly for overlapping regions.
 *
 * @param destination Writable output buffer.
 * @param size Capacity of @p destination in bytes.
 * @param source NUL-terminated source string, or NULL for an empty string.
 */
void lsm_copy_string(char *destination, size_t size, const char *source);

/**
 * Remove leading and trailing C-locale whitespace in place.
 *
 * @param text Mutable NUL-terminated string, or NULL.
 */
void lsm_trim(char *text);

/**
 * Remove trailing carriage-return and line-feed characters in place.
 *
 * @param text Mutable NUL-terminated string, or NULL.
 */
void lsm_trim_line_end(char *text);

/**
 * Compare two possibly-null strings for equality.
 *
 * @param left First string, or NULL.
 * @param right Second string, or NULL.
 * @return true when both are NULL or contain the same bytes.
 */
bool lsm_string_equal(const char *left, const char *right);

/**
 * Test whether a string begins with a prefix.
 *
 * @param text String to inspect, or NULL.
 * @param prefix Prefix to match, or NULL.
 * @return true only when both inputs are non-null and the prefix matches.
 */
bool lsm_string_starts_with(const char *text, const char *prefix);

/**
 * Test whether a string ends with a suffix.
 *
 * @param text String to inspect, or NULL.
 * @param suffix Suffix to match, or NULL.
 * @return true only when both inputs are non-null and the suffix matches.
 */
bool lsm_string_ends_with(const char *text, const char *suffix);

/**
 * Parse a complete unsigned value in base 0 or a base from 2 through 36.
 *
 * @param text Text to parse.
 * @param base Numeric base; zero enables conventional C prefixes.
 * @param value Receives the parsed value on success.
 * @return true only for complete, non-negative, in-range input.
 */
bool lsm_parse_u64(const char *text, unsigned int base, uint64_t *value);


/**
 * Clamp a floating-point value to inclusive bounds.
 *
 * @param value Value to constrain.
 * @param lower Inclusive lower bound.
 * @param upper Inclusive upper bound.
 * @return The bounded value; invalid bounds or NAN leave @p value unchanged.
 */
double lsm_clamp_double(double value, double lower, double upper);

/**
 * Resolve a path into caller-owned storage.
 *
 * @param path Existing path to resolve.
 * @param destination Writable output buffer.
 * @param size Capacity of @p destination in bytes.
 * @return true when realpath(3) succeeded and the result fitted completely.
 */
bool lsm_realpath_copy(const char *path, char *destination, size_t size);

/**
 * Concatenate a base path and suffix without implicit separators.
 *
 * @param destination Writable output buffer.
 * @param size Capacity of @p destination in bytes.
 * @param base First NUL-terminated component.
 * @param suffix Second NUL-terminated component.
 * @return true when the complete result fitted; false leaves an empty output.
 */
bool lsm_join_path(char *destination, size_t size,
                   const char *base, const char *suffix);

/**
 * Read a small text attribute using one open/read/close sequence.
 *
 * This function is intended for procfs, sysfs and similarly small pseudo-files.
 * It avoids stdio buffering and heap allocation, reads at most @p size - 1
 * bytes, terminates the result and removes a trailing CR/LF sequence.
 *
 * @param path File to read.
 * @param buffer Writable output buffer.
 * @param size Capacity of @p buffer in bytes; must be at least two.
 * @return true when at least one byte was read successfully.
 */
bool lsm_read_text_file(const char *path, char *buffer, size_t size);

/**
 * Read a complete unsigned base-10 integer from a small text file.
 *
 * Leading and trailing whitespace is accepted. Signs, overflow and trailing
 * non-whitespace characters are rejected.
 *
 * @param path File to parse.
 * @param value Receives the parsed value on success.
 * @return true only for a complete, in-range unsigned decimal value.
 */
bool lsm_read_u64_file(const char *path, uint64_t *value);

/**
 * Read an unsigned value, returning zero when unavailable or invalid.
 *
 * Use this only where zero and unavailable are intentionally equivalent. Live
 * metrics that must distinguish a genuine zero should call lsm_read_u64_file().
 *
 * @param path File to parse.
 * @return Parsed value, or zero on failure.
 */
uint64_t lsm_read_u64_or_zero(const char *path);

/**
 * Read a finite floating-point value from a small text file.
 *
 * @param path File to parse.
 * @param value Receives the finite parsed value on success.
 * @return true only when the complete text represents a finite number.
 */
bool lsm_read_double_file(const char *path, double *value);

/**
 * Read a floating-point value, returning NAN when unavailable or invalid.
 *
 * @param path File to parse.
 * @return Parsed finite value, or NAN on failure.
 */
double lsm_read_double_or_nan(const char *path);


/**
 * Add two unsigned quantities without allowing wraparound.
 *
 * @param [in] left First operand.
 * @param [in] right Second operand.
 * @return Exact sum, or UINT64_MAX when the mathematical result is larger.
 */
uint64_t lsm_u64_add_saturating(uint64_t left, uint64_t right);

/**
 * Multiply two unsigned quantities without allowing wraparound.
 *
 * @param [in] left First operand.
 * @param [in] right Second operand.
 * @return Exact product, or UINT64_MAX when the mathematical result is larger.
 */
uint64_t lsm_u64_multiply_saturating(uint64_t left, uint64_t right);

/**
 * Calculate a bounded percentage from unsigned quantities.
 *
 * @param [in] part Numerator.
 * @param [in] whole Denominator.
 * @return Percentage in the inclusive range 0..100, or zero when @p whole is
 *         zero.
 */
double lsm_percent_u64(uint64_t part, uint64_t whole);

/**
 * Convert a monotonic unsigned-counter delta into a rate.
 *
 * Counter rollback, a non-positive or non-finite interval, and a non-finite
 * scale are rejected rather than being converted into spikes.
 *
 * @param [in] current Current counter value.
 * @param [in] previous Previous counter value.
 * @param [in] units_per_count Units represented by one counter increment.
 * @param [in] elapsed_seconds Monotonic interval between the samples.
 * @param [out] rate Receives units per second on success, or zero on failure.
 * @return true when the rate was calculated from a valid monotonic interval.
 */
bool lsm_u64_counter_rate(uint64_t current, uint64_t previous,
                          long double units_per_count,
                          double elapsed_seconds, double *rate);

/**
 * Return CLOCK_MONOTONIC as fractional seconds.
 *
 * @return Monotonic seconds, or 0.0 when the clock query fails.
 */
double lsm_monotonic_seconds(void);

/**
 * Format a byte count using traditional binary-scaled labels.
 *
 * @param bytes Quantity to format.
 * @param buffer Writable output buffer.
 * @param buffer_size Capacity of @p buffer in bytes.
 * @return @p buffer for convenient expression chaining.
 */
char *lsm_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size);

/**
 * Format a byte-per-second rate using traditional binary-scaled labels.
 *
 * Negative and non-finite rates are normalised to zero because they represent
 * invalid counter deltas rather than meaningful throughput.
 *
 * @param bytes_per_second Rate to format.
 * @param buffer Writable output buffer.
 * @param buffer_size Capacity of @p buffer in bytes.
 * @return @p buffer for convenient expression chaining.
 */
char *lsm_format_rate(double bytes_per_second, char *buffer,
                      size_t buffer_size);

#endif
