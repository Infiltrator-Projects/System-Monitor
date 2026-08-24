// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file common.c
 * @brief Linux System Monitor compatibility facade over Infiltratr Common.
 *
 * Existing collectors retain the stable `lsm_` API while every implementation
 * is owned by the versioned common C library. New reusable functionality
 * belongs in that library; this file should contain delegation only.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "common.h"

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/posix.h>
#include <infiltratr/posix_path.h>
#include <infiltratr/token.h>

void lsm_copy_string(char *destination, size_t size, const char *source)
{
    infiltratr_copy_string(destination, size, source);
}

void lsm_trim(char *text)
{
    infiltratr_trim(text);
}

void lsm_trim_line_end(char *text)
{
    infiltratr_trim_line_end(text);
}

bool lsm_string_equal(const char *left, const char *right)
{
    return infiltratr_string_equal(left, right);
}

bool lsm_string_starts_with(const char *text, const char *prefix)
{
    return infiltratr_string_starts_with(text, prefix);
}

bool lsm_string_ends_with(const char *text, const char *suffix)
{
    return infiltratr_string_ends_with(text, suffix);
}

bool lsm_parse_u64(const char *text, unsigned int base, uint64_t *value)
{
    return infiltratr_parse_u64(text, base, value);
}

bool lsm_parse_u64_token(const char **cursor, unsigned int base,
                         uint64_t *value)
{
    return infiltratr_parse_u64_token(cursor, base, value);
}

bool lsm_array_reserve(void **array, size_t *capacity, size_t element_size,
                       size_t required, size_t initial_capacity)
{
    return infiltratr_array_reserve(array, capacity, element_size, required,
                                    initial_capacity);
}

double lsm_clamp_double(double value, double lower, double upper)
{
    return infiltratr_clamp_double(value, lower, upper);
}

bool lsm_realpath_copy(const char *path, char *destination, size_t size)
{
    return infiltratr_realpath_copy(path, destination, size);
}

const char *lsm_path_basename(const char *path)
{
    return infiltratr_path_basename(path);
}

bool lsm_join_path(char *destination, size_t size,
                   const char *base, const char *suffix)
{
    return infiltratr_path_concat(destination, size, base, suffix);
}

bool lsm_read_text_file(const char *path, char *buffer, size_t size)
{
    return infiltratr_read_text_file(path, buffer, size);
}

bool lsm_read_u64_file(const char *path, uint64_t *value)
{
    return infiltratr_read_u64_file(path, value);
}

uint64_t lsm_read_u64_or_zero(const char *path)
{
    return infiltratr_read_u64_or_zero(path);
}

bool lsm_read_double_file(const char *path, double *value)
{
    return infiltratr_read_double_file(path, value);
}

double lsm_read_double_or_nan(const char *path)
{
    return infiltratr_read_double_or_nan(path);
}

uint64_t lsm_u64_add_saturating(uint64_t left, uint64_t right)
{
    return infiltratr_u64_add_saturating(left, right);
}

uint64_t lsm_u64_multiply_saturating(uint64_t left, uint64_t right)
{
    return infiltratr_u64_multiply_saturating(left, right);
}

double lsm_percent_u64(uint64_t part, uint64_t whole)
{
    return infiltratr_percent_u64(part, whole);
}

bool lsm_u64_counter_rate(uint64_t current, uint64_t previous,
                          long double units_per_count,
                          double elapsed_seconds, double *rate)
{
    return infiltratr_u64_counter_rate(current, previous, units_per_count,
                                       elapsed_seconds, rate);
}

double lsm_monotonic_seconds(void)
{
    return infiltratr_monotonic_seconds();
}

char *lsm_format_bytes(uint64_t bytes, char *buffer, size_t buffer_size)
{
    return infiltratr_format_bytes(bytes, buffer, buffer_size);
}

char *lsm_format_rate(double bytes_per_second, char *buffer,
                      size_t buffer_size)
{
    return infiltratr_format_rate(bytes_per_second, buffer, buffer_size);
}
