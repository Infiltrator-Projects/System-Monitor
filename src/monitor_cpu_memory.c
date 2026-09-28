// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_cpu_memory.c
 * @brief CPU, process/thread totals and physical-memory collection.
 *
 * This module owns scheduler counters, direct CPUID identity/topology, cached
 * frequency attributes, sysinfo memory accounting, temperature and SMBIOS.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_linux_internal.h"
#include "common.h"
#include "cpu_accounting.h"
#include "cpu_direct.h"
#include "memory_accounting.h"
#include "memory_hardware.h"
#include "refresh_policy.h"
#include "system_sources.h"

#include <infiltratr/posix_numeric.h>
#include <infiltratr/quantity.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <unistd.h>

/* Scheduler utilisation and event rates come from one testable procfs parser. */
static bool read_cpu_counters(LsmMonitor *monitor, bool initial,
                              double elapsed_seconds)
{
    LsmCpuAccountingSample sample;
    if (!lsm_cpu_accounting_read("/proc/stat", &sample)) return false;
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return false;
    lsm_cpu_accounting_apply(&monitor->cpu, &state->cpu_accounting, &sample,
                             initial, elapsed_seconds);
    return true;
}

static void format_cache_summary(uint64_t bytes, unsigned instances,
                                 char *buffer, size_t buffer_size)
{
    if (!bytes || !instances) {
        snprintf(buffer, buffer_size, "N/A");
        return;
    }
    if (bytes % (1024ULL * 1024ULL) == 0) {
        snprintf(buffer, buffer_size, "%lluMB(%u%s)",
                 (unsigned long long)(bytes / (1024ULL * 1024ULL)),
                 instances, instances == 1 ? "instance" : "instances");
    } else if (bytes % 1024ULL == 0) {
        snprintf(buffer, buffer_size, "%lluKB(%u%s)",
                 (unsigned long long)(bytes / 1024ULL),
                 instances, instances == 1 ? "instance" : "instances");
    } else {
        snprintf(buffer, buffer_size, "%lluB(%u%s)",
                 (unsigned long long)bytes,
                 instances, instances == 1 ? "instance" : "instances");
    }
}

typedef struct {
    char key[192];
    int level;
    uint64_t bytes;
} LsmSeenCache;

static int compare_cpu_ids(const void *left, const void *right)
{
    const unsigned a = *(const unsigned *)left;
    const unsigned b = *(const unsigned *)right;
    return a > b ? 1 : a < b ? -1 : 0;
}

/* Kernel CPU identifiers are not guaranteed to be dense after hotplug or on
 * systems that expose sparse topology. Enumerate the actual online cpuN
 * directories instead of assuming the IDs are 0..logical_cores-1. */
static bool read_online_cpu_ids(unsigned ids[LSM_MAX_CPUS],
                                size_t *out_count)
{
    if (out_count) *out_count = 0U;
    if (!ids || !out_count) return false;
    DIR *directory = opendir("/sys/devices/system/cpu");
    if (!directory) return false;

    size_t count = 0U;
    bool complete = true;
    int enumeration_error = 0;
    struct dirent *entry = NULL;
    for (;;) {
        errno = 0;
        entry = readdir(directory);
        if (!entry) {
            if (errno != 0) {
                complete = false;
                enumeration_error = errno;
            }
            break;
        }
        if (!lsm_string_starts_with(entry->d_name, "cpu") ||
            !lsm_ascii_is_digit((unsigned char)entry->d_name[3]))
            continue;
        uint64_t parsed = 0U;
        if (!lsm_parse_u64(entry->d_name + 3U, 10U, &parsed) ||
            parsed > UINT_MAX)
            continue;

        char online_path[LSM_PATH_LEN];
        const int written = snprintf(
            online_path, sizeof(online_path),
            "/sys/devices/system/cpu/%s/online", entry->d_name);
        int64_t online = 1;
        if (written >= 0 && (size_t)written < sizeof(online_path)) {
            int64_t reported = 0;
            if (infiltratr_read_i64_file(online_path, &reported))
                online = reported;
        }
        if (online == 0) continue;
        if (count >= LSM_MAX_CPUS) {
            complete = false;
            enumeration_error = EOVERFLOW;
            continue;
        }
        ids[count++] = (unsigned)parsed;
    }
    if (closedir(directory) != 0 && complete) {
        complete = false;
        enumeration_error = errno != 0 ? errno : EIO;
    }
    if (!complete) {
        errno = enumeration_error != 0 ? enumeration_error : EIO;
        return false;
    }
    if (count > 1U)
        qsort(ids, count, sizeof(ids[0]), compare_cpu_ids);
    *out_count = count;
    return true;
}

static size_t read_topology_cpu_ids(unsigned ids[LSM_MAX_CPUS])
{
    size_t count = 0U;
    if (!read_online_cpu_ids(ids, &count) || count == 0U)
        return 0U;
    return count;
}

static void read_cpu_cache_totals(LsmCpuInfo *cpu)
{
    unsigned cpu_ids[LSM_MAX_CPUS];
    const size_t cpu_count = read_topology_cpu_ids(cpu_ids);
    const size_t capacity = cpu_count * 16U + 16U;
    LsmSeenCache *seen = calloc(capacity, sizeof(*seen));
    if (!seen) {
        snprintf(cpu->cache_l1, sizeof(cpu->cache_l1), "N/A");
        snprintf(cpu->cache_l2, sizeof(cpu->cache_l2), "N/A");
        snprintf(cpu->cache_l3, sizeof(cpu->cache_l3), "N/A");
        return;
    }

    size_t seen_count = 0;
    uint64_t totals[4] = {0, 0, 0, 0};
    unsigned instances[4] = {0, 0, 0, 0};

    for (size_t cpu_position = 0U; cpu_position < cpu_count; cpu_position++) {
        const unsigned cpu_index = cpu_ids[cpu_position];
        for (int index = 0; index < 32; index++) {
            char path[LSM_PATH_LEN], type[32] = "", size_text[32] = "";
            char shared[128] = "";
            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/cache/index%d/level",
                     cpu_index, index);
            int64_t level_value = 0;
            if (!infiltratr_read_i64_file(path, &level_value) ||
                level_value < 1 || level_value > 3)
                continue;
            const int level = (int)level_value;

            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/cache/index%d/type",
                     cpu_index, index);
            lsm_read_text_file(path, type, sizeof(type));
            if (level == 1 && strcmp(type, "Data") != 0) continue;

            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/cache/index%d/size",
                     cpu_index, index);
            if (!lsm_read_text_file(path, size_text, sizeof(size_text))) continue;
            uint64_t bytes = 0U;
            if (!infiltratr_parse_binary_quantity_u64(size_text, &bytes) ||
                bytes == 0U)
                continue;

            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/cache/index%d/shared_cpu_list",
                     cpu_index, index);
            if (!lsm_read_text_file(path, shared, sizeof(shared)))
                snprintf(shared, sizeof(shared), "%u", cpu_index);

            char key[192];
            snprintf(key, sizeof(key), "%d:%s:%s", level, type, shared);
            bool duplicate = false;
            for (size_t i = 0; i < seen_count; i++) {
                if (strcmp(seen[i].key, key) == 0) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate || seen_count >= capacity) continue;

            lsm_copy_string(seen[seen_count].key, sizeof(seen[seen_count].key), key);
            seen[seen_count].level = level;
            seen[seen_count].bytes = bytes;
            seen_count++;
            totals[level] =
                lsm_u64_add_saturating(totals[level], bytes);
            instances[level]++;
        }
    }

    format_cache_summary(totals[1], instances[1], cpu->cache_l1, sizeof(cpu->cache_l1));
    format_cache_summary(totals[2], instances[2], cpu->cache_l2, sizeof(cpu->cache_l2));
    format_cache_summary(totals[3], instances[3], cpu->cache_l3, sizeof(cpu->cache_l3));
    free(seen);
}

static void read_cpu_static(LsmMonitor *monitor)
{
    if (lsm_cpu_direct_read_static(&monitor->cpu)) return;

    monitor->cpu.logical_cores = (unsigned)sysconf(_SC_NPROCESSORS_ONLN);
    if (monitor->cpu.logical_cores == 0 || monitor->cpu.logical_cores > LSM_MAX_CPUS)
        monitor->cpu.logical_cores = 1;

    FILE *file = fopen("/proc/cpuinfo", "r");
    char line[512];
    bool model_found = false;
    bool physical_pairs[256][256] = {{false}};
    int physical_id = 0, core_id = 0;
    unsigned physical_count = 0;

    if (file) {
        while (fgets(line, sizeof(line), file)) {
            char *colon = strchr(line, ':');
            if (!colon) continue;
            *colon = '\0';
            char *value = colon + 1;
            lsm_trim(line);
            lsm_trim(value);
            if (!model_found && strcmp(line, "model name") == 0) {
                lsm_copy_string(monitor->cpu.model, sizeof(monitor->cpu.model), value);
                model_found = true;
            } else if (strcmp(line, "physical id") == 0) {
                int64_t parsed = 0;
                if (lsm_parse_i64_range(value, 10U, INT_MIN, INT_MAX,
                                        &parsed))
                    physical_id = (int)parsed;
            } else if (strcmp(line, "core id") == 0) {
                int64_t parsed = 0;
                if (!lsm_parse_i64_range(value, 10U, INT_MIN, INT_MAX,
                                         &parsed))
                    continue;
                core_id = (int)parsed;
                if (physical_id >= 0 && physical_id < 256 && core_id >= 0 && core_id < 256 &&
                    !physical_pairs[physical_id][core_id]) {
                    physical_pairs[physical_id][core_id] = true;
                    physical_count++;
                }
            } else if (strcmp(line, "flags") == 0 ||
                       strcmp(line, "Features") == 0) {
                monitor->cpu.virtualization_available = true;
                if (strstr(value, " vmx") || strstr(value, " svm") ||
                    strstr(value, "virt"))
                    monitor->cpu.virtualization = true;
            }
        }
        fclose(file);
    }
    if (!model_found) snprintf(monitor->cpu.model, sizeof(monitor->cpu.model), "Unknown processor");
    monitor->cpu.physical_cores = physical_count ? physical_count : monitor->cpu.logical_cores;

    read_cpu_cache_totals(&monitor->cpu);
}

static unsigned read_cpu_socket_count(const LsmCpuInfo *cpu)
{
    int packages[LSM_MAX_CPUS];
    size_t count = 0U;
    unsigned cpu_ids[LSM_MAX_CPUS];
    const size_t cpu_count = read_topology_cpu_ids(cpu_ids);
    for (size_t position = 0U; position < cpu_count; position++) {
        const unsigned cpu_id = cpu_ids[position];
        char path[LSM_PATH_LEN];
        (void)snprintf(path, sizeof(path),
                       "/sys/devices/system/cpu/cpu%u/topology/physical_package_id",
                       cpu_id);
        int64_t package = 0;
        if (!infiltratr_read_i64_file(path, &package) ||
            package < INT_MIN || package > INT_MAX)
            continue;
        bool known = false;
        for (size_t current = 0U; current < count; current++)
            if (packages[current] == (int)package) known = true;
        if (!known && count < LSM_MAX_CPUS) packages[count++] = (int)package;
    }
    return (unsigned)count;
}

static unsigned read_numa_node_count(void)
{
    DIR *directory = opendir("/sys/devices/system/node");
    if (!directory) return 0U;

    unsigned count = 0U;
    bool complete = true;
    int enumeration_error = 0;
    struct dirent *entry = NULL;
    for (;;) {
        errno = 0;
        entry = readdir(directory);
        if (!entry) {
            if (errno != 0) {
                complete = false;
                enumeration_error = errno;
            }
            break;
        }
        if (!lsm_string_starts_with(entry->d_name, "node") ||
            !lsm_ascii_is_digit((unsigned char)entry->d_name[4]))
            continue;
        uint64_t node = 0U;
        if (!lsm_parse_u64(entry->d_name + 4U, 10U, &node))
            continue;
        if (count == UINT_MAX) {
            complete = false;
            if (enumeration_error == 0) enumeration_error = EOVERFLOW;
            continue;
        }
        count++;
    }
    if (closedir(directory) != 0 && complete) {
        complete = false;
        enumeration_error = errno != 0 ? errno : EIO;
    }
    if (!complete) {
        errno = enumeration_error != 0 ? enumeration_error : EIO;
        return 0U;
    }
    return count;
}

static void update_load_average(LsmCpuInfo *cpu)
{
    double values[3] = {0.0, 0.0, 0.0};
    if (!cpu) return;
    cpu->load_average_available = false;
    if (getloadavg(values, 3) != 3) return;
    cpu->load_average_1 = values[0];
    cpu->load_average_5 = values[1];
    cpu->load_average_15 = values[2];
    cpu->load_average_available = true;
}

typedef struct {
    char *current_path;
    char *maximum_path;
} LsmCpuFrequencyPath;

typedef struct {
    LsmCpuFrequencyPath *paths;
    size_t count;
    size_t capacity;
} LsmCpuFrequencySource;

/* cpufreq paths are discovered once and reused rather than rescanned. */
static void destroy_cpu_frequency_source(LsmCpuFrequencySource *source)
{
    if (!source) return;
    for (size_t index = 0U; index < source->count; index++) {
        free(source->paths[index].current_path);
        free(source->paths[index].maximum_path);
    }
    free(source->paths);
    free(source);
}

static LsmCpuFrequencySource *create_cpu_frequency_source(void)
{
    DIR *directory = opendir("/sys/devices/system/cpu/cpufreq");
    if (!directory) return NULL;

    LsmCpuFrequencySource *source = calloc(1U, sizeof(*source));
    if (!source) {
        closedir(directory);
        return NULL;
    }

    bool failed = false;
    struct dirent *entry = NULL;
    for (;;) {
        errno = 0;
        entry = readdir(directory);
        if (!entry) {
            if (errno != 0) failed = true;
            break;
        }
        if (!lsm_string_starts_with(entry->d_name, "policy"))
            continue;
        uint64_t policy_index = 0U;
        if (!lsm_parse_u64(entry->d_name + 6U, 10U, &policy_index))
            continue;

        char current[LSM_PATH_LEN];
        char maximum[LSM_PATH_LEN];
        const int current_written = snprintf(
            current, sizeof(current),
            "/sys/devices/system/cpu/cpufreq/%s/scaling_cur_freq", entry->d_name);
        int maximum_written = snprintf(
            maximum, sizeof(maximum),
            "/sys/devices/system/cpu/cpufreq/%s/scaling_max_freq", entry->d_name);
        if (current_written < 0 || (size_t)current_written >= sizeof(current) ||
            maximum_written < 0 || (size_t)maximum_written >= sizeof(maximum))
            continue;
        if (access(maximum, R_OK) != 0) {
            maximum_written = snprintf(
                maximum, sizeof(maximum),
                "/sys/devices/system/cpu/cpufreq/%s/cpuinfo_max_freq", entry->d_name);
            if (maximum_written < 0 || (size_t)maximum_written >= sizeof(maximum))
                continue;
        }
        LsmCpuFrequencyPath candidate = {
            .current_path = strdup(current),
            .maximum_path = strdup(maximum)
        };
        if (!candidate.current_path || !candidate.maximum_path) {
            free(candidate.current_path);
            free(candidate.maximum_path);
            failed = true;
            errno = ENOMEM;
            break;
        }
        if (!lsm_array_reserve((void **)&source->paths, &source->capacity,
                               sizeof(*source->paths), source->count + 1U,
                               16U)) {
            free(candidate.current_path);
            free(candidate.maximum_path);
            failed = true;
            if (errno == 0) errno = ENOMEM;
            break;
        }
        source->paths[source->count++] = candidate;
    }
    if (closedir(directory) != 0) failed = true;
    if (failed || source->count == 0U) {
        destroy_cpu_frequency_source(source);
        return NULL;
    }
    return source;
}

static double read_cpu_frequency_ghz(const LsmMonitor *monitor, bool maximum)
{
    if (!monitor) return 0.0;
    const LsmCpuInfo *cpu = &monitor->cpu;
    if (maximum && cpu->max_frequency_ghz > 0.0)
        return cpu->max_frequency_ghz;

    const LsmLinuxMonitorBackendState *state = monitor_backend_state_const(monitor);
    const LsmCpuFrequencySource *source = state
        ? (const LsmCpuFrequencySource *)state->cpu_frequency_source : NULL;
    if (source) {
        double total_khz = 0.0;
        unsigned count = 0U;
        for (size_t index = 0U; index < source->count; index++) {
            const char *path = maximum
                ? source->paths[index].maximum_path
                : source->paths[index].current_path;
            const uint64_t khz = lsm_read_u64_or_zero(path);
            if (!khz) continue;
            total_khz += (double)khz;
            count++;
        }
        if (count > 0U) return total_khz / (double)count / 1000000.0;
    }

    /* Nominal/base frequency does not establish current or maximum speed. */
    return 0.0;
}

static void read_cpu_thermal(LsmMonitor *monitor)
{
    if (!monitor) return;
    LsmCpuInfo *cpu = &monitor->cpu;
    cpu->temperature_c = NAN;
    cpu->temperature_available = false;
    cpu->temperature_warning_c = NAN;
    cpu->temperature_warning_available = false;
    cpu->temperature_critical_c = NAN;
    cpu->temperature_critical_available = false;

    LsmSystemSources *sources = monitor_system_sources(monitor);
    LsmCpuThermalSample sample;
    if (!sources || !lsm_sources_read_cpu_thermal(sources, &sample))
        return;

    cpu->temperature_c = sample.temperature_c;
    cpu->temperature_available = isfinite(sample.temperature_c);
    cpu->temperature_warning_c = sample.warning_c;
    cpu->temperature_warning_available = isfinite(sample.warning_c);
    cpu->temperature_critical_c = sample.critical_c;
    cpu->temperature_critical_available = isfinite(sample.critical_c);
}


/* sysinfo supplies fast totals every sample. The kernel's authoritative
 * MemAvailable value is also read every sample, while reclaimable/cache detail
 * fields retain the slower detail cadence. */
static bool read_system_file_handles(uint64_t *count)
{
    if (!count) return false;
    char text[128];
    if (!lsm_read_text_file("/proc/sys/fs/file-nr", text, sizeof(text)))
        return false;
    const char *cursor = text;
    uint64_t allocated = 0U;
    if (!lsm_parse_u64_token(&cursor, 10U, &allocated))
        return false;
    *count = allocated;
    return true;
}

static void update_memory(LsmMonitor *monitor, bool refresh_details)
{
    if (!monitor) return;
    LsmMemoryInfo *memory = &monitor->memory;

    struct sysinfo information;
    if (sysinfo(&information) == 0) {
        monitor->cpu.uptime_seconds = information.uptime > 0
            ? (uint64_t)information.uptime : 0U;
        const uint64_t unit = information.mem_unit ? information.mem_unit : 1U;
        memory->total_bytes = lsm_u64_multiply_saturating(
            (uint64_t)information.totalram, unit);
        memory->free_bytes = lsm_u64_multiply_saturating(
            (uint64_t)information.freeram, unit);
        memory->buffers_bytes = lsm_u64_multiply_saturating(
            (uint64_t)information.bufferram, unit);
        memory->swap_total_bytes = lsm_u64_multiply_saturating(
            (uint64_t)information.totalswap, unit);
        const uint64_t free_swap = lsm_u64_multiply_saturating(
            (uint64_t)information.freeswap, unit);
        memory->swap_used_bytes = memory->swap_total_bytes >= free_swap
            ? memory->swap_total_bytes - free_swap : 0U;
    }

    const bool have_available = lsm_memory_accounting_read(
        "/proc/meminfo", memory, refresh_details);
    if (!have_available) {
        /*
         * MemAvailable has no faithful sysinfo substitute. Do not combine
         * current free/buffer counters with retained cache data and present
         * the result as a fresh measurement.
         */
        memory->available_bytes = 0U;
        memory->used_bytes = 0U;
        memory->committed_bytes = 0U;
        memory->commit_limit_bytes = 0U;
        memory->cached_bytes = 0U;
        memory->kernel_reclaimable_bytes = 0U;
        memory->kernel_nonreclaimable_bytes = 0U;
        memory->page_tables_bytes = 0U;
        memory->hardware_corrupted_bytes = 0U;
        memory->usage_percent = NAN;
    }
    if (memory->free_bytes > memory->total_bytes)
        memory->free_bytes = memory->total_bytes;
    if (memory->buffers_bytes > memory->total_bytes)
        memory->buffers_bytes = memory->total_bytes;
    if (memory->cached_bytes > memory->total_bytes)
        memory->cached_bytes = memory->total_bytes;
    if (memory->available_bytes > memory->total_bytes)
        memory->available_bytes = memory->total_bytes;
    if (have_available) {
        memory->used_bytes = memory->total_bytes > memory->available_bytes
            ? memory->total_bytes - memory->available_bytes : 0U;
        memory->usage_percent = lsm_percent_u64(
            memory->used_bytes, memory->total_bytes);
    }
    uint64_t file_handles = 0U;
    monitor->cpu.file_handle_count_available =
        read_system_file_handles(&file_handles);
    if (monitor->cpu.file_handle_count_available)
        monitor->cpu.file_handle_count = file_handles;
}


/* Public CPU and memory lifecycle. */
bool lsm_cpu_memory_initialise(LsmMonitor *monitor)
{
    if (!monitor) return false;
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return false;
    read_cpu_static(monitor);
    monitor->cpu.socket_count = read_cpu_socket_count(&monitor->cpu);
    monitor->cpu.numa_node_count = read_numa_node_count();
    update_load_average(&monitor->cpu);
    state->cpu_frequency_source = create_cpu_frequency_source();
    state->last_cpu_frequency_source_refresh_monotonic =
        lsm_monotonic_seconds();
    if (!read_cpu_counters(monitor, true, 0.0)) return false;
    update_memory(monitor, true);
    state->last_memory_detail_monotonic = lsm_monotonic_seconds();
    (void)lsm_memory_hardware_read_direct(&monitor->memory);
    return true;
}

void lsm_cpu_memory_update(LsmMonitor *monitor, double elapsed_seconds)
{
    if (!monitor) return;
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return;
    (void)read_cpu_counters(monitor, false, elapsed_seconds);
    update_load_average(&monitor->cpu);
    const double now = lsm_monotonic_seconds();
    double current_frequency = read_cpu_frequency_ghz(monitor, false);
    if (current_frequency <= 0.0 &&
        lsm_refresh_interval_due(
            now, state->last_cpu_frequency_source_refresh_monotonic, 30.0)) {
        destroy_cpu_frequency_source(
            (LsmCpuFrequencySource *)state->cpu_frequency_source);
        state->cpu_frequency_source = create_cpu_frequency_source();
        state->last_cpu_frequency_source_refresh_monotonic = now;
        current_frequency = read_cpu_frequency_ghz(monitor, false);
        monitor->cpu.max_frequency_ghz = 0.0;
    }
    if (current_frequency > 0.0)
        monitor->cpu.frequency_ghz = current_frequency;
    if (monitor->cpu.max_frequency_ghz <= 0.0) {
        const double maximum_frequency =
            read_cpu_frequency_ghz(monitor, true);
        if (maximum_frequency > 0.0)
            monitor->cpu.max_frequency_ghz = maximum_frequency;
    }
    read_cpu_thermal(monitor);
    const bool refresh_memory_details = lsm_refresh_interval_due(
        now, state->last_memory_detail_monotonic, 10.0);
    update_memory(monitor, refresh_memory_details);
    if (refresh_memory_details) state->last_memory_detail_monotonic = now;
}


void lsm_cpu_memory_shutdown(LsmMonitor *monitor)
{
    if (!monitor) return;
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return;
    destroy_cpu_frequency_source(
        (LsmCpuFrequencySource *)state->cpu_frequency_source);
    state->cpu_frequency_source = NULL;
}
