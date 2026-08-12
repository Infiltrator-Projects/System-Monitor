// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor.c
 * @brief Platform-neutral lifecycle for the retained monitoring snapshot.
 *
 * The application owns one plain-C LsmMonitor model. Operating-system
 * discovery, cadence policy, cumulative baselines and native resource handles
 * are delegated through monitor_platform.h, so this module contains no Linux
 * collector knowledge. A new operating-system port supplies another native
 * implementation of that internal contract while retaining this lifecycle and
 * the presentation-facing snapshot API.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor.h"

#include "monitor_platform.h"

#include <limits.h>

bool lsm_monitor_init(LsmMonitor *monitor)
{
    return lsm_monitor_platform_init(monitor);
}

bool lsm_monitor_update(LsmMonitor *monitor)
{
    return lsm_monitor_platform_update(monitor);
}

void lsm_monitor_request_topology_refresh(LsmMonitor *monitor)
{
    lsm_monitor_platform_request_topology_refresh(monitor);
}

void lsm_monitor_set_process_totals(LsmMonitor *monitor,
                                    const LsmProcessInfo *processes,
                                    size_t process_count)
{
    if (!monitor) return;
    uint64_t thread_count = 0U;
    for (size_t index = 0U; index < process_count; index++) {
        const uint64_t threads = processes ? processes[index].threads : 0U;
        thread_count = UINT64_MAX - thread_count < threads
            ? UINT64_MAX : thread_count + threads;
    }
    monitor->cpu.process_count = process_count > UINT_MAX
        ? UINT_MAX : (unsigned)process_count;
    monitor->cpu.thread_count = thread_count > UINT_MAX
        ? UINT_MAX : (unsigned)thread_count;
}

void lsm_monitor_destroy(LsmMonitor *monitor)
{
    lsm_monitor_platform_destroy(monitor);
}
