// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file duration_format.h
 * @brief System Monitor remaining-time policy over Common formatting.
 *
 * Generic elapsed-time rendering is called directly from Infiltratr Common.
 * System Monitor retains only the domain rule that a zero remaining-time
 * estimate means unavailable.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_DURATION_FORMAT_H
#define INFILTRATOR_SYSTEM_MONITOR_DURATION_FORMAT_H

#include "temporal_presentation.h"

#include <stddef.h>
#include <stdint.h>

/**
 * Present an estimated remaining duration using the active temporal policy.
 *
 * @param seconds Canonical estimated SI seconds remaining; zero means unavailable.
 * @param buffer Destination text buffer.
 * @param size Destination capacity including the terminating NUL.
 */
static inline void lsm_duration_format_remaining(uint64_t seconds,
                                                 char *buffer, size_t size)
{
    (void)lsm_temporal_format_remaining_seconds(
        seconds, buffer, size);
}

#endif
