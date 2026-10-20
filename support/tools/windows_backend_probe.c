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
#include "windows_process_runtime.h"
#include "windows_registry_string.h"

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

static DWORD WINAPI exited_process_worker(LPVOID user_data)
{
    (void)user_data;
    return 0U;
}

static bool verify_terminated_process_worker_unavailable(void)
{
    LsmWindowsProcessRuntime runtime = {0};
    InitializeCriticalSection(&runtime.process_lock);
    runtime.process_lock_initialised = true;
    runtime.process_thread = CreateThread(
        NULL, 0U, exited_process_worker, NULL, 0U, NULL);
    if (!runtime.process_thread) {
        DeleteCriticalSection(&runtime.process_lock);
        return false;
    }

    const DWORD stopped = WaitForSingleObject(runtime.process_thread, 2000U);
    const bool unavailable =
        stopped == WAIT_OBJECT_0 &&
        !lsm_windows_process_runtime_available(&runtime);
    const bool cleaned = lsm_windows_process_runtime_stop(&runtime);
    return unavailable && cleaned;
}

static bool wait_for_process_snapshot(LsmWindowsProcessRuntime *runtime)
{
    const ULONGLONG started = GetTickCount64();
    while (GetTickCount64() - started < 15000ULL) {
        LsmProcessInfo *processes = NULL;
        size_t count = 0U;
        if (lsm_windows_process_runtime_take(runtime, &processes, &count)) {
            const bool complete = processes != NULL;
            lsm_process_list_free(processes);
            return complete;
        }
        if (!lsm_windows_process_runtime_available(runtime)) return false;
        Sleep(20U);
    }
    return false;
}

/* A dead worker retains a valid HANDLE. The public start contract must
 * reap it before constructing a replacement, not mistake it for a live
 * backend. Keep test storage alive if native cancellation times out. */
static bool verify_terminated_process_worker_restart(void)
{
    LsmWindowsProcessRuntime *runtime = calloc(1U, sizeof(*runtime));
    if (!runtime) return false;
    InitializeCriticalSection(&runtime->process_lock);
    runtime->process_lock_initialised = true;
    runtime->process_thread = CreateThread(
        NULL, 0U, exited_process_worker, NULL, 0U, NULL);
    if (!runtime->process_thread) {
        DeleteCriticalSection(&runtime->process_lock);
        free(runtime);
        return false;
    }

    const bool exited =
        WaitForSingleObject(runtime->process_thread, 2000U) == WAIT_OBJECT_0;
    const bool restarted = exited &&
        lsm_windows_process_runtime_start(runtime) &&
        wait_for_process_snapshot(runtime);
    const bool stopped = lsm_windows_process_runtime_stop(runtime);
    if (stopped) free(runtime);
    return restarted && stopped;
}

/* A successful stop is not terminal: the same runtime must be able to
 * initialise a new worker and publish another complete snapshot. Allocate
 * separately so a timed-out cancellation cannot leave a worker using stack
 * storage after this test returns. */
static bool verify_process_worker_restart(void)
{
    LsmWindowsProcessRuntime *runtime = calloc(1U, sizeof(*runtime));
    if (!runtime) return false;

    const bool first = lsm_windows_process_runtime_start(runtime) &&
                       wait_for_process_snapshot(runtime);
    if (!lsm_windows_process_runtime_stop(runtime)) return false;

    const bool second = first &&
                        lsm_windows_process_runtime_start(runtime) &&
                        wait_for_process_snapshot(runtime);
    const bool stopped = lsm_windows_process_runtime_stop(runtime);
    if (stopped) free(runtime);
    return first && second && stopped;
}


/* A volatile, uniquely named HKCU key exercises the real Win32 registry
 * boundary without relying on machine GPU drivers or persistent test state.
 * Every path closes the key and removes the test entry. */
static bool verify_registry_string_boundary(void)
{
    char subkey[160];
    const int written = snprintf(
        subkey, sizeof(subkey),
        "Software\\Classes\\InfiltratorSystemMonitorRegistryProbe-%lu-%llu",
        (unsigned long)GetCurrentProcessId(),
        (unsigned long long)GetTickCount64());
    if (written < 0 || (size_t)written >= sizeof(subkey)) return false;

    HKEY key = NULL;
    DWORD disposition = 0U;
    if (RegCreateKeyExA(
            HKEY_CURRENT_USER, subkey, 0U, NULL, REG_OPTION_VOLATILE,
            KEY_READ | KEY_WRITE, NULL, &key, &disposition) != ERROR_SUCCESS)
        return false;

    bool passed = disposition == REG_CREATED_NEW_KEY;
    static const BYTE terminated[] = {'A', 'B', 'C', '\0'};
    static const BYTE unterminated[] = {'A', 'B', 'C'};
    static const BYTE extra_nul[] = {'A', 'B', 'C', '\0', '\0'};
    static const BYTE embedded_nul[] = {'A', '\0', 'B', '\0'};
    static const BYTE empty[] = {'\0'};
    static const BYTE expandable[] = {'%', 'T', 'E', 'M', 'P', '%', '\0'};
    static const struct {
        DWORD type;
        const BYTE *value;
        DWORD bytes;
        size_t capacity;
        bool accepted;
        const char *expected;
    } cases[] = {
        {REG_SZ, terminated, (DWORD)sizeof(terminated), 64U, true, "ABC"},
        {REG_SZ, unterminated, (DWORD)sizeof(unterminated), 64U, false, ""},
        {REG_SZ, extra_nul, (DWORD)sizeof(extra_nul), 64U, true, "ABC"},
        {REG_SZ, embedded_nul, (DWORD)sizeof(embedded_nul), 64U, false, ""},
        {REG_SZ, empty, (DWORD)sizeof(empty), 64U, false, ""},
        {REG_EXPAND_SZ, expandable, (DWORD)sizeof(expandable),
         64U, true, "%TEMP%"},
        {REG_BINARY, terminated, (DWORD)sizeof(terminated), 64U, false, ""},
        {REG_SZ, terminated, (DWORD)sizeof(terminated), 3U, false, ""}
    };

    for (size_t index = 0U;
         passed && index < sizeof(cases) / sizeof(cases[0]); index++) {
        if (RegSetValueExA(
                key, "Probe", 0U, cases[index].type,
                cases[index].value, cases[index].bytes) != ERROR_SUCCESS) {
            passed = false;
            break;
        }

        char destination[64];
        memset(destination, 0x7f, sizeof(destination));
        const bool accepted = lsm_windows_registry_read_string(
            key, "Probe", destination, cases[index].capacity);
        if (accepted != cases[index].accepted ||
            strcmp(destination, cases[index].expected) != 0)
            passed = false;
    }

    if (RegCloseKey(key) != ERROR_SUCCESS) passed = false;
    if (disposition == REG_CREATED_NEW_KEY &&
        RegDeleteKeyA(HKEY_CURRENT_USER, subkey) != ERROR_SUCCESS)
        passed = false;
    return passed;
}

int main(void)
{
    printf("System Monitor experimental Windows backend probe\n\n");

    if (!verify_registry_string_boundary()) {
        fprintf(stderr, "Windows registry string boundary failed.\n");
        return EXIT_FAILURE;
    }

    if (!verify_terminated_process_worker_unavailable()) {
        fprintf(stderr,
                "Terminated process worker remained available to the UI.\n");
        return EXIT_FAILURE;
    }
    if (!verify_terminated_process_worker_restart()) {
        fprintf(stderr,
                "Terminated process worker failed to recover on start.\n");
        return EXIT_FAILURE;
    }
    if (!verify_process_worker_restart()) {
        fprintf(stderr,
                "Process worker failed to restart after a clean stop.\n");
        return EXIT_FAILURE;
    }

    LsmMonitor *monitor = (LsmMonitor *)calloc(1U, sizeof(*monitor));
    if (!monitor) {
        fprintf(stderr, "Unable to allocate Windows monitor snapshot.\n");
        return EXIT_FAILURE;
    }
    if (!lsm_monitor_platform_init(monitor)) {
        fprintf(stderr, "Monitor backend initialisation failed (Windows error %lu).\n",
                (unsigned long)GetLastError());
        free(monitor);
        return EXIT_FAILURE;
    }

    Sleep(1000U);
    if (!lsm_monitor_platform_update(monitor)) {
        fprintf(stderr, "Monitor backend update failed (Windows error %lu).\n",
                (unsigned long)GetLastError());
        lsm_monitor_platform_destroy(monitor);
        free(monitor);
        return EXIT_FAILURE;
    }

    printf("CPU\n");
    printf("  Model: %s\n", monitor->cpu.model[0] ? monitor->cpu.model : "Unavailable");
    printf("  Cores: %u physical, %u logical; %u socket(s), %u NUMA node(s)\n",
           monitor->cpu.physical_cores, monitor->cpu.logical_cores,
           monitor->cpu.socket_count, monitor->cpu.numa_node_count);
    printf("  Frequency: current %.2f GHz, base %.2f GHz, max %.2f GHz\n",
           monitor->cpu.frequency_ghz, monitor->cpu.base_frequency_ghz,
           monitor->cpu.max_frequency_ghz);
    printf("  Cache: L1 %s; L2 %s; L3 %s\n",
           monitor->cpu.cache_l1[0] ? monitor->cpu.cache_l1 : "N/A",
           monitor->cpu.cache_l2[0] ? monitor->cpu.cache_l2 : "N/A",
           monitor->cpu.cache_l3[0] ? monitor->cpu.cache_l3 : "N/A");
    if (monitor->cpu.logical_cores == 0U ||
        monitor->cpu.physical_cores == 0U ||
        monitor->cpu.physical_cores > monitor->cpu.logical_cores ||
        monitor->cpu.socket_count == 0U ||
        monitor->cpu.numa_node_count == 0U) {
        fprintf(stderr, "Native Windows CPU topology is incomplete.\n");
        lsm_monitor_platform_destroy(monitor);
        free(monitor);
        return EXIT_FAILURE;
    }
    printf("  Usage: %.1f%% (user %.1f%%, kernel %.1f%%)\n",
           monitor->cpu.usage_percent,
           monitor->cpu.user_percent,
           monitor->cpu.kernel_percent);
    printf("  Uptime: %llu seconds\n",
           (unsigned long long)monitor->cpu.uptime_seconds);
    printf("  Processes: %u  Threads: %u  Handles: %llu\n\n",
           monitor->cpu.process_count,
           monitor->cpu.thread_count,
           (unsigned long long)monitor->cpu.file_handle_count);

    printf("Memory\n");
    printf("  Physical: %.2f GB total, %.2f GB used, %.2f GB available\n",
           bytes_to_gb(monitor->memory.total_bytes),
           bytes_to_gb(monitor->memory.used_bytes),
           bytes_to_gb(monitor->memory.available_bytes));
    printf("  Usage: %.1f%%\n", monitor->memory.usage_percent);
    printf("  Commit: %.2f GB / %.2f GB\n\n",
           bytes_to_gb(monitor->memory.committed_bytes),
           bytes_to_gb(monitor->memory.commit_limit_bytes));

    LsmProcessBackend *process_backend = lsm_process_backend_create();
    if (!process_backend) {
        fprintf(stderr, "Process backend initialisation failed.\n");
        lsm_monitor_platform_destroy(monitor);
        free(monitor);
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
        lsm_monitor_platform_destroy(monitor);
        free(monitor);
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
    lsm_monitor_platform_destroy(monitor);
    free(monitor);
    return EXIT_SUCCESS;
}
