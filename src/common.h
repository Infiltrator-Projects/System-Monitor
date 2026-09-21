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
 * @copyright Copyright (c) 2000-2026 Shannon Smith
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
#define lsm_ascii_is_space infiltratr_ascii_is_space
#define lsm_ascii_is_alpha infiltratr_ascii_is_alpha
#define lsm_ascii_is_digit infiltratr_ascii_is_digit
#define lsm_ascii_is_alnum infiltratr_ascii_is_alnum
#define lsm_ascii_is_xdigit infiltratr_ascii_is_xdigit
#define lsm_ascii_to_lower infiltratr_ascii_to_lower
#define lsm_ascii_to_upper infiltratr_ascii_to_upper
#define lsm_ascii_equal_ci infiltratr_ascii_equal_ci
#define lsm_ascii_compare_ci infiltratr_ascii_compare_ci
#define lsm_ascii_starts_with_ci infiltratr_ascii_starts_with_ci
#define lsm_ascii_contains_ci infiltratr_ascii_contains_ci
#define LSM_FNV1A64_OFFSET_BASIS INFILTRATR_FNV1A64_OFFSET_BASIS
#define lsm_fnv1a64_mix_byte infiltratr_fnv1a64_mix_byte
#define lsm_fnv1a64_mix_text infiltratr_fnv1a64_mix_text
#define lsm_fnv1a64_mix_u64_le infiltratr_fnv1a64_mix_u64_le
#define lsm_parse_u64 infiltratr_parse_u64
#define lsm_parse_i64 infiltratr_parse_i64
#define lsm_parse_u64_range infiltratr_parse_u64_range
#define lsm_parse_i64_range infiltratr_parse_i64_range
#define lsm_parse_u64_token infiltratr_parse_u64_token
#define lsm_parse_i64_token infiltratr_parse_i64_token
#define lsm_parse_double_token infiltratr_parse_double_token
#define lsm_array_reserve infiltratr_array_reserve
#define lsm_clamp_double infiltratr_clamp_double
#define lsm_realpath_copy infiltratr_realpath_copy
#define lsm_home_directory infiltratr_posix_home_directory
#define lsm_xdg_config_home infiltratr_xdg_config_home
#define lsm_xdg_data_home infiltratr_xdg_data_home
#define lsm_mkdir_parents infiltratr_mkdir_parents
#define lsm_path_basename infiltratr_path_basename
#define lsm_path_dirname infiltratr_path_dirname
#define lsm_join_path infiltratr_path_join
#define lsm_read_text_file infiltratr_read_text_file
#define lsm_read_text_file_alloc infiltratr_read_text_file_alloc
#define lsm_read_u64_file infiltratr_read_u64_file
#define lsm_read_u64_or_zero infiltratr_read_u64_or_zero
#define lsm_read_double_file infiltratr_read_double_file
#define lsm_read_double_or_nan infiltratr_read_double_or_nan
#define lsm_read_first_u64 infiltratr_read_first_u64
#define lsm_u64_add_checked infiltratr_u64_add_checked
#define lsm_u64_add_saturating infiltratr_u64_add_saturating
#define lsm_u64_multiply_checked infiltratr_u64_multiply_checked
#define lsm_u64_multiply_saturating infiltratr_u64_multiply_saturating
#define lsm_size_add_checked infiltratr_size_add_checked
#define lsm_size_multiply_checked infiltratr_size_multiply_checked
#define lsm_percent_u64 infiltratr_percent_u64
#define lsm_u64_counter_delta infiltratr_u64_counter_delta
#define lsm_u64_counter_rate infiltratr_u64_counter_rate
#define lsm_monotonic_seconds infiltratr_monotonic_seconds
#define lsm_posix_deadline_after_milliseconds \
    infiltratr_posix_deadline_after_milliseconds
#define lsm_posix_deadline_remaining_milliseconds \
    infiltratr_posix_deadline_remaining_milliseconds
#define lsm_format_bytes infiltratr_format_bytes
#define lsm_format_rate infiltratr_format_rate

#endif
