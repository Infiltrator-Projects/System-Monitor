// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file duration_format.h
 * @brief System Monitor duration policy over Common formatting.
 *
 * Generic rendering is owned by Infiltratr Common. System Monitor retains only
 * the domain rule that a zero remaining-time estimate means unavailable.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_DURATION_FORMAT_H
#define LINUX_SYSTEM_MONITOR_DURATION_FORMAT_H

#include <infiltratr/format.h>

#include <stddef.h>
#include <stdint.h>

static inline void lsm_duration_format_clock(uint64_t seconds,
                                             char *buffer, size_t size)
{
    (void)infiltratr_format_duration_clock(seconds, buffer, size);
}

static inline void lsm_duration_format_remaining(uint64_t seconds,
                                                 char *buffer, size_t size)
{
    (void)infiltratr_format_duration_compact(
        seconds != 0U, seconds, buffer, size);
}

#endif
