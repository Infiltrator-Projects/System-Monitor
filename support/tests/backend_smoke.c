// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file backend_smoke.c
 * @brief Live native monitoring-backend smoke test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor.h"
#include "process_backend.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    LsmMonitor monitor;
    if (!lsm_monitor_init(&monitor)) {
        fputs("monitor initialisation failed\n", stderr);
        return 1;
    }
    usleep(250000);
    if (!lsm_monitor_update(&monitor)) {
        fputs("monitor update failed\n", stderr);
        return 1;
    }

    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) return 1;
    LsmProcessInfo *processes = NULL;
    size_t process_count = lsm_process_scan(
        backend, &processes, LSM_PROCESS_SCAN_NONE);
    uint64_t expected_threads = 0U;
    for (size_t index = 0U; index < process_count; index++)
        expected_threads += processes[index].threads;
    lsm_monitor_set_process_totals(&monitor, processes, process_count);

    bool process_totals_valid = monitor.cpu.process_count == process_count
        && monitor.cpu.thread_count == expected_threads;
    printf("CPU: %.1f%%, logical processors: %u\n",
           monitor.cpu.usage_percent, monitor.cpu.logical_cores);
    printf("Memory: %.1f%%, disks: %zu, networks: %zu, GPUs: %zu\n",
           monitor.memory.usage_percent, monitor.disk_count,
           monitor.net_count, monitor.gpu_count);
    printf("Processes visible: %zu, threads: %u\n",
           process_count, monitor.cpu.thread_count);

    bool monitor_valid = monitor.cpu.logical_cores > 0
        && monitor.memory.total_bytes > 0 && process_totals_valid;

    lsm_process_list_free(processes);
    lsm_process_backend_destroy(backend);
    lsm_monitor_destroy(&monitor);
    return monitor_valid ? 0 : 1;
}
