// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_backend_linux.c
 * @brief Linux implementation of the platform-neutral process backend.
 *
 * Process identity and scheduler counters come from the documented procfs
 * process interfaces. Expensive details such as executable paths and open
 * descriptor counts are collected only when the corresponding column or
 * details dialog requests them.
 *
 * The retained-state design is central to correctness and performance. Each
 * process is identified by (PID, start_ticks), prior samples are kept in a
 * PID-sorted vector, and current rows locate baselines by binary search. This
 * prevents PID reuse from contaminating deltas while avoiding a quadratic scan.
 * Immutable machine constants, NSS work buffers and UID results are cached for
 * the backend lifetime. The public context is serial and GTK-independent.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_backend.h"
#include "common.h"
#include "compiler.h"
#include "process_gpu.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>


#ifndef IOPRIO_CLASS_SHIFT
#define IOPRIO_CLASS_SHIFT 13
#endif
#ifndef IOPRIO_CLASS_BE
#define IOPRIO_CLASS_BE 2
#endif
#ifndef IOPRIO_CLASS_IDLE
#define IOPRIO_CLASS_IDLE 3
#endif
#ifndef IOPRIO_WHO_PROCESS
#define IOPRIO_WHO_PROCESS 1
#endif
#define LSM_IOPRIO_VALUE(class_id, data) (((class_id) << IOPRIO_CLASS_SHIFT) | (data))

typedef struct {
    uint64_t cpu_ticks;
    uint64_t start_ticks;
    int nice_value;
} LinuxProcessStat;

typedef struct {
    uid_t uid;
    bool uid_available;
} LinuxProcessStatus;

typedef struct {
    pid_t pid;
    uint64_t start_ticks;
    uint64_t cpu_ticks;
    uint64_t read_bytes;
    uint64_t write_bytes;
    double sampled_at;
    double gpu_sampled_at;
    LsmProcessGpuSnapshot gpu;
    unsigned generation;
} PreviousProcessSample;

typedef struct {
    pid_t pid;
    pid_t ppid;
    LsmProcessInstanceId instance_id;
    unsigned depth;
} PidParent;

typedef struct {
    uid_t uid;
    char name[64];
} UidNameEntry;

/* One context is owned by each application instance.  GTK invokes scans
 * serially from the main loop, so no locking is required.  start_ticks
 * protects every delta from PID reuse. */
struct LsmProcessBackend {
    PreviousProcessSample *samples;
    size_t sample_count;
    size_t sample_capacity;
    unsigned generation;
    uint64_t previous_total_cpu_ticks;

    /* Immutable machine constants are captured once rather than queried for
     * every process scan. They cannot change during a booted process lifetime. */
    uint64_t total_memory_bytes;
    long ticks_per_second;
    int64_t boot_time_epoch;

    /* A desktop normally has hundreds of processes but only a handful of UIDs.
     * Caching NSS results avoids a getpwuid_r() lookup for every row on every
     * refresh while preserving numeric fallback for unknown identities. */
    UidNameEntry uid_names[64];
    size_t uid_name_count;
    char *passwd_buffer;
    size_t passwd_buffer_size;
    uid_t current_uid;
};

static bool native_pid(LsmProcessId id, pid_t *pid)
{
    if (!pid || id == 0U || id > (LsmProcessId)INT_MAX) {
        errno = EINVAL;
        return false;
    }
    *pid = (pid_t)id;
    return true;
}

static LsmProcessPriority priority_from_nice(int nice_value)
{
    if (nice_value <= -10) return LSM_PROCESS_PRIORITY_HIGH;
    if (nice_value < 0) return LSM_PROCESS_PRIORITY_ABOVE_NORMAL;
    if (nice_value == 0) return LSM_PROCESS_PRIORITY_NORMAL;
    if (nice_value <= 10) return LSM_PROCESS_PRIORITY_BELOW_NORMAL;
    return LSM_PROCESS_PRIORITY_LOW;
}

static int nice_from_priority(LsmProcessPriority priority)
{
    switch (priority) {
        case LSM_PROCESS_PRIORITY_HIGH: return -10;
        case LSM_PROCESS_PRIORITY_ABOVE_NORMAL: return -5;
        case LSM_PROCESS_PRIORITY_NORMAL: return 0;
        case LSM_PROCESS_PRIORITY_BELOW_NORMAL: return 5;
        case LSM_PROCESS_PRIORITY_LOW: return 10;
    }
    return 0;
}

static int signal_from_control(LsmProcessControl action)
{
    switch (action) {
        case LSM_PROCESS_CONTROL_TERMINATE: return SIGTERM;
        case LSM_PROCESS_CONTROL_SUSPEND: return SIGSTOP;
        case LSM_PROCESS_CONTROL_RESUME: return SIGCONT;
        case LSM_PROCESS_CONTROL_FORCE_TERMINATE: return SIGKILL;
    }
    return 0;
}

static bool numeric_name(const char *name)
{
    if (!name || !*name) return false;
    for (const char *p = name; *p; p++)
        if (!isdigit((unsigned char)*p)) return false;
    return true;
}

static bool read_whole_file(const char *path, char *buffer, size_t size)
{
    if (!path || !buffer || size < 2U) return false;
    buffer[0] = '\0';
    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return false;

    ssize_t length;
    do {
        length = read(descriptor, buffer, size - 1U);
    } while (length < 0 && errno == EINTR);
    (void)close(descriptor);
    if (length <= 0) return false;
    buffer[(size_t)length] = '\0';
    return true;
}

static bool parse_prefixed_u64(const char *line, const char *prefix,
                               uint64_t *value)
{
    const size_t prefix_length = strlen(prefix);
    if (strncmp(line, prefix, prefix_length) != 0) return false;
    const char *cursor = line + prefix_length;
    while (*cursor == ' ' || *cursor == '\t') cursor++;
    if (!isdigit((unsigned char)*cursor)) return false;

    uint64_t parsed = 0U;
    do {
        const unsigned digit = (unsigned)(*cursor - '0');
        if (parsed > (UINT64_MAX - digit) / 10U) return false;
        parsed = parsed * 10U + digit;
        cursor++;
    } while (isdigit((unsigned char)*cursor));
    *value = parsed;
    return true;
}

static int compare_sample_pid(const void *left, const void *right)
{
    const PreviousProcessSample *a = left;
    const PreviousProcessSample *b = right;
    return a->pid > b->pid ? 1 : a->pid < b->pid ? -1 : 0;
}

/* The retained sample vector is sorted by PID at the end of every scan. During
 * the next scan, binary search therefore reduces lookup from O(P^2) to
 * O(P log P). Newly observed PIDs are appended after the searchable prefix;
 * each PID appears at most once in a /proc enumeration, so they need not enter
 * the search set until the vector is sorted again. */
static PreviousProcessSample *find_or_create_sample(LsmProcessBackend *backend,
                                                     size_t searchable_count,
                                                     pid_t pid)
{
    size_t low = 0U;
    size_t high = searchable_count;
    while (low < high) {
        const size_t middle = low + (high - low) / 2U;
        const pid_t candidate = backend->samples[middle].pid;
        if (candidate < pid)
            low = middle + 1U;
        else
            high = middle;
    }
    if (low < searchable_count && backend->samples[low].pid == pid)
        return &backend->samples[low];

    if (backend->sample_count == backend->sample_capacity) {
        const size_t new_capacity = backend->sample_capacity
            ? backend->sample_capacity * 2U : 1024U;
        if (new_capacity < backend->sample_capacity ||
            new_capacity > SIZE_MAX / sizeof(*backend->samples))
            return NULL;
        PreviousProcessSample *grown = realloc(
            backend->samples, new_capacity * sizeof(*grown));
        if (!grown) return NULL;
        backend->samples = grown;
        backend->sample_capacity = new_capacity;
    }

    PreviousProcessSample *sample = &backend->samples[backend->sample_count++];
    memset(sample, 0, sizeof(*sample));
    sample->pid = pid;
    return sample;
}


static const char *state_name(char state)
{
    switch (state) {
        case 'R': return "Running";
        case 'S': return "Sleeping";
        case 'D': return "Disk sleep";
        case 'Z': return "Zombie";
        case 'T': case 't': return "Stopped";
        case 'I': return "Idle";
        case 'X': case 'x': return "Dead";
        case 'W': return "Paging";
        default: return "Unknown";
    }
}

/* /proc/<pid>/stat field 2 may contain spaces and parentheses.  The code
 * locates that field first, then tokenises fields 3 onward by their documented
 * numeric positions. */
static bool read_process_stat(pid_t pid, LsmProcessInfo *process,
                              LinuxProcessStat *native_stat)
{
    if (!process || !native_stat) return false;
    *native_stat = (LinuxProcessStat){0};
    char path[128], text[8192];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    if (!read_whole_file(path, text, sizeof(text))) return false;

    char *left = strchr(text, '(');
    char *right = strrchr(text, ')');
    if (!left || !right || right <= left) return false;
    *right = '\0';
    snprintf(process->name, sizeof(process->name), "%s", left + 1);

    char *save = NULL;
    char *token = strtok_r(right + 2, " ", &save);
    unsigned field = 3;
    char state = '?';
    bool have_ppid = false, have_start = false;
    uint64_t utime = 0, stime = 0, minflt = 0, majflt = 0;
    int nice_value = 0;

    while (token) {
        switch (field) {
            case 3: state = token[0]; break;
            case 4: process->ppid = (LsmProcessId)strtoull(token, NULL, 10); have_ppid = true; break;
            case 10: minflt = strtoull(token, NULL, 10); break;
            case 12: majflt = strtoull(token, NULL, 10); break;
            case 14: utime = strtoull(token, NULL, 10); break;
            case 15: stime = strtoull(token, NULL, 10); break;
            case 19: nice_value = (int)strtol(token, NULL, 10); break;
            case 20: {
                long threads = strtol(token, NULL, 10);
                process->threads = threads <= 0 ? 0U :
                    (unsigned long)threads > UINT_MAX
                        ? UINT_MAX : (unsigned)threads;
                break;
            }
            case 22:
                native_stat->start_ticks = strtoull(token, NULL, 10);
                have_start = true;
                break;
            default: break;
        }
        if (field >= 22) break;
        token = strtok_r(NULL, " ", &save);
        field++;
    }

    if (!have_ppid || !have_start) return false;
    native_stat->cpu_ticks = lsm_u64_add_saturating(utime, stime);
    native_stat->nice_value = nice_value;
    process->page_faults = lsm_u64_add_saturating(minflt, majflt);
    snprintf(process->state, sizeof(process->state), "%s", state_name(state));
    return true;
}

static bool process_identity_pid(LsmProcessId process_id,
                                 LsmProcessInstanceId instance_id,
                                 pid_t *pid)
{
    if (!native_pid(process_id, pid) || instance_id == 0U) {
        errno = EINVAL;
        return false;
    }
    LsmProcessInfo process = { .pid = process_id };
    LinuxProcessStat native_stat = {0};
    errno = 0;
    if (!read_process_stat(*pid, &process, &native_stat)) {
        if (errno == 0 || errno == ENOENT) errno = ESRCH;
        return false;
    }
    if (native_stat.start_ticks != instance_id) {
        errno = ESRCH;
        return false;
    }
    return true;
}

bool lsm_process_identity_matches(LsmProcessId process_id,
                                  LsmProcessInstanceId instance_id)
{
    pid_t pid = 0;
    return process_identity_pid(process_id, instance_id, &pid);
}

static void resolve_process_user(LsmProcessBackend *backend, uid_t uid,
                                 LsmProcessInfo *process)
{
    (void)snprintf(process->account_identity, sizeof(process->account_identity),
                   "uid:%llu", (unsigned long long)uid);
    for (size_t index = 0U; index < backend->uid_name_count; index++) {
        if (backend->uid_names[index].uid == uid) {
            lsm_copy_string(process->user, sizeof(process->user),
                            backend->uid_names[index].name);
            return;
        }
    }

    char resolved_name[sizeof(process->user)];
    resolved_name[0] = '\0';
    if (backend->passwd_buffer && backend->passwd_buffer_size > 0U) {
        struct passwd record;
        struct passwd *result = NULL;
        const int status = getpwuid_r(uid, &record,
                                      backend->passwd_buffer,
                                      backend->passwd_buffer_size, &result);
        if (status == 0 && result && result->pw_name)
            lsm_copy_string(resolved_name, sizeof(resolved_name), result->pw_name);
    }
    if (!resolved_name[0])
        (void)snprintf(resolved_name, sizeof(resolved_name), "%llu",
                       (unsigned long long)uid);

    if (backend->uid_name_count < LSM_ARRAY_LENGTH(backend->uid_names)) {
        UidNameEntry *entry = &backend->uid_names[backend->uid_name_count++];
        entry->uid = uid;
        lsm_copy_string(entry->name, sizeof(entry->name), resolved_name);
    }
    lsm_copy_string(process->user, sizeof(process->user), resolved_name);
}

static void read_process_status(LsmProcessBackend *backend, pid_t pid,
                                LsmProcessInfo *process,
                                LinuxProcessStatus *native_status)
{
    if (!native_status) return;
    *native_status = (LinuxProcessStatus){0};
    char path[128];
    char text[4096];
    (void)snprintf(path, sizeof(path), "/proc/%d/status", pid);
    if (!read_whole_file(path, text, sizeof(text))) return;

    uint64_t voluntary = 0U;
    uint64_t involuntary = 0U;
    char *line = text;
    while (*line) {
        char *next = strchr(line, '\n');
        if (next) *next = '\0';

        uint64_t value = 0U;
        if (parse_prefixed_u64(line, "Uid:", &value)) {
            native_status->uid = (uid_t)value;
            native_status->uid_available = true;
        }
        else if (parse_prefixed_u64(line, "VmRSS:", &value))
            process->rss_bytes =
                lsm_u64_multiply_saturating(value, 1024U);
        else if (parse_prefixed_u64(line, "Threads:", &value))
            process->threads = value <= UINT_MAX ? (unsigned)value : UINT_MAX;
        else if (parse_prefixed_u64(line, "voluntary_ctxt_switches:", &value))
            voluntary = value;
        else if (parse_prefixed_u64(line, "nonvoluntary_ctxt_switches:", &value))
            involuntary = value;

        if (!next) break;
        line = next + 1;
    }
    process->context_switches =
        lsm_u64_add_saturating(voluntary, involuntary);
    if (native_status->uid_available)
        resolve_process_user(backend, native_status->uid, process);
}

static void read_process_command(pid_t pid, LsmProcessInfo *process)
{
    char path[128];
    (void)snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    const int descriptor = open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        (void)snprintf(process->command, sizeof(process->command),
                       "[%s]", process->name);
        return;
    }

    ssize_t result;
    do {
        result = read(descriptor, process->command, sizeof(process->command) - 1U);
    } while (result < 0 && errno == EINTR);
    (void)close(descriptor);
    if (result <= 0) {
        (void)snprintf(process->command, sizeof(process->command),
                       "[%s]", process->name);
        return;
    }

    size_t length = (size_t)result;
    process->command[length] = '\0';
    for (size_t index = 0U; index < length; index++) {
        const unsigned char value = (unsigned char)process->command[index];
        if (value == '\0' || value == '\n' || value == '\r' || value == '\t' ||
            value < 0x20U || value == 0x7fU)
            process->command[index] = ' ';
    }
    while (length > 0U && process->command[length - 1U] == ' ')
        process->command[--length] = '\0';
}

static void read_process_io(pid_t pid, LsmProcessInfo *process)
{
    char path[128];
    char text[1024];
    (void)snprintf(path, sizeof(path), "/proc/%d/io", pid);
    if (!read_whole_file(path, text, sizeof(text))) return;

    char *line = text;
    while (*line) {
        char *next = strchr(line, '\n');
        if (next) *next = '\0';
        uint64_t value = 0U;
        if (parse_prefixed_u64(line, "read_bytes:", &value))
            process->read_bytes = value;
        else if (parse_prefixed_u64(line, "write_bytes:", &value))
            process->write_bytes = value;
        if (!next) break;
        line = next + 1;
    }
}

static void read_process_executable(pid_t pid, LsmProcessInfo *process)
{
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/exe", pid);
    ssize_t length = readlink(path, process->executable, sizeof(process->executable) - 1);
    if (length < 0) {
        process->executable[0] = '\0';
        return;
    }
    process->executable[length] = '\0';
}

static unsigned count_process_fds(pid_t pid)
{
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/fd", pid);
    DIR *directory = opendir(path);
    if (!directory) return 0;

    unsigned count = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)))
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
            count++;
    closedir(directory);
    return count;
}

bool lsm_process_enrich(LsmProcessId process_id, LsmProcessInfo *process, unsigned scan_flags)
{
    pid_t pid = 0;
    if (!process || process->pid != process_id ||
        !process_identity_pid(process_id, process->instance_id, &pid))
        return false;
    if (scan_flags & LSM_PROCESS_SCAN_EXECUTABLE)
        read_process_executable(pid, process);
    if (scan_flags & LSM_PROCESS_SCAN_HANDLE_COUNT)
        process->handle_count = count_process_fds(pid);
    if (process_identity_pid(process_id, process->instance_id, &pid))
        return true;
    if (scan_flags & LSM_PROCESS_SCAN_EXECUTABLE)
        process->executable[0] = '\0';
    if (scan_flags & LSM_PROCESS_SCAN_HANDLE_COUNT)
        process->handle_count = 0U;
    return false;
}

static uint64_t read_total_cpu_ticks(void)
{
    char text[512];
    if (!lsm_read_text_file("/proc/stat", text, sizeof(text))) return 0U;
    unsigned long long user = 0U;
    unsigned long long nice = 0U;
    unsigned long long system = 0U;
    unsigned long long idle = 0U;
    unsigned long long iowait = 0U;
    unsigned long long irq = 0U;
    unsigned long long softirq = 0U;
    unsigned long long steal = 0U;
    const int matched = sscanf(
        text, "%*s %llu %llu %llu %llu %llu %llu %llu %llu",
        &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);
    if (matched < 4) return 0U;
    const uint64_t fields[] = {
        user, nice, system, idle, iowait, irq, softirq, steal
    };
    uint64_t total = 0U;
    for (size_t index = 0U; index < LSM_ARRAY_LENGTH(fields); index++)
        total = lsm_u64_add_saturating(total, fields[index]);
    return total;
}

static int64_t read_boot_time(void)
{
    FILE *file = fopen("/proc/stat", "r");
    if (!file) return 0;
    char line[512];
    int64_t boot = 0;
    while (fgets(line, sizeof(line), file)) {
        long long value = 0;
        if (sscanf(line, "btime %lld", &value) == 1) {
            boot = value;
            break;
        }
    }
    fclose(file);
    return boot;
}

static double read_uptime_seconds(void)
{
    double uptime = 0.0;
    return lsm_read_double_file("/proc/uptime", &uptime) ? uptime : 0.0;
}

static int compare_process_cpu(const void *left, const void *right)
{
    const LsmProcessInfo *a = left, *b = right;
    if (a->cpu_percent < b->cpu_percent) return 1;
    if (a->cpu_percent > b->cpu_percent) return -1;
    return a->pid > b->pid ? 1 : a->pid < b->pid ? -1 : 0;
}

double lsm_process_cpu_total_percent(uint64_t process_delta,
                                     uint64_t system_delta)
{
    return lsm_percent_u64(process_delta, system_delta);
}

size_t lsm_process_scan(LsmProcessBackend *backend,
                        LsmProcessInfo **out_processes,
                        unsigned scan_flags)
{
    if (!backend || !out_processes) return 0;
    *out_processes = NULL;

    uint64_t current_total_ticks = read_total_cpu_ticks();
    uint64_t total_delta = current_total_ticks >= backend->previous_total_cpu_ticks ?
                           current_total_ticks - backend->previous_total_cpu_ticks : 0;
    backend->previous_total_cpu_ticks = current_total_ticks;

    DIR *directory = opendir("/proc");
    if (!directory) return 0;

    backend->generation++;
    const size_t searchable_sample_count = backend->sample_count;
    size_t count = 0U;
    size_t capacity = 512U;
    LsmProcessInfo *processes = calloc(capacity, sizeof(*processes));
    if (!processes) {
        closedir(directory);
        return 0;
    }

    const uint64_t total_memory = backend->total_memory_bytes;
    const long ticks_per_second = backend->ticks_per_second;
    const int64_t boot_time = backend->boot_time_epoch;
    const double uptime = read_uptime_seconds();
    const double sampled_at = lsm_monotonic_seconds();

    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (!numeric_name(entry->d_name)) continue;
        pid_t pid = (pid_t)atoi(entry->d_name);
        if (pid <= 0) continue;

        if (count == capacity) {
            if (capacity > SIZE_MAX / 2U ||
                capacity * 2U > SIZE_MAX / sizeof(*processes))
                break;
            capacity *= 2U;
            LsmProcessInfo *grown = realloc(processes, capacity * sizeof(*grown));
            if (!grown) break;
            processes = grown;
        }

        LsmProcessInfo process = {0};
        LinuxProcessStat native_stat = {0};
        LinuxProcessStatus native_status = {0};
        process.pid = (LsmProcessId)pid;
        if (!read_process_stat(pid, &process, &native_stat)) continue;
        process.instance_id = native_stat.start_ticks;
        process.priority = priority_from_nice(native_stat.nice_value);
        process.efficiency_mode = native_stat.nice_value >= 10;
        read_process_status(backend, pid, &process, &native_status);
        process.owned_by_current_user = native_status.uid_available &&
            native_status.uid == backend->current_uid;
        read_process_command(pid, &process);
        read_process_io(pid, &process);
        if (scan_flags) (void)lsm_process_enrich(pid, &process, scan_flags);

        process.memory_percent =
            lsm_percent_u64(process.rss_bytes, total_memory);
        const uint64_t cpu_ticks = native_stat.cpu_ticks;
        if (ticks_per_second > 0) {
            process.cpu_time_seconds = cpu_ticks / (uint64_t)ticks_per_second;
            const long double ns = ((long double)cpu_ticks * 1000000000.0L) /
                                   (long double)ticks_per_second;
            process.cpu_time_nanoseconds = ns >= (long double)UINT64_MAX
                ? UINT64_MAX : (uint64_t)ns;
            double started_after_boot = (double)process.instance_id / (double)ticks_per_second;
            process.start_time_epoch = boot_time > 0 ?
                boot_time + (int64_t)started_after_boot : 0;
            process.elapsed_seconds = uptime > started_after_boot ?
                (uint64_t)(uptime - started_after_boot) : 0;
        }

        PreviousProcessSample *sample = find_or_create_sample(
            backend, searchable_sample_count, pid);
        if (sample) {
            bool same_process = sample->start_ticks == process.instance_id && sample->start_ticks != 0;
            if (same_process && total_delta > 0 && cpu_ticks >= sample->cpu_ticks) {
                const uint64_t delta =
                    cpu_ticks - sample->cpu_ticks;
                process.cpu_percent = lsm_process_cpu_total_percent(
                    delta, total_delta);
            }
            double interval = sampled_at - sample->sampled_at;
            if (same_process && sample->sampled_at > 0.0 && interval > 0.0) {
                (void)lsm_u64_counter_rate(
                    process.read_bytes, sample->read_bytes, 1.0L, interval,
                    &process.read_bytes_per_sec);
                (void)lsm_u64_counter_rate(
                    process.write_bytes, sample->write_bytes, 1.0L, interval,
                    &process.write_bytes_per_sec);
            }
            if ((scan_flags & LSM_PROCESS_SCAN_GPU) != 0U) {
                LsmProcessGpuSnapshot gpu;
                if (lsm_process_gpu_read("/proc", pid, &gpu)) {
                    process.gpu_memory_bytes = gpu.memory_available
                        ? gpu.memory_bytes : 0U;
                    process.gpu_memory_available = gpu.memory_available;
                    const double gpu_interval =
                        sampled_at - sample->gpu_sampled_at;
                    if (same_process && sample->gpu_sampled_at > 0.0)
                        lsm_process_gpu_normalise(&gpu, &sample->gpu);
                    process.gpu_available = same_process &&
                        sample->gpu_sampled_at > 0.0 &&
                        lsm_process_gpu_calculate_engine(
                            &gpu, &sample->gpu, gpu_interval,
                            &process.gpu_percent, process.gpu_engine,
                            sizeof(process.gpu_engine));
                    sample->gpu = gpu;
                    sample->gpu_sampled_at = sampled_at;
                }
            }
            sample->start_ticks = process.instance_id;
            sample->cpu_ticks = cpu_ticks;
            sample->read_bytes = process.read_bytes;
            sample->write_bytes = process.write_bytes;
            sample->sampled_at = sampled_at;
            sample->generation = backend->generation;
        }
        processes[count++] = process;
    }
    closedir(directory);

    /* Removing entries from a sorted prefix preserves its ordering. A qsort is
     * required only when newly observed PIDs were appended during this scan. */
    const bool samples_appended =
        backend->sample_count > searchable_sample_count;

    /* Drop retained samples after their process disappears. */
    size_t write = 0;
    for (size_t i = 0; i < backend->sample_count; i++) {
        if (backend->samples[i].generation == backend->generation) {
            backend->samples[write++] = backend->samples[i];
        }
    }
    backend->sample_count = write;
    if (samples_appended && backend->sample_count > 1U)
        qsort(backend->samples, backend->sample_count,
              sizeof(*backend->samples), compare_sample_pid);

    if (count > 1U)
        qsort(processes, count, sizeof(*processes), compare_process_cpu);
    *out_processes = processes;
    return count;
}

bool lsm_process_set_priority(LsmProcessId process_id,
                              LsmProcessInstanceId instance_id,
                              LsmProcessPriority priority)
{
    pid_t pid = 0;
    if (!process_identity_pid(process_id, instance_id, &pid)) return false;
    if (pid <= 1) {
        errno = EINVAL;
        return false;
    }
    const int nice_value = nice_from_priority(priority);
    return setpriority(PRIO_PROCESS, (id_t)pid, nice_value) == 0;
}

bool lsm_process_set_efficiency(LsmProcessId process_id,
                                LsmProcessInstanceId instance_id,
                                bool enabled)
{
    pid_t pid = 0;
    if (!process_identity_pid(process_id, instance_id, &pid)) return false;
    if (pid <= 1) {
        errno = EINVAL;
        return false;
    }

    /*
     * Efficiency mode deliberately uses only standard scheduler controls:
     * lower CPU priority plus the idle block-I/O class.  No resident service,
     * cgroup or external command is required.  Linux may refuse a later CPU
     * priority increase for an unprivileged caller; the UI reports that error
     * rather than pretending the original priority was restored.
     */
    const int nice_value = enabled ? 10 : 0;
    const int io_value = enabled
        ? LSM_IOPRIO_VALUE(IOPRIO_CLASS_IDLE, 0)
        : LSM_IOPRIO_VALUE(IOPRIO_CLASS_BE, 4);

    bool io_ok = false;
    int io_error = 0;
#ifdef SYS_ioprio_set
    io_ok = syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, pid, io_value) == 0;
    if (!io_ok) io_error = errno;
#endif

    const bool cpu_ok = setpriority(PRIO_PROCESS, (id_t)pid, nice_value) == 0;
    const int cpu_error = cpu_ok ? 0 : errno;

    /* Some containers and older kernels block ioprio_set.  A successfully
     * lowered CPU priority is still useful Efficiency mode, and vice versa. */
    if (cpu_ok || io_ok) return true;
    errno = cpu_error ? cpu_error : io_error;
    return false;
}

size_t lsm_process_affinity_get(LsmProcessId process_id,
                                LsmProcessInstanceId instance_id,
                                bool *enabled, size_t capacity)
{
    pid_t pid = 0;
    if (!enabled || capacity == 0U) {
        errno = EINVAL;
        return 0U;
    }
    if (!process_identity_pid(process_id, instance_id, &pid)) return 0U;
    cpu_set_t set;
    CPU_ZERO(&set);
    if (sched_getaffinity(pid, sizeof(set), &set) != 0) return 0;

    long configured = sysconf(_SC_NPROCESSORS_CONF);
    size_t count = configured > 0 ? (size_t)configured : capacity;
    if (count > capacity) count = capacity;
    if (count > CPU_SETSIZE) count = CPU_SETSIZE;
    for (size_t i = 0; i < count; i++) enabled[i] = CPU_ISSET((int)i, &set) != 0;
    return count;
}

bool lsm_process_affinity_set(LsmProcessId process_id,
                              LsmProcessInstanceId instance_id,
                              const bool *enabled, size_t count)
{
    pid_t pid = 0;
    if (!enabled || count == 0U || count > CPU_SETSIZE) {
        errno = EINVAL;
        return false;
    }
    if (!process_identity_pid(process_id, instance_id, &pid)) return false;
    if (pid <= 1) {
        errno = EINVAL;
        return false;
    }
    cpu_set_t set;
    CPU_ZERO(&set);
    bool any = false;
    for (size_t i = 0; i < count; i++) {
        if (enabled[i]) {
            CPU_SET((int)i, &set);
            any = true;
        }
    }
    if (!any) {
        errno = EINVAL;
        return false;
    }
    return sched_setaffinity(pid, sizeof(set), &set) == 0;
}

static bool read_pid_parent(pid_t pid, PidParent *item)
{
    if (!item) return false;
    LsmProcessInfo process = { .pid = (LsmProcessId)pid };
    LinuxProcessStat native_stat = {0};
    if (!read_process_stat(pid, &process, &native_stat)) return false;
    item->pid = pid;
    item->ppid = process.ppid <= (LsmProcessId)INT_MAX
        ? (pid_t)process.ppid : 0;
    item->instance_id = native_stat.start_ticks;
    item->depth = 0U;
    return true;
}

static int compare_pid_parent_pid(const void *left, const void *right)
{
    const PidParent *a = left;
    const PidParent *b = right;
    return a->pid > b->pid ? 1 : a->pid < b->pid ? -1 : 0;
}

static const PidParent *find_pid_parent(const PidParent *items, size_t count,
                                        pid_t pid)
{
    size_t low = 0U;
    size_t high = count;
    while (low < high) {
        const size_t middle = low + (high - low) / 2U;
        if (items[middle].pid < pid)
            low = middle + 1U;
        else
            high = middle;
    }
    return low < count && items[low].pid == pid ? &items[low] : NULL;
}

/* Parent resolution is performed by binary search over a PID-sorted snapshot.
 * Signalling a large tree is not a periodic hot path, but avoiding the former
 * nested linear walk keeps the GUI responsive on hosts with thousands of
 * processes and deeply nested service trees. */
static unsigned process_depth(const PidParent *items, size_t count,
                              size_t index, pid_t root)
{
    unsigned depth = 0U;
    pid_t current = items[index].pid;
    for (size_t guard = 0U; guard < count && current != root; guard++) {
        const PidParent *record = find_pid_parent(items, count, current);
        if (!record) return 0U;
        current = record->ppid;
        depth++;
    }
    return current == root ? depth : 0U;
}

static int compare_depth_descending(const void *left, const void *right)
{
    const PidParent *a = left, *b = right;
    if (a->depth < b->depth) return 1;
    if (a->depth > b->depth) return -1;
    return a->pid > b->pid ? -1 : a->pid < b->pid ? 1 : 0;
}

bool lsm_process_control_tree(LsmProcessId root_id,
                              LsmProcessInstanceId root_instance_id,
                              LsmProcessControl action)
{
    pid_t root_pid = 0;
    const int signal_number = signal_from_control(action);
    if (!process_identity_pid(root_id, root_instance_id, &root_pid)) return false;
    if (root_pid <= 1 || signal_number <= 0) {
        errno = EINVAL;
        return false;
    }

    DIR *directory = opendir("/proc");
    if (!directory) return false;
    size_t count = 0, capacity = 256;
    PidParent *items = calloc(capacity, sizeof(*items));
    if (!items) {
        closedir(directory);
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (!numeric_name(entry->d_name)) continue;
        const pid_t pid = (pid_t)atoi(entry->d_name);
        PidParent item = {0};
        if (pid <= 1 || !read_pid_parent(pid, &item)) continue;
        if (count == capacity) {
            if (capacity > SIZE_MAX / 2U ||
                capacity * 2U > SIZE_MAX / sizeof(*items))
                break;
            capacity *= 2U;
            PidParent *grown = realloc(items, capacity * sizeof(*grown));
            if (!grown) break;
            items = grown;
        }
        items[count++] = item;
    }
    closedir(directory);
    if (!process_identity_pid(root_id, root_instance_id, &root_pid)) {
        free(items);
        return false;
    }
    if (count > 1U)
        qsort(items, count, sizeof(*items), compare_pid_parent_pid);

    PidParent *descendant_items = calloc(count ? count : 1, sizeof(*descendant_items));
    if (!descendant_items) {
        free(items);
        return false;
    }
    size_t descendants = 0;
    for (size_t i = 0; i < count; i++) {
        unsigned depth = process_depth(items, count, i, root_pid);
        if (depth > 0) {
            descendant_items[descendants] = items[i];
            descendant_items[descendants++].depth = depth;
        }
    }
    qsort(descendant_items, descendants, sizeof(*descendant_items), compare_depth_descending);

    bool success = true;
    int saved_errno = 0;
    for (size_t i = 0; i < descendants; i++) {
        if (!lsm_process_control((LsmProcessId)descendant_items[i].pid,
                                 descendant_items[i].instance_id, action) &&
            errno != ESRCH) {
            if (!saved_errno) saved_errno = errno;
            success = false;
        }
    }
    if (!lsm_process_control(root_id, root_instance_id, action) && errno != ESRCH) {
        if (!saved_errno) saved_errno = errno;
        success = false;
    }
    free(descendant_items);
    free(items);
    if (!success) errno = saved_errno;
    return success;
}

void lsm_process_list_free(LsmProcessInfo *processes)
{
    free(processes);
}

LsmProcessBackend *lsm_process_backend_create(void)
{
    LsmProcessBackend *backend = calloc(1U, sizeof(*backend));
    if (!backend) return NULL;

    const long page_size = sysconf(_SC_PAGESIZE);
    const long pages = sysconf(_SC_PHYS_PAGES);
    backend->total_memory_bytes = page_size > 0 && pages > 0
        ? lsm_u64_multiply_saturating(
            (uint64_t)page_size, (uint64_t)pages) : 0U;
    backend->ticks_per_second = sysconf(_SC_CLK_TCK);
    backend->boot_time_epoch = read_boot_time();
    backend->current_uid = getuid();

    long suggested = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (suggested < 4096L) suggested = 4096L;
    if (suggested > 65536L) suggested = 65536L;
    backend->passwd_buffer_size = (size_t)suggested;
    backend->passwd_buffer = malloc(backend->passwd_buffer_size);
    if (!backend->passwd_buffer) backend->passwd_buffer_size = 0U;
    return backend;
}

void lsm_process_backend_destroy(LsmProcessBackend *backend)
{
    if (!backend) return;
    free(backend->passwd_buffer);
    free(backend->samples);
    free(backend);
}

bool lsm_process_control(LsmProcessId process_id,
                         LsmProcessInstanceId instance_id,
                         LsmProcessControl action)
{
    pid_t pid = 0;
    const int signal_number = signal_from_control(action);
    if (!process_identity_pid(process_id, instance_id, &pid)) return false;
    if (pid <= 1 || signal_number <= 0) {
        errno = EINVAL;
        return false;
    }
    return kill(pid, signal_number) == 0;
}

void lsm_process_error_message(char *buffer, size_t size)
{
    if (!buffer || size == 0U) return;
    const int error = errno;
    if (error == 0) {
        (void)snprintf(buffer, size, "Unknown process backend error");
        return;
    }
    (void)snprintf(buffer, size, "%s", strerror(error));
}
