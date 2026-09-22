// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_backend_linux_internal.h
 * @brief Private Linux process-record parsing contracts used by implementation tests.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_BACKEND_LINUX_INTERNAL_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_BACKEND_LINUX_INTERNAL_H

#include <stdbool.h>

/**
 * Parse the first uptime field from one Linux /proc/uptime record.
 *
 * A complete numeric token may be followed by end-of-string or C-locale
 * whitespace. Other trailing bytes immediately after the token are rejected so
 * malformed records cannot be accepted merely because their prefix is numeric.
 *
 * @param [in] text Complete procfs record with line ending already removed.
 * @param [out] uptime Receives a finite non-negative uptime value on success.
 * @return true when the first field is syntactically complete and valid.
 */
bool lsm_process_linux_parse_uptime_record(const char *text, double *uptime);

#endif
