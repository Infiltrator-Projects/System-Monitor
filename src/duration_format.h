// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file duration_format.h
 * @brief Consistent elapsed-time presentation for System Monitor views.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_DURATION_FORMAT_H
#define LINUX_SYSTEM_MONITOR_DURATION_FORMAT_H

#include <stddef.h>
#include <stdint.h>

/**
 * Format seconds as HH:MM:SS, prefixed with whole days when necessary.
 *
 * @param seconds Duration to format.
 * @param buffer Destination text buffer.
 * @param size Size of @p buffer in bytes.
 */
void lsm_duration_format_clock(uint64_t seconds, char *buffer, size_t size);

/**
 * Format a nonzero estimated duration using compact day/hour/minute units.
 *
 * A zero estimate is presented as N/A because battery backends use zero when
 * no reliable remaining-time estimate is available.
 *
 * @param seconds Estimated duration to format.
 * @param buffer Destination text buffer.
 * @param size Size of @p buffer in bytes.
 */
void lsm_duration_format_remaining(uint64_t seconds, char *buffer, size_t size);

#endif
