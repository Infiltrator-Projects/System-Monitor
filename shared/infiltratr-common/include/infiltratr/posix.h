// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file posix.h
 * @brief POSIX file, path and monotonic-clock adapters for the shared C core.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATR_COMMON_POSIX_H
#define INFILTRATR_COMMON_POSIX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Resolve an existing path into caller-owned bounded storage. */
bool infiltratr_realpath_copy(const char *path, char *destination, size_t size);
/** Concatenate two path fragments without inserting a separator. */
bool infiltratr_path_concat(char *destination, size_t size,
                            const char *base, const char *suffix);
/** Join two path fragments with exactly one separator at their boundary. */
bool infiltratr_path_join(char *destination, size_t size,
                          const char *left, const char *right);
/** Read one bounded pseudo-file value and remove its line ending. */
bool infiltratr_read_text_file(const char *path, char *buffer, size_t size);
/** Parse a complete unsigned decimal value from a small text file. */
bool infiltratr_read_u64_file(const char *path, uint64_t *value);
/** Read an unsigned value, mapping unavailable or invalid input to zero. */
uint64_t infiltratr_read_u64_or_zero(const char *path);
/** Parse a complete finite floating-point value from a small text file. */
bool infiltratr_read_double_file(const char *path, double *value);
/** Read a finite floating-point value, mapping failure to NAN. */
double infiltratr_read_double_or_nan(const char *path);
/** Read the first available unsigned attribute below a shared base path. */
bool infiltratr_read_first_u64(const char *base,
                               const char *const *suffixes,
                               size_t suffix_count, uint64_t *value);
/** Return CLOCK_MONOTONIC in fractional seconds, or zero on failure. */
double infiltratr_monotonic_seconds(void);

#ifdef __cplusplus
}
#endif

#endif
