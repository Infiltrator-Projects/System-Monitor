// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_gpu.c
 * @brief Native DRM per-process engine and graphics-memory accounting.
 *
 * DRM exposes cumulative client counters through procfs descriptor information.
 * A process may hold several descriptors for the same client, so client identity
 * is deduplicated before counters are combined. No device node, helper process,
 * command invocation or elevated access is required.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "process_gpu.h"

#include "common.h"

#include <infiltratr/quantity.h>

#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define LSM_PROCESS_GPU_MAX_CLIENTS 128
#define LSM_PROCESS_GPU_MAX_REGIONS 32

typedef struct {
    char name[96];
    uint64_t bytes;
    bool resident_preferred;
} LsmDrmMemoryRegion;

typedef struct {
    char key[192];
    char device_key[192];
    LsmProcessGpuEngine engines[LSM_PROCESS_GPU_MAX_ENGINES];
    size_t engine_count;
    LsmDrmMemoryRegion regions[LSM_PROCESS_GPU_MAX_REGIONS];
    size_t region_count;
    bool memory_available;
} LsmDrmClient;

static char *field_value(char *line)
{
    char *separator = strchr(line, ':');
    if (!separator) return NULL;
    char *value = separator + 1;
    lsm_trim(value);
    return value;
}

static void add_engine(LsmProcessGpuEngine *engines, size_t *count,
                       const char *name, uint64_t time_ns,
                       unsigned capacity, bool have_time)
{
    if (!engines || !count || !name || !*name) return;
    for (size_t index = 0U; index < *count; index++) {
        if (strcmp(engines[index].name, name) != 0) continue;
        if (have_time) {
            engines[index].time_ns = lsm_u64_add_saturating(
                engines[index].time_ns, time_ns);
            engines[index].time_available = true;
        }
        if (capacity > engines[index].capacity)
            engines[index].capacity = capacity;
        return;
    }
    if (*count >= LSM_PROCESS_GPU_MAX_ENGINES) return;
    LsmProcessGpuEngine *engine = &engines[(*count)++];
    memset(engine, 0, sizeof(*engine));
    lsm_copy_string(engine->name, sizeof(engine->name), name);
    engine->time_ns = time_ns;
    engine->capacity = capacity > 0U ? capacity : 1U;
    engine->time_available = have_time;
}

static void set_memory_region(LsmDrmClient *client, const char *name,
                              uint64_t bytes, bool resident)
{
    if (!client || !name || !*name) return;
    for (size_t index = 0U; index < client->region_count; index++) {
        LsmDrmMemoryRegion *region = &client->regions[index];
        if (strcmp(region->name, name) != 0) continue;
        if (resident || !region->resident_preferred) region->bytes = bytes;
        region->resident_preferred = region->resident_preferred || resident;
        return;
    }
    if (client->region_count >= LSM_PROCESS_GPU_MAX_REGIONS) return;
    LsmDrmMemoryRegion *region = &client->regions[client->region_count++];
    memset(region, 0, sizeof(*region));
    lsm_copy_string(region->name, sizeof(region->name), name);
    region->bytes = bytes;
    region->resident_preferred = resident;
}

static bool read_client_file(const char *path, LsmDrmClient *client)
{
    FILE *file = fopen(path, "r");
    if (!file) return false;

    char driver[64] = "";
    char device[96] = "";
    uint64_t client_id = 0U;
    bool have_client_id = false;
    bool recognised = false;
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        if (lsm_string_starts_with(line, "drm-driver:")) {
            char *value = field_value(line);
            if (value) lsm_copy_string(driver, sizeof(driver), value);
        } else if (lsm_string_starts_with(line, "drm-pdev:")) {
            char *value = field_value(line);
            if (value) lsm_copy_string(device, sizeof(device), value);
        } else if (lsm_string_starts_with(line, "drm-minor:") && !device[0]) {
            char *value = field_value(line);
            if (value) (void)snprintf(device, sizeof(device), "minor-%s", value);
        } else if (lsm_string_starts_with(line, "drm-client-id:")) {
            char *value = field_value(line);
            const char *cursor = value;
            have_client_id = value &&
                lsm_parse_u64_token(&cursor, 10U, &client_id);
        } else if (lsm_string_starts_with(line, "drm-engine-capacity-")) {
            char *separator = strchr(line, ':');
            uint64_t capacity = 0U;
            const char *cursor = separator ? separator + 1 : NULL;
            if (!cursor || !lsm_parse_u64_token(&cursor, 10U, &capacity) ||
                capacity == 0U)
                continue;
            *separator = '\0';
            add_engine(client->engines, &client->engine_count,
                       line + 20U, 0U,
                       capacity > (uint64_t)UINT_MAX
                           ? UINT_MAX : (unsigned)capacity,
                       false);
        } else if (lsm_string_starts_with(line, "drm-engine-")) {
            char *separator = strchr(line, ':');
            uint64_t time_ns = 0U;
            const char *cursor = separator ? separator + 1 : NULL;
            if (!cursor || !lsm_parse_u64_token(&cursor, 10U, &time_ns))
                continue;
            *separator = '\0';
            add_engine(client->engines, &client->engine_count,
                       line + 11U, time_ns, 1U, true);
            recognised = true;
        } else if (lsm_string_starts_with(line, "drm-resident-") ||
                   lsm_string_starts_with(line, "drm-memory-")) {
            const bool resident = lsm_string_starts_with(line, "drm-resident-");
            const size_t prefix = resident ? 13U : 11U;
            char *separator = strchr(line, ':');
            char *value = field_value(line);
            uint64_t bytes = 0U;
            if (!separator || !value ||
                !infiltratr_parse_binary_quantity_u64(value, &bytes))
                continue;
            *separator = '\0';
            set_memory_region(client, line + prefix, bytes, resident);
            client->memory_available = true;
            recognised = true;
        }
    }
    fclose(file);
    if (!recognised) return false;

    (void)snprintf(client->device_key, sizeof(client->device_key), "%s:%s",
                   driver, device);
    if (have_client_id)
        (void)snprintf(client->key, sizeof(client->key), "%s:%s:%llu",
                       driver, device, (unsigned long long)client_id);
    else
        (void)snprintf(client->key, sizeof(client->key), "fd:%.180s",
                       lsm_path_basename(path));
    return true;
}

static bool client_seen(char seen[][192], size_t count, const char *key)
{
    for (size_t index = 0U; index < count; index++)
        if (strcmp(seen[index], key) == 0) return true;
    return false;
}

bool lsm_process_gpu_read(const char *proc_root, LsmProcessId pid,
                          LsmProcessGpuSnapshot *snapshot)
{
    if (!proc_root || !*proc_root || pid <= 0 || !snapshot) return false;
    memset(snapshot, 0, sizeof(*snapshot));

    char directory_path[512];
    const int written = snprintf(directory_path, sizeof(directory_path),
                                 "%s/%llu/fdinfo", proc_root,
                                 (unsigned long long)pid);
    if (written < 0 || (size_t)written >= sizeof(directory_path)) return false;
    DIR *directory = opendir(directory_path);
    if (!directory) return false;

    char seen[LSM_PROCESS_GPU_MAX_CLIENTS][192];
    size_t seen_count = 0U;
    struct dirent *entry = NULL;
    while ((entry = readdir(directory))) {
        uint64_t descriptor = 0U;
        if (!lsm_parse_u64_range(entry->d_name, 10U, 0U,
                                 (uint64_t)INT_MAX, &descriptor))
            continue;
        char path[640];
        if (!lsm_join_path(path, sizeof(path), directory_path, entry->d_name))
            continue;
        LsmDrmClient client;
        memset(&client, 0, sizeof(client));
        if (!read_client_file(path, &client) ||
            client_seen(seen, seen_count, client.key))
            continue;
        /* Every accumulated client must also have a retained deduplication
         * key. Stop at the bounded working set instead of accepting clients
         * that subsequent descriptors could count again. */
        if (seen_count >= LSM_PROCESS_GPU_MAX_CLIENTS) break;
        lsm_copy_string(seen[seen_count], sizeof(seen[seen_count]),
                        client.key);
        seen_count++;
        for (size_t index = 0U; index < client.engine_count; index++) {
            if (!client.engines[index].time_available) continue;
            char engine_key[256];
            (void)snprintf(engine_key, sizeof(engine_key), "%.120s:%.120s",
                           client.device_key, client.engines[index].name);
            add_engine(snapshot->engines, &snapshot->engine_count,
                       engine_key, client.engines[index].time_ns,
                       client.engines[index].capacity, true);
        }
        for (size_t index = 0U; index < client.region_count; index++)
            snapshot->memory_bytes = lsm_u64_add_saturating(
                snapshot->memory_bytes, client.regions[index].bytes);
        snapshot->memory_available = snapshot->memory_available ||
                                     client.memory_available;
    }
    closedir(directory);
    snapshot->engine_counters_available = snapshot->engine_count > 0U;
    return snapshot->engine_counters_available || snapshot->memory_available;
}

static const LsmProcessGpuEngine *find_engine(
    const LsmProcessGpuSnapshot *snapshot, const char *name)
{
    if (!snapshot || !name) return NULL;
    for (size_t index = 0U; index < snapshot->engine_count; index++)
        if (strcmp(snapshot->engines[index].name, name) == 0)
            return &snapshot->engines[index];
    return NULL;
}

void lsm_process_gpu_normalise(LsmProcessGpuSnapshot *current,
                               const LsmProcessGpuSnapshot *previous)
{
    if (!current || !previous) return;
    for (size_t index = 0U; index < current->engine_count; index++) {
        LsmProcessGpuEngine *engine = &current->engines[index];
        const LsmProcessGpuEngine *old = find_engine(previous, engine->name);
        if (old && engine->time_ns < old->time_ns)
            engine->time_ns = old->time_ns;
    }
}

bool lsm_process_gpu_calculate_engine(
    const LsmProcessGpuSnapshot *current,
    const LsmProcessGpuSnapshot *previous,
    double elapsed_seconds, double *percent,
    char *engine, size_t engine_size)
{
    if (percent) *percent = 0.0;
    if (engine && engine_size > 0U) engine[0] = '\0';
    if (!current || !previous || !percent || !isfinite(elapsed_seconds) ||
        elapsed_seconds <= 0.0 || !current->engine_counters_available ||
        !previous->engine_counters_available)
        return false;

    long double peak_percent = 0.0L;
    const char *peak_engine = NULL;
    bool matched = false;
    const long double interval_ns =
        (long double)elapsed_seconds * 1000000000.0L;
    for (size_t index = 0U; index < current->engine_count; index++) {
        const LsmProcessGpuEngine *old = find_engine(
            previous, current->engines[index].name);
        if (!old || current->engines[index].time_ns < old->time_ns) continue;
        const uint64_t delta = current->engines[index].time_ns - old->time_ns;
        const unsigned capacity = current->engines[index].capacity > 0U
            ? current->engines[index].capacity : 1U;
        const long double value = (long double)delta * 100.0L /
                                  (interval_ns * capacity);
        if (!matched || value > peak_percent) {
            peak_percent = value;
            peak_engine = current->engines[index].name;
        }
        matched = true;
    }
    if (!matched) return false;
    long double value = peak_percent;
    if (!isfinite(value) || value < 0.0L) value = 0.0L;
    if (value > 100.0L) value = 100.0L;
    *percent = (double)value;
    if (engine && engine_size > 0U && peak_engine) {
        const char *display_name = strrchr(peak_engine, ':');
        display_name = display_name ? display_name + 1U : peak_engine;
        lsm_copy_string(engine, engine_size, display_name);
    }
    return true;
}
