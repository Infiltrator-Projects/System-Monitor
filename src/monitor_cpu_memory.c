// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_cpu_memory.c
 * @brief CPU, process/thread totals and physical-memory collection.
 *
 * This module owns scheduler counters, direct CPUID identity/topology, cached
 * frequency attributes, sysinfo memory accounting, temperature and SMBIOS.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_linux_internal.h"
#include "common.h"
#include "cpu_accounting.h"
#include "cpu_direct.h"
#include "memory_accounting.h"
#include "memory_hardware.h"
#include "system_sources.h"

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

/* Non-x86 and incomplete-CPUID fallback for immutable cache geometry. */
static uint64_t parse_cache_size_bytes(const char *text)
{
    uint64_t bytes = 0U;
    return infiltratr_parse_binary_quantity_u64(text, &bytes) ? bytes : 0U;
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

static void read_cpu_cache_totals(LsmCpuInfo *cpu)
{
    const size_t capacity = (size_t)cpu->logical_cores * 16U + 16U;
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

    for (unsigned cpu_index = 0; cpu_index < cpu->logical_cores; cpu_index++) {
        for (int index = 0; index < 32; index++) {
            char path[LSM_PATH_LEN], level_text[32], type[32] = "", size_text[32] = "";
            char shared[128] = "";
            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/cache/index%d/level",
                     cpu_index, index);
            if (!lsm_read_text_file(path, level_text, sizeof(level_text))) continue;
            const int level = atoi(level_text);
            if (level < 1 || level > 3) continue;

            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/cache/index%d/type",
                     cpu_index, index);
            lsm_read_text_file(path, type, sizeof(type));
            if (level == 1 && strcmp(type, "Data") != 0) continue;

            snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%u/cache/index%d/size",
                     cpu_index, index);
            if (!lsm_read_text_file(path, size_text, sizeof(size_text))) continue;
            const uint64_t bytes = parse_cache_size_bytes(size_text);
            if (!bytes) continue;

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

            snprintf(seen[seen_count].key, sizeof(seen[seen_count].key), "%s", key);
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
                snprintf(monitor->cpu.model, sizeof(monitor->cpu.model), "%s", value);
                model_found = true;
            } else if (strcmp(line, "physical id") == 0) {
                physical_id = atoi(value);
            } else if (strcmp(line, "core id") == 0) {
                core_id = atoi(value);
                if (physical_id >= 0 && physical_id < 256 && core_id >= 0 && core_id < 256 &&
                    !physical_pairs[physical_id][core_id]) {
                    physical_pairs[physical_id][core_id] = true;
                    physical_count++;
                }
            } else if (strcmp(line, "flags") == 0 || strcmp(line, "Features") == 0) {
                if (strstr(value, " vmx") || strstr(value, " svm") || strstr(value, "virt"))
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
    const unsigned logical = cpu && cpu->logical_cores <= LSM_MAX_CPUS
        ? cpu->logical_cores : LSM_MAX_CPUS;
    for (unsigned index = 0U; index < logical; index++) {
        char path[LSM_PATH_LEN];
        char value[64];
        (void)snprintf(path, sizeof(path),
                       "/sys/devices/system/cpu/cpu%u/topology/physical_package_id",
                       index);
        if (!lsm_read_text_file(path, value, sizeof(value))) continue;
        char *end = NULL;
        errno = 0;
        const long package = strtol(value, &end, 10);
        if (errno != 0 || end == value || (*end && *end != '\n') ||
            package < INT_MIN || package > INT_MAX)
            continue;
        bool known = false;
        for (size_t current = 0U; current < count; current++)
            if (packages[current] == (int)package) known = true;
        if (!known && count < LSM_MAX_CPUS) packages[count++] = (int)package;
    }
    return count > 0U ? (unsigned)count : 1U;
}

static unsigned read_numa_node_count(void)
{
    DIR *directory = opendir("/sys/devices/system/node");
    if (!directory) return 1U;
    unsigned count = 0U;
    struct dirent *entry = NULL;
    while ((entry = readdir(directory))) {
        if (strncmp(entry->d_name, "node", 4U) != 0 ||
            !isdigit((unsigned char)entry->d_name[4]))
            continue;
        bool numeric = true;
        for (const char *cursor = entry->d_name + 4U; *cursor; cursor++)
            if (!isdigit((unsigned char)*cursor)) numeric = false;
        if (numeric && count < UINT_MAX) count++;
    }
    closedir(directory);
    return count > 0U ? count : 1U;
}

static void update_load_average(LsmCpuInfo *cpu)
{
    double values[3] = {0.0, 0.0, 0.0};
    if (!cpu || getloadavg(values, 3) != 3) return;
    cpu->load_average_1 = values[0];
    cpu->load_average_5 = values[1];
    cpu->load_average_15 = values[2];
}

typedef struct {
    char **current_paths;
    char **maximum_paths;
    size_t count;
} LsmCpuFrequencySource;

/* cpufreq paths are discovered once and reused rather than rescanned. */
static void destroy_cpu_frequency_source(LsmCpuFrequencySource *source)
{
    if (!source) return;
    for (size_t index = 0U; index < source->count; index++) {
        free(source->current_paths[index]);
        free(source->maximum_paths[index]);
    }
    free(source->current_paths);
    free(source->maximum_paths);
    free(source);
}

static LsmCpuFrequencySource *create_cpu_frequency_source(void)
{
    DIR *directory = opendir("/sys/devices/system/cpu/cpufreq");
    if (!directory) return NULL;

    size_t capacity = 16U;
    LsmCpuFrequencySource *source = calloc(1U, sizeof(*source));
    if (!source) {
        closedir(directory);
        return NULL;
    }
    source->current_paths = calloc(capacity, sizeof(*source->current_paths));
    source->maximum_paths = calloc(capacity, sizeof(*source->maximum_paths));
    if (!source->current_paths || !source->maximum_paths) {
        closedir(directory);
        destroy_cpu_frequency_source(source);
        return NULL;
    }

    struct dirent *entry = NULL;
    while ((entry = readdir(directory))) {
        if (strncmp(entry->d_name, "policy", 6U) != 0 ||
            !isdigit((unsigned char)entry->d_name[6])) continue;
        if (source->count == capacity) {
            const size_t next = capacity * 2U;
            char **current = realloc(source->current_paths, next * sizeof(*current));
            if (!current) break;
            source->current_paths = current;
            char **maximum = realloc(source->maximum_paths, next * sizeof(*maximum));
            if (!maximum) break;
            source->maximum_paths = maximum;
            memset(source->current_paths + capacity, 0,
                   (next - capacity) * sizeof(*source->current_paths));
            memset(source->maximum_paths + capacity, 0,
                   (next - capacity) * sizeof(*source->maximum_paths));
            capacity = next;
        }

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
        source->current_paths[source->count] = strdup(current);
        source->maximum_paths[source->count] = strdup(maximum);
        if (!source->current_paths[source->count] ||
            !source->maximum_paths[source->count]) {
            free(source->current_paths[source->count]);
            free(source->maximum_paths[source->count]);
            source->current_paths[source->count] = NULL;
            source->maximum_paths[source->count] = NULL;
            break;
        }
        source->count++;
    }
    closedir(directory);
    if (source->count == 0U) {
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
            const char *path = maximum ? source->maximum_paths[index]
                                       : source->current_paths[index];
            const uint64_t khz = lsm_read_u64_or_zero(path);
            if (!khz) continue;
            total_khz += (double)khz;
            count++;
        }
        if (count > 0U) return total_khz / (double)count / 1000000.0;
    }

    if (cpu->base_frequency_ghz > 0.0) return cpu->base_frequency_ghz;
    return 0.0;
}

static double read_temperature_c(LsmMonitor *monitor)
{
    LsmSystemSources *sources = monitor_system_sources(monitor);
    if (!sources) return NAN;
    return lsm_sources_read_cpu_temperature(sources);
}


/* sysinfo supplies fast totals every sample. The kernel's authoritative
 * MemAvailable value is also read every sample, while reclaimable/cache detail
 * fields retain the slower detail cadence. */
static uint64_t read_system_file_handles(void)
{
    FILE *file = fopen("/proc/sys/fs/file-nr", "r");
    if (!file) return 0U;
    unsigned long long allocated = 0U;
    const bool valid = fscanf(file, "%llu", &allocated) == 1;
    fclose(file);
    return valid ? (uint64_t)allocated : 0U;
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
        uint64_t available = lsm_u64_add_saturating(
            memory->free_bytes, memory->buffers_bytes);
        available = lsm_u64_add_saturating(
            available, memory->cached_bytes);
        memory->available_bytes = available < memory->total_bytes
            ? available : memory->total_bytes;
    }
    if (memory->free_bytes > memory->total_bytes)
        memory->free_bytes = memory->total_bytes;
    if (memory->buffers_bytes > memory->total_bytes)
        memory->buffers_bytes = memory->total_bytes;
    if (memory->cached_bytes > memory->total_bytes)
        memory->cached_bytes = memory->total_bytes;
    if (memory->available_bytes > memory->total_bytes)
        memory->available_bytes = memory->total_bytes;
    memory->used_bytes = memory->total_bytes > memory->available_bytes
        ? memory->total_bytes - memory->available_bytes : 0U;
    memory->usage_percent = lsm_percent_u64(
        memory->used_bytes, memory->total_bytes);
    monitor->cpu.file_handle_count = read_system_file_handles();
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
    monitor->cpu.frequency_ghz = read_cpu_frequency_ghz(monitor, false);
    if (monitor->cpu.max_frequency_ghz <= 0.0)
        monitor->cpu.max_frequency_ghz = read_cpu_frequency_ghz(monitor, true);
    monitor->cpu.temperature_c = read_temperature_c(monitor);
    const double now = lsm_monotonic_seconds();
    const bool refresh_memory_details =
        now - state->last_memory_detail_monotonic >= 10.0;
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
