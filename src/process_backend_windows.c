// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_backend_windows.c
 * @brief Initial read-only Windows implementation of the process backend.
 *
 * The first Windows process slice deliberately favours trustworthy native data
 * over breadth. Tool Help supplies process identity and parent/thread counts;
 * process handles supply creation identity, CPU time, working set, I/O totals,
 * priority, executable path and handle count when permissions allow. Retained
 * samples calculate CPU and I/O rates without confusing PID reuse.
 *
 * Process ownership and account identity come from native access-token SIDs.
 * Command-line recovery, GPU/cgroup enrichment and all process-control
 * operations are intentionally unsupported in this first backend. Callers
 * receive partial rows rather than guessed values.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_backend.h"

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <sddl.h>
#include <tlhelp32.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    DWORD pid;
    LsmProcessInstanceId instance_id;
    uint64_t cpu_time_100ns;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t sampled_at_ms;
    unsigned generation;
} LsmWindowsProcessSample;

struct LsmProcessBackend {
    LsmWindowsProcessSample *samples;
    size_t sample_count;
    size_t sample_capacity;
    unsigned generation;
    uint64_t previous_system_cpu_100ns;
    uint64_t total_memory_bytes;
    PSID current_user_sid;
};

static bool reserve_array(void **items, size_t *capacity,
                          size_t element_size, size_t required)
{
    if (!items || !capacity || element_size == 0U) return false;
    if (required <= *capacity) return true;

    size_t new_capacity = *capacity ? *capacity : 256U;
    while (new_capacity < required) {
        if (new_capacity > SIZE_MAX / 2U) {
            new_capacity = required;
            break;
        }
        new_capacity *= 2U;
    }
    if (new_capacity > SIZE_MAX / element_size) return false;

    void *grown = realloc(*items, new_capacity * element_size);
    if (!grown) return false;
    *items = grown;
    *capacity = new_capacity;
    return true;
}

static uint64_t filetime_value(FILETIME value)
{
    return ((uint64_t)value.dwHighDateTime << 32U) |
           (uint64_t)value.dwLowDateTime;
}

static uint64_t process_cpu_time_100ns(HANDLE process,
                                       LsmProcessInstanceId *instance_id,
                                       int64_t *start_time_epoch)
{
    FILETIME creation;
    FILETIME exit_time;
    FILETIME kernel;
    FILETIME user;
    if (!GetProcessTimes(process, &creation, &exit_time, &kernel, &user))
        return 0U;

    const uint64_t creation_value = filetime_value(creation);
    if (instance_id) *instance_id = creation_value;

    if (start_time_epoch) {
        const uint64_t seconds = creation_value / 10000000ULL;
        const uint64_t windows_to_unix = 11644473600ULL;
        *start_time_epoch = seconds >= windows_to_unix
            ? (int64_t)(seconds - windows_to_unix) : 0;
    }

    const uint64_t kernel_value = filetime_value(kernel);
    const uint64_t user_value = filetime_value(user);
    return UINT64_MAX - kernel_value < user_value
        ? UINT64_MAX : kernel_value + user_value;
}

static uint64_t system_cpu_time_100ns(void)
{
    FILETIME idle;
    FILETIME kernel;
    FILETIME user;
    if (!GetSystemTimes(&idle, &kernel, &user))
        return 0U;
    const uint64_t kernel_value = filetime_value(kernel);
    const uint64_t user_value = filetime_value(user);
    return UINT64_MAX - kernel_value < user_value
        ? UINT64_MAX : kernel_value + user_value;
}

static double percent_u64(uint64_t part, uint64_t total)
{
    if (total == 0U) return 0.0;
    const double value = ((double)part * 100.0) / (double)total;
    return value > 100.0 ? 100.0 : value;
}

double lsm_process_cpu_total_percent(uint64_t process_delta,
                                     uint64_t system_delta)
{
    return percent_u64(process_delta, system_delta);
}

static LsmProcessPriority priority_from_class(DWORD priority_class)
{
    switch (priority_class) {
        case REALTIME_PRIORITY_CLASS:
        case HIGH_PRIORITY_CLASS:
            return LSM_PROCESS_PRIORITY_HIGH;
        case ABOVE_NORMAL_PRIORITY_CLASS:
            return LSM_PROCESS_PRIORITY_ABOVE_NORMAL;
        case BELOW_NORMAL_PRIORITY_CLASS:
            return LSM_PROCESS_PRIORITY_BELOW_NORMAL;
        case IDLE_PRIORITY_CLASS:
            return LSM_PROCESS_PRIORITY_LOW;
        case NORMAL_PRIORITY_CLASS:
        default:
            return LSM_PROCESS_PRIORITY_NORMAL;
    }
}

static bool native_pid(LsmProcessId id, DWORD *pid)
{
    if (!pid || id == 0U || id > UINT32_MAX) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }
    *pid = (DWORD)id;
    return true;
}

static void copy_text(char *destination, size_t capacity,
                      const char *source)
{
    if (!destination || capacity == 0U) return;
    destination[0] = '\0';
    if (!source) return;

    const size_t length = strlen(source);
    const size_t copied = length < capacity - 1U ? length : capacity - 1U;
    if (copied > 0U)
        memcpy(destination, source, copied);
    destination[copied] = '\0';
}

static PSID copy_process_user_sid(HANDLE process)
{
    if (!process) return NULL;

    HANDLE token = NULL;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token))
        return NULL;

    DWORD required = 0U;
    (void)GetTokenInformation(token, TokenUser, NULL, 0U, &required);
    if (required == 0U || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        CloseHandle(token);
        return NULL;
    }

    TOKEN_USER *token_user = malloc((size_t)required);
    if (!token_user) {
        CloseHandle(token);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    if (!GetTokenInformation(token, TokenUser, token_user, required, &required) ||
        !IsValidSid(token_user->User.Sid)) {
        free(token_user);
        CloseHandle(token);
        return NULL;
    }

    const DWORD sid_size = GetLengthSid(token_user->User.Sid);
    PSID sid = malloc((size_t)sid_size);
    if (!sid) {
        free(token_user);
        CloseHandle(token);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    if (!CopySid(sid_size, sid, token_user->User.Sid)) {
        free(sid);
        sid = NULL;
    }

    free(token_user);
    CloseHandle(token);
    return sid;
}

static void populate_process_account(LsmProcessBackend *backend,
                                     HANDLE process,
                                     LsmProcessInfo *info)
{
    if (!backend || !process || !info || !backend->current_user_sid)
        return;

    PSID sid = copy_process_user_sid(process);
    if (!sid) return;

    info->owned_by_current_user =
        EqualSid(sid, backend->current_user_sid) != FALSE;

    LPSTR sid_text = NULL;
    if (ConvertSidToStringSidA(sid, &sid_text) && sid_text) {
        static const char prefix[] = "sid:";
        const size_t prefix_length = sizeof(prefix) - 1U;
        const size_t sid_length = strlen(sid_text);
        if (prefix_length + sid_length < sizeof(info->account_identity)) {
            memcpy(info->account_identity, prefix, prefix_length);
            memcpy(info->account_identity + prefix_length, sid_text,
                   sid_length + 1U);
        }
        LocalFree(sid_text);
    }

    char account[256];
    char domain[256];
    DWORD account_length = (DWORD)sizeof(account);
    DWORD domain_length = (DWORD)sizeof(domain);
    SID_NAME_USE use = SidTypeUnknown;
    if (LookupAccountSidA(NULL, sid, account, &account_length,
                          domain, &domain_length, &use))
        copy_text(info->user, sizeof(info->user), account);

    free(sid);
}

static void wide_to_utf8(const WCHAR *source, char *destination,
                         size_t capacity)
{
    if (!destination || capacity == 0U) return;
    destination[0] = '\0';
    if (!source || !source[0] || capacity > (size_t)INT_MAX) return;

    const int result = WideCharToMultiByte(
        CP_UTF8, 0, source, -1, destination, (int)capacity, NULL, NULL);
    if (result <= 0)
        destination[0] = '\0';
}

static LsmWindowsProcessSample *find_or_create_sample(
    LsmProcessBackend *backend, DWORD pid)
{
    if (!backend) return NULL;
    for (size_t index = 0U; index < backend->sample_count; index++) {
        if (backend->samples[index].pid == pid)
            return &backend->samples[index];
    }

    if (!reserve_array((void **)&backend->samples,
                       &backend->sample_capacity,
                       sizeof(*backend->samples),
                       backend->sample_count + 1U))
        return NULL;

    LsmWindowsProcessSample *sample =
        &backend->samples[backend->sample_count++];
    memset(sample, 0, sizeof(*sample));
    sample->pid = pid;
    return sample;
}

static void prune_process_samples(LsmProcessBackend *backend)
{
    if (!backend) return;
    size_t write_index = 0U;
    for (size_t read_index = 0U;
         read_index < backend->sample_count; read_index++) {
        if (backend->samples[read_index].generation != backend->generation)
            continue;
        if (write_index != read_index)
            backend->samples[write_index] = backend->samples[read_index];
        write_index++;
    }
    backend->sample_count = write_index;
}

static HANDLE open_process_for_query(DWORD pid)
{
    HANDLE process = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process)
        process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    return process;
}

static void populate_optional_process_fields(HANDLE process,
                                             LsmProcessInfo *info,
                                             unsigned scan_flags)
{
    if (!process || !info) return;

    if ((scan_flags & LSM_PROCESS_SCAN_EXECUTABLE) != 0U) {
        DWORD length = (DWORD)sizeof(info->executable);
        if (!QueryFullProcessImageNameA(
                process, 0U, info->executable, &length))
            info->executable[0] = '\0';
    }

    if ((scan_flags & LSM_PROCESS_SCAN_HANDLE_COUNT) != 0U) {
        DWORD handle_count = 0U;
        if (GetProcessHandleCount(process, &handle_count))
            info->handle_count = (unsigned)handle_count;
    }
}

static void populate_process_metrics(LsmProcessBackend *backend,
                                     HANDLE process,
                                     LsmProcessInfo *info,
                                     uint64_t system_delta,
                                     uint64_t now_ms,
                                     unsigned scan_flags)
{
    if (!backend || !process || !info) return;

    int64_t start_epoch = 0;
    const uint64_t cpu_time = process_cpu_time_100ns(
        process, &info->instance_id, &start_epoch);
    info->start_time_epoch = start_epoch;
    info->cpu_time_seconds = cpu_time / 10000000ULL;
    info->cpu_time_nanoseconds =
        cpu_time > UINT64_MAX / 100ULL ? UINT64_MAX : cpu_time * 100ULL;

    FILETIME now_filetime;
    GetSystemTimeAsFileTime(&now_filetime);
    const uint64_t now_seconds =
        filetime_value(now_filetime) / 10000000ULL;
    const uint64_t windows_to_unix = 11644473600ULL;
    if (now_seconds >= windows_to_unix && start_epoch > 0) {
        const uint64_t now_epoch = now_seconds - windows_to_unix;
        info->elapsed_seconds = now_epoch >= (uint64_t)start_epoch
            ? now_epoch - (uint64_t)start_epoch : 0U;
    }

    PROCESS_MEMORY_COUNTERS_EX memory;
    memset(&memory, 0, sizeof(memory));
    if (GetProcessMemoryInfo(
            process, (PROCESS_MEMORY_COUNTERS *)&memory,
            (DWORD)sizeof(memory))) {
        info->rss_bytes = (uint64_t)memory.WorkingSetSize;
        info->memory_percent = percent_u64(
            info->rss_bytes, backend->total_memory_bytes);
        info->page_faults = (uint64_t)memory.PageFaultCount;
    }

    IO_COUNTERS io;
    memset(&io, 0, sizeof(io));
    if (GetProcessIoCounters(process, &io)) {
        info->read_bytes = (uint64_t)io.ReadTransferCount;
        info->write_bytes = (uint64_t)io.WriteTransferCount;
    }

    const DWORD priority_class = GetPriorityClass(process);
    if (priority_class != 0U)
        info->priority = priority_from_class(priority_class);

    populate_process_account(backend, process, info);
    populate_optional_process_fields(process, info, scan_flags);

    LsmWindowsProcessSample *sample =
        find_or_create_sample(backend, (DWORD)info->pid);
    if (!sample) return;

    if (sample->instance_id != 0U &&
        sample->instance_id == info->instance_id) {
        if (cpu_time >= sample->cpu_time_100ns)
            info->cpu_percent = lsm_process_cpu_total_percent(
                cpu_time - sample->cpu_time_100ns, system_delta);

        if (now_ms > sample->sampled_at_ms) {
            const double elapsed =
                (double)(now_ms - sample->sampled_at_ms) / 1000.0;
            if (elapsed > 0.0) {
                if (info->read_bytes >= sample->read_bytes)
                    info->read_bytes_per_sec =
                        (double)(info->read_bytes - sample->read_bytes) /
                        elapsed;
                if (info->write_bytes >= sample->write_bytes)
                    info->write_bytes_per_sec =
                        (double)(info->write_bytes - sample->write_bytes) /
                        elapsed;
            }
        }
    }

    sample->instance_id = info->instance_id;
    sample->cpu_time_100ns = cpu_time;
    sample->read_bytes = info->read_bytes;
    sample->write_bytes = info->write_bytes;
    sample->sampled_at_ms = now_ms;
    sample->generation = backend->generation;
}

LsmProcessBackend *lsm_process_backend_create(void)
{
    LsmProcessBackend *backend = calloc(1U, sizeof(*backend));
    if (!backend) return NULL;

    backend->current_user_sid = copy_process_user_sid(GetCurrentProcess());
    if (!backend->current_user_sid) {
        free(backend);
        return NULL;
    }

    MEMORYSTATUSEX memory;
    memset(&memory, 0, sizeof(memory));
    memory.dwLength = (DWORD)sizeof(memory);
    if (GlobalMemoryStatusEx(&memory))
        backend->total_memory_bytes = (uint64_t)memory.ullTotalPhys;

    backend->previous_system_cpu_100ns = system_cpu_time_100ns();
    return backend;
}

void lsm_process_backend_destroy(LsmProcessBackend *backend)
{
    if (!backend) return;
    free(backend->samples);
    free(backend->current_user_sid);
    free(backend);
}

size_t lsm_process_scan(LsmProcessBackend *backend,
                        LsmProcessInfo **out_processes,
                        unsigned scan_flags)
{
    if (!backend || !out_processes) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0U;
    }
    *out_processes = NULL;

    const uint64_t system_cpu = system_cpu_time_100ns();
    const uint64_t system_delta =
        system_cpu >= backend->previous_system_cpu_100ns
            ? system_cpu - backend->previous_system_cpu_100ns : 0U;
    const uint64_t now_ms = (uint64_t)GetTickCount64();

    backend->generation++;
    if (backend->generation == 0U) {
        for (size_t index = 0U; index < backend->sample_count; index++)
            backend->samples[index].generation = 0U;
        backend->generation = 1U;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0U);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0U;

    LsmProcessInfo *processes = NULL;
    size_t count = 0U;
    size_t capacity = 0U;

    PROCESSENTRY32W entry;
    memset(&entry, 0, sizeof(entry));
    entry.dwSize = (DWORD)sizeof(entry);
    BOOL have_entry = Process32FirstW(snapshot, &entry);
    while (have_entry) {
        if (!reserve_array((void **)&processes, &capacity,
                           sizeof(*processes), count + 1U)) {
            free(processes);
            CloseHandle(snapshot);
            SetLastError(ERROR_NOT_ENOUGH_MEMORY);
            return 0U;
        }

        LsmProcessInfo *info = &processes[count];
        memset(info, 0, sizeof(*info));
        info->pid = (LsmProcessId)entry.th32ProcessID;
        info->ppid = (LsmProcessId)entry.th32ParentProcessID;
        info->threads = (unsigned)entry.cntThreads;
        info->priority = LSM_PROCESS_PRIORITY_NORMAL;
        wide_to_utf8(entry.szExeFile, info->name, sizeof(info->name));
        (void)snprintf(info->state, sizeof(info->state), "%s", "Unknown");

        HANDLE process = open_process_for_query(entry.th32ProcessID);
        if (process) {
            populate_process_metrics(
                backend, process, info, system_delta, now_ms, scan_flags);
            CloseHandle(process);
        }

        count++;
        have_entry = Process32NextW(snapshot, &entry);
    }
    CloseHandle(snapshot);

    backend->previous_system_cpu_100ns = system_cpu;
    prune_process_samples(backend);

    if (count == 0U) {
        free(processes);
        return 0U;
    }
    *out_processes = processes;
    return count;
}

bool lsm_process_identity_matches(LsmProcessId pid,
                                  LsmProcessInstanceId instance_id)
{
    DWORD native = 0U;
    if (!native_pid(pid, &native) || instance_id == 0U)
        return false;

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, native);
    if (!process) return false;

    LsmProcessInstanceId current_instance = 0U;
    const uint64_t cpu_time = process_cpu_time_100ns(
        process, &current_instance, NULL);
    (void)cpu_time;
    CloseHandle(process);
    return current_instance != 0U && current_instance == instance_id;
}

bool lsm_process_enrich(LsmProcessId pid, LsmProcessInfo *process,
                        unsigned scan_flags)
{
    DWORD native = 0U;
    if (!process || !native_pid(pid, &native) ||
        process->instance_id == 0U ||
        process->pid != pid ||
        !lsm_process_identity_matches(pid, process->instance_id))
        return false;

    HANDLE handle = open_process_for_query(native);
    if (!handle) return false;
    populate_optional_process_fields(handle, process, scan_flags);
    CloseHandle(handle);
    return lsm_process_identity_matches(pid, process->instance_id);
}

static bool unsupported_process_operation(void)
{
    SetLastError(ERROR_NOT_SUPPORTED);
    return false;
}

bool lsm_process_set_priority(LsmProcessId pid,
                              LsmProcessInstanceId instance_id,
                              LsmProcessPriority priority)
{
    (void)pid;
    (void)instance_id;
    (void)priority;
    return unsupported_process_operation();
}

bool lsm_process_set_efficiency(LsmProcessId pid,
                                LsmProcessInstanceId instance_id,
                                bool enabled)
{
    (void)pid;
    (void)instance_id;
    (void)enabled;
    return unsupported_process_operation();
}

size_t lsm_process_affinity_get(LsmProcessId pid,
                                LsmProcessInstanceId instance_id,
                                bool *enabled, size_t capacity)
{
    (void)pid;
    (void)instance_id;
    (void)enabled;
    (void)capacity;
    SetLastError(ERROR_NOT_SUPPORTED);
    return 0U;
}

bool lsm_process_affinity_set(LsmProcessId pid,
                              LsmProcessInstanceId instance_id,
                              const bool *enabled, size_t count)
{
    (void)pid;
    (void)instance_id;
    (void)enabled;
    (void)count;
    return unsupported_process_operation();
}

bool lsm_process_control_tree(LsmProcessId root_pid,
                              LsmProcessInstanceId root_instance_id,
                              LsmProcessControl action)
{
    (void)root_pid;
    (void)root_instance_id;
    (void)action;
    return unsupported_process_operation();
}

bool lsm_process_control(LsmProcessId pid,
                         LsmProcessInstanceId instance_id,
                         LsmProcessControl action)
{
    (void)pid;
    (void)instance_id;
    (void)action;
    return unsupported_process_operation();
}

void lsm_process_error_message(char *buffer, size_t size)
{
    if (!buffer || size == 0U) return;
    buffer[0] = '\0';

    const DWORD error = GetLastError();
    if (error == ERROR_SUCCESS) {
        (void)snprintf(buffer, size, "%s", "No Windows process error");
        return;
    }

    const DWORD length = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, 0U, buffer,
        size > (size_t)UINT32_MAX ? UINT32_MAX : (DWORD)size,
        NULL);
    if (length == 0U) {
        (void)snprintf(
            buffer, size, "Windows error %lu", (unsigned long)error);
        return;
    }

    size_t used = strlen(buffer);
    while (used > 0U &&
           (buffer[used - 1U] == '\r' || buffer[used - 1U] == '\n' ||
            buffer[used - 1U] == ' ')) {
        buffer[--used] = '\0';
    }
}

void lsm_process_list_free(LsmProcessInfo *processes)
{
    free(processes);
}
