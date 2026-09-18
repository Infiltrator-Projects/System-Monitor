// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file common.h
 * @brief Stable System Monitor names mapped directly to Infiltratr Common.
 *
 * This header keeps established lsm_ call sites readable while eliminating
 * the former wrapper translation unit. Reusable implementation and contracts
 * are owned entirely by the pinned Infiltratr Common library.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_COMMON_H
#define LINUX_SYSTEM_MONITOR_COMMON_H

#include <infiltratr/arithmetic.h>
#include <infiltratr/core.h>
#include <infiltratr/posix.h>
#include <infiltratr/posix_path.h>
#include <infiltratr/token.h>

#define LSM_ARRAY_LENGTH(array) INFILTRATR_ARRAY_LENGTH(array)

#define lsm_copy_string infiltratr_copy_string
#define lsm_trim infiltratr_trim
#define lsm_trim_line_end infiltratr_trim_line_end
#define lsm_string_equal infiltratr_string_equal
#define lsm_string_starts_with infiltratr_string_starts_with
#define lsm_string_ends_with infiltratr_string_ends_with
#define lsm_parse_u64 infiltratr_parse_u64
#define lsm_parse_u64_token infiltratr_parse_u64_token
#define lsm_array_reserve infiltratr_array_reserve
#define lsm_clamp_double infiltratr_clamp_double
#define lsm_realpath_copy infiltratr_realpath_copy
#define lsm_path_basename infiltratr_path_basename
#define lsm_join_path infiltratr_path_concat
#define lsm_read_text_file infiltratr_read_text_file
#define lsm_read_u64_file infiltratr_read_u64_file
#define lsm_read_u64_or_zero infiltratr_read_u64_or_zero
#define lsm_read_double_file infiltratr_read_double_file
#define lsm_read_double_or_nan infiltratr_read_double_or_nan
#define lsm_u64_add_saturating infiltratr_u64_add_saturating
#define lsm_u64_multiply_saturating infiltratr_u64_multiply_saturating
#define lsm_percent_u64 infiltratr_percent_u64
#define lsm_u64_counter_rate infiltratr_u64_counter_rate
#define lsm_monotonic_seconds infiltratr_monotonic_seconds
#define lsm_format_bytes infiltratr_format_bytes
#define lsm_format_rate infiltratr_format_rate

#endif
