// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_platform_smoke.c
 * @brief Proves the public monitor lifecycle can use a non-Linux test backend.
 *
 * This test deliberately links monitor.c without any Linux collector source.
 * Its tiny synthetic backend satisfies monitor_platform.h and verifies that
 * lifecycle forwarding, topology requests and common process-total accounting
 * remain independent of procfs, sysfs, udev and Linux collector state.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor.h"
#include "monitor_platform.h"

#include <stdio.h>
#include <string.h>

static unsigned init_calls;
static unsigned update_calls;
static unsigned topology_calls;
static unsigned destroy_calls;

bool lsm_monitor_platform_init(LsmMonitor *monitor)
{
    if (!monitor) return false;
    memset(monitor, 0, sizeof(*monitor));
    init_calls++;
    monitor->cpu.logical_cores = 4U;
    snprintf(monitor->cpu.model, sizeof(monitor->cpu.model), "Synthetic CPU");
    monitor->backend_state = monitor;
    return true;
}

bool lsm_monitor_platform_update(LsmMonitor *monitor)
{
    if (!monitor || monitor->backend_state != monitor) return false;
    update_calls++;
    monitor->cpu.usage_percent = 12.5;
    return true;
}

void lsm_monitor_platform_request_topology_refresh(LsmMonitor *monitor)
{
    if (monitor && monitor->backend_state == monitor) topology_calls++;
}

void lsm_monitor_platform_destroy(LsmMonitor *monitor)
{
    if (!monitor) return;
    destroy_calls++;
    monitor->backend_state = NULL;
}

int main(void)
{
    LsmMonitor monitor = {0};
    if (!lsm_monitor_init(&monitor) || init_calls != 1U) return 1;
    if (strcmp(monitor.cpu.model, "Synthetic CPU") != 0) return 2;
    if (!lsm_monitor_update(&monitor) || update_calls != 1U ||
        monitor.cpu.usage_percent != 12.5) return 3;

    lsm_monitor_request_topology_refresh(&monitor);
    if (topology_calls != 1U) return 4;

    LsmProcessInfo processes[2] = {0};
    processes[0].threads = 3U;
    processes[1].threads = 5U;
    lsm_monitor_set_process_totals(&monitor, processes, 2U);
    if (monitor.cpu.process_count != 2U || monitor.cpu.thread_count != 8U)
        return 5;

    lsm_monitor_destroy(&monitor);
    if (destroy_calls != 1U || monitor.backend_state != NULL) return 6;

    puts("Platform-neutral monitor lifecycle smoke test passed.");
    return 0;
}
