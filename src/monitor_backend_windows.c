// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_backend_windows.c
 * @brief First native Windows implementation of the monitor backend contract.
 *
 * This deliberately small backend establishes the Windows side of the existing
 * platform seam without pretending feature parity with Linux. It currently
 * supplies aggregate CPU utilisation, logical processor count, uptime,
 * process/thread/handle totals, and physical/commit memory through documented
 * Win32 and PSAPI interfaces. Device topology, pressure, disks, networking,
 * accelerators, batteries and other optional telemetry remain unavailable
 * until dedicated Windows collectors are added.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_platform.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint64_t idle_time;
    uint64_t kernel_time;
    uint64_t user_time;
    bool cpu_baseline_valid;
} LsmWindowsMonitorBackendState;

static uint64_t filetime_value(FILETIME value)
{
    return ((uint64_t)value.dwHighDateTime << 32U) |
           (uint64_t)value.dwLowDateTime;
}

static uint64_t pages_to_bytes(SIZE_T pages, SIZE_T page_size)
{
    if (page_size != 0U &&
        (uint64_t)pages > UINT64_MAX / (uint64_t)page_size)
        return UINT64_MAX;
    return (uint64_t)pages * (uint64_t)page_size;
}

static unsigned size_to_unsigned(SIZE_T value)
{
    return (uint64_t)value > (uint64_t)UINT_MAX
        ? UINT_MAX : (unsigned)value;
}

static double percent_u64(uint64_t part, uint64_t total)
{
    if (total == 0U) return 0.0;
    const double value = ((double)part * 100.0) / (double)total;
    return value > 100.0 ? 100.0 : value;
}

static bool read_cpu_times(uint64_t *idle, uint64_t *kernel, uint64_t *user)
{
    if (!idle || !kernel || !user) return false;

    FILETIME idle_time;
    FILETIME kernel_time;
    FILETIME user_time;
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time))
        return false;

    *idle = filetime_value(idle_time);
    *kernel = filetime_value(kernel_time);
    *user = filetime_value(user_time);
    return true;
}

static void populate_cpu_identity(LsmMonitor *monitor)
{
    if (!monitor) return;

    SYSTEM_INFO system_info;
    GetNativeSystemInfo(&system_info);
    monitor->cpu.logical_cores = (unsigned)system_info.dwNumberOfProcessors;

    monitor->cpu.model[0] = '\0';
    const DWORD length = GetEnvironmentVariableA(
        "PROCESSOR_IDENTIFIER", monitor->cpu.model,
        (DWORD)sizeof(monitor->cpu.model));
    if (length == 0U || length >= (DWORD)sizeof(monitor->cpu.model))
        monitor->cpu.model[0] = '\0';
}

static bool update_cpu_snapshot(LsmMonitor *monitor,
                                LsmWindowsMonitorBackendState *state)
{
    if (!monitor || !state) return false;

    uint64_t idle = 0U;
    uint64_t kernel = 0U;
    uint64_t user = 0U;
    if (!read_cpu_times(&idle, &kernel, &user))
        return false;

    if (state->cpu_baseline_valid &&
        idle >= state->idle_time &&
        kernel >= state->kernel_time &&
        user >= state->user_time) {
        const uint64_t idle_delta = idle - state->idle_time;
        const uint64_t kernel_delta = kernel - state->kernel_time;
        const uint64_t user_delta = user - state->user_time;
        const uint64_t total_delta = kernel_delta + user_delta;
        const uint64_t busy_kernel_delta =
            kernel_delta >= idle_delta ? kernel_delta - idle_delta : 0U;
        const uint64_t busy_delta = user_delta + busy_kernel_delta;

        monitor->cpu.usage_percent = percent_u64(busy_delta, total_delta);
        monitor->cpu.user_percent = percent_u64(user_delta, total_delta);
        monitor->cpu.kernel_percent =
            percent_u64(busy_kernel_delta, total_delta);
    } else {
        monitor->cpu.usage_percent = 0.0;
        monitor->cpu.user_percent = 0.0;
        monitor->cpu.kernel_percent = 0.0;
    }

    state->idle_time = idle;
    state->kernel_time = kernel;
    state->user_time = user;
    state->cpu_baseline_valid = true;
    monitor->cpu.uptime_seconds = (uint64_t)(GetTickCount64() / 1000ULL);
    return true;
}

static bool update_memory_snapshot(LsmMonitor *monitor)
{
    if (!monitor) return false;

    PERFORMANCE_INFORMATION performance;
    memset(&performance, 0, sizeof(performance));
    performance.cb = (DWORD)sizeof(performance);
    if (GetPerformanceInfo(&performance, (DWORD)sizeof(performance))) {
        monitor->memory.total_bytes = pages_to_bytes(
            performance.PhysicalTotal, performance.PageSize);
        monitor->memory.available_bytes = pages_to_bytes(
            performance.PhysicalAvailable, performance.PageSize);
        monitor->memory.cached_bytes = pages_to_bytes(
            performance.SystemCache, performance.PageSize);
        monitor->memory.committed_bytes = pages_to_bytes(
            performance.CommitTotal, performance.PageSize);
        monitor->memory.commit_limit_bytes = pages_to_bytes(
            performance.CommitLimit, performance.PageSize);
        monitor->cpu.process_count =
            size_to_unsigned((SIZE_T)performance.ProcessCount);
        monitor->cpu.thread_count =
            size_to_unsigned((SIZE_T)performance.ThreadCount);
        monitor->cpu.file_handle_count = (uint64_t)performance.HandleCount;
    } else {
        MEMORYSTATUSEX status;
        memset(&status, 0, sizeof(status));
        status.dwLength = (DWORD)sizeof(status);
        if (!GlobalMemoryStatusEx(&status))
            return false;
        monitor->memory.total_bytes = (uint64_t)status.ullTotalPhys;
        monitor->memory.available_bytes = (uint64_t)status.ullAvailPhys;
        monitor->memory.cached_bytes = 0U;
        monitor->memory.committed_bytes = 0U;
        monitor->memory.commit_limit_bytes = 0U;
    }

    monitor->memory.used_bytes =
        monitor->memory.total_bytes >= monitor->memory.available_bytes
            ? monitor->memory.total_bytes - monitor->memory.available_bytes
            : 0U;
    monitor->memory.usage_percent = percent_u64(
        monitor->memory.used_bytes, monitor->memory.total_bytes);
    return true;
}

bool lsm_monitor_platform_init(LsmMonitor *monitor)
{
    if (!monitor) return false;
    memset(monitor, 0, sizeof(*monitor));

    LsmWindowsMonitorBackendState *state =
        calloc(1U, sizeof(*state));
    if (!state) return false;
    monitor->backend_state = state;

    populate_cpu_identity(monitor);
    if (!update_cpu_snapshot(monitor, state) ||
        !update_memory_snapshot(monitor)) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }
    return true;
}

bool lsm_monitor_platform_update(LsmMonitor *monitor)
{
    if (!monitor || !monitor->backend_state) return false;
    LsmWindowsMonitorBackendState *state =
        (LsmWindowsMonitorBackendState *)monitor->backend_state;

    const bool cpu_ok = update_cpu_snapshot(monitor, state);
    const bool memory_ok = update_memory_snapshot(monitor);
    return cpu_ok && memory_ok;
}

void lsm_monitor_platform_request_topology_refresh(LsmMonitor *monitor)
{
    (void)monitor;
    /* This first backend slice exposes no device topology yet. */
}

void lsm_monitor_platform_destroy(LsmMonitor *monitor)
{
    if (!monitor) return;
    free(monitor->backend_state);
    monitor->backend_state = NULL;
}
