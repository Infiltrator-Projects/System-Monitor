// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file duration_format.c
 * @brief Consistent elapsed-time presentation for System Monitor views.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "duration_format.h"

#include <stdio.h>

void lsm_duration_format_clock(uint64_t seconds, char *buffer, size_t size)
{
    if (!buffer || size == 0U) return;
    const uint64_t days = seconds / 86400ULL;
    seconds %= 86400ULL;
    const unsigned hours = (unsigned)(seconds / 3600ULL);
    const unsigned minutes = (unsigned)((seconds % 3600ULL) / 60ULL);
    const unsigned remainder = (unsigned)(seconds % 60ULL);
    if (days > 0U)
        (void)snprintf(buffer, size, "%llud %02u:%02u:%02u",
                       (unsigned long long)days, hours, minutes, remainder);
    else
        (void)snprintf(buffer, size, "%02u:%02u:%02u",
                       hours, minutes, remainder);
}

void lsm_duration_format_remaining(uint64_t seconds, char *buffer, size_t size)
{
    if (!buffer || size == 0U) return;
    if (seconds == 0U) {
        (void)snprintf(buffer, size, "N/A");
        return;
    }

    const uint64_t days = seconds / 86400ULL;
    seconds %= 86400ULL;
    const unsigned hours = (unsigned)(seconds / 3600ULL);
    const unsigned minutes = (unsigned)((seconds % 3600ULL) / 60ULL);
    if (days > 0U)
        (void)snprintf(buffer, size, "%llud %02uh %02um",
                       (unsigned long long)days, hours, minutes);
    else if (hours > 0U)
        (void)snprintf(buffer, size, "%uh %02um", hours, minutes);
    else
        (void)snprintf(buffer, size, "%um", minutes);
}
