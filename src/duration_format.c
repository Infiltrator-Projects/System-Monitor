// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file duration_format.c
 * @brief System Monitor compatibility facade over Common duration formatting.
 *
 * Generic rendering lives in Infiltratr Common. System Monitor retains only
 * the domain rule that a zero remaining-time estimate means unavailable.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "duration_format.h"

#include <infiltratr/format.h>

void lsm_duration_format_clock(uint64_t seconds, char *buffer, size_t size)
{
    (void)infiltratr_format_duration_clock(seconds, buffer, size);
}

void lsm_duration_format_remaining(uint64_t seconds, char *buffer, size_t size)
{
    (void)infiltratr_format_duration_compact(
        seconds != 0U, seconds, buffer, size);
}
