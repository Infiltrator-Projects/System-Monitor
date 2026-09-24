// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_backend_probe.c
 * @brief Minimal console probe for the experimental Windows backends.
 *
 * This executable is intentionally not the Windows GUI. It exists so early
 * native Windows monitor and process collectors can be exercised on a real
 * Windows machine before the remaining application backends and presentation
 * layer are ported.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_platform.h"
#include "process_backend.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

static double bytes_to_gb(uint64_t bytes)
{
    return (double)bytes / (1024.0 * 1024.0 * 1024.0);
}

int main(void)
{
    printf("System Monitor experimental Windows backend probe\n\n");

    LsmMonitor monitor;
    if (!lsm_monitor_platform_init(&monitor)) {
        fprintf(stderr, "Monitor backend initialisation failed (Windows error %lu).\n",
                (unsigned long)GetLastError());
        return EXIT_FAILURE;
    }

    Sleep(1000U);
    if (!lsm_monitor_platform_update(&monitor)) {
        fprintf(stderr, "Monitor backend update failed (Windows error %lu).\n",
                (unsigned long)GetLastError());
        lsm_monitor_platform_destroy(&monitor);
        return EXIT_FAILURE;
    }

    printf("CPU\n");
    printf("  Model: %s\n", monitor.cpu.model[0] ? monitor.cpu.model : "Unavailable");
    printf("  Logical processors: %u\n", monitor.cpu.logical_cores);
    printf("  Usage: %.1f%% (user %.1f%%, kernel %.1f%%)\n",
           monitor.cpu.usage_percent,
           monitor.cpu.user_percent,
           monitor.cpu.kernel_percent);
    printf("  Uptime: %llu seconds\n",
           (unsigned long long)monitor.cpu.uptime_seconds);
    printf("  Processes: %u  Threads: %u  Handles: %llu\n\n",
           monitor.cpu.process_count,
           monitor.cpu.thread_count,
           (unsigned long long)monitor.cpu.file_handle_count);

    printf("Memory\n");
    printf("  Physical: %.2f GB total, %.2f GB used, %.2f GB available\n",
           bytes_to_gb(monitor.memory.total_bytes),
           bytes_to_gb(monitor.memory.used_bytes),
           bytes_to_gb(monitor.memory.available_bytes));
    printf("  Usage: %.1f%%\n", monitor.memory.usage_percent);
    printf("  Commit: %.2f GB / %.2f GB\n\n",
           bytes_to_gb(monitor.memory.committed_bytes),
           bytes_to_gb(monitor.memory.commit_limit_bytes));

    LsmProcessBackend *process_backend = lsm_process_backend_create();
    if (!process_backend) {
        fprintf(stderr, "Process backend initialisation failed.\n");
        lsm_monitor_platform_destroy(&monitor);
        return EXIT_FAILURE;
    }

    LsmProcessInfo *processes = NULL;
    size_t process_count = lsm_process_scan(
        process_backend, &processes,
        LSM_PROCESS_SCAN_EXECUTABLE | LSM_PROCESS_SCAN_HANDLE_COUNT);
    lsm_process_list_free(processes);
    processes = NULL;

    Sleep(1000U);
    process_count = lsm_process_scan(
        process_backend, &processes,
        LSM_PROCESS_SCAN_EXECUTABLE | LSM_PROCESS_SCAN_HANDLE_COUNT);
    if (process_count == 0U || !processes) {
        char error[256];
        lsm_process_error_message(error, sizeof(error));
        fprintf(stderr, "Process scan failed: %s\n", error);
        lsm_process_backend_destroy(process_backend);
        lsm_monitor_platform_destroy(&monitor);
        return EXIT_FAILURE;
    }

    printf("Processes: %zu found; showing up to 25\n", process_count);
    printf("%-7s %-7s %-8s %-8s %-8s %-20s %s\n",
           "PID", "PPID", "CPU%", "MEM%", "THREADS", "USER", "NAME");

    const size_t shown = process_count < 25U ? process_count : 25U;
    for (size_t index = 0U; index < shown; index++) {
        const LsmProcessInfo *process = &processes[index];
        printf("%-7llu %-7llu %-8.2f %-8.2f %-8u %-20.20s %s\n",
               (unsigned long long)process->pid,
               (unsigned long long)process->ppid,
               process->cpu_percent,
               process->memory_percent,
               process->threads,
               process->user[0] ? process->user : "-",
               process->name[0] ? process->name : "-");
        if (process->executable[0])
            printf("        exe: %s\n", process->executable);
    }

    printf("\nRead-only backend probe completed successfully.\n");
    printf("Process control, services, users, startup apps and device backends ");
    printf("are intentionally not implemented in this release.\n");

    lsm_process_list_free(processes);
    lsm_process_backend_destroy(process_backend);
    lsm_monitor_platform_destroy(&monitor);
    return EXIT_SUCCESS;
}
