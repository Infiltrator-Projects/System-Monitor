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
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "process_gpu.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

static bool numeric_name(const char *name)
{
    if (!name || !*name) return false;
    for (const char *cursor = name; *cursor; cursor++)
        if (!isdigit((unsigned char)*cursor)) return false;
    return true;
}

static uint64_t add_saturating(uint64_t left, uint64_t right)
{
    return UINT64_MAX - left < right ? UINT64_MAX : left + right;
}

static bool parse_u64(const char *text, uint64_t *value)
{
    if (!text || !value) return false;
    while (*text && isspace((unsigned char)*text)) text++;
    if (!isdigit((unsigned char)*text)) return false;
    errno = 0;
    char *end = NULL;
    const unsigned long long parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text) return false;
    *value = (uint64_t)parsed;
    return true;
}

static bool parse_bytes(const char *text, uint64_t *bytes)
{
    if (!text || !bytes) return false;
    while (*text && isspace((unsigned char)*text)) text++;
    if (!isdigit((unsigned char)*text)) return false;
    errno = 0;
    char *end = NULL;
    const unsigned long long parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text) return false;
    while (*end && isspace((unsigned char)*end)) end++;

    uint64_t multiplier = 1U;
    switch (toupper((unsigned char)*end)) {
    case 'K': multiplier = 1024U; break;
    case 'M': multiplier = 1024U * 1024U; break;
    case 'G': multiplier = 1024U * 1024U * 1024U; break;
    case '\0':
    case 'B': break;
    default: return false;
    }
    *bytes = (uint64_t)parsed > UINT64_MAX / multiplier
        ? UINT64_MAX : (uint64_t)parsed * multiplier;
    return true;
}

static char *field_value(char *line)
{
    char *separator = strchr(line, ':');
    if (!separator) return NULL;
    char *value = separator + 1;
    while (*value && isspace((unsigned char)*value)) value++;
    char *end = value + strlen(value);
    while (end > value && isspace((unsigned char)end[-1])) *--end = '\0';
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
            engines[index].time_ns = add_saturating(
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
    (void)snprintf(engine->name, sizeof(engine->name), "%s", name);
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
    (void)snprintf(region->name, sizeof(region->name), "%s", name);
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
        if (strncmp(line, "drm-driver:", 11U) == 0) {
            char *value = field_value(line);
            if (value) (void)snprintf(driver, sizeof(driver), "%s", value);
        } else if (strncmp(line, "drm-pdev:", 9U) == 0) {
            char *value = field_value(line);
            if (value) (void)snprintf(device, sizeof(device), "%s", value);
        } else if (strncmp(line, "drm-minor:", 10U) == 0 && !device[0]) {
            char *value = field_value(line);
            if (value) (void)snprintf(device, sizeof(device), "minor-%s", value);
        } else if (strncmp(line, "drm-client-id:", 14U) == 0) {
            char *value = field_value(line);
            have_client_id = value && parse_u64(value, &client_id);
        } else if (strncmp(line, "drm-engine-capacity-", 20U) == 0) {
            char *separator = strchr(line, ':');
            uint64_t capacity = 0U;
            if (!separator || !parse_u64(separator + 1, &capacity) ||
                capacity == 0U)
                continue;
            *separator = '\0';
            add_engine(client->engines, &client->engine_count,
                       line + 20U, 0U,
                       capacity > (uint64_t)UINT_MAX
                           ? UINT_MAX : (unsigned)capacity,
                       false);
        } else if (strncmp(line, "drm-engine-", 11U) == 0) {
            char *separator = strchr(line, ':');
            uint64_t time_ns = 0U;
            if (!separator || !parse_u64(separator + 1, &time_ns)) continue;
            *separator = '\0';
            add_engine(client->engines, &client->engine_count,
                       line + 11U, time_ns, 1U, true);
            recognised = true;
        } else if (strncmp(line, "drm-resident-", 13U) == 0 ||
                   strncmp(line, "drm-memory-", 11U) == 0) {
            const bool resident = strncmp(line, "drm-resident-", 13U) == 0;
            const size_t prefix = resident ? 13U : 11U;
            char *separator = strchr(line, ':');
            char *value = field_value(line);
            uint64_t bytes = 0U;
            if (!separator || !value || !parse_bytes(value, &bytes)) continue;
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
                       strrchr(path, '/') ? strrchr(path, '/') + 1 : path);
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
        if (!numeric_name(entry->d_name)) continue;
        char path[640];
        const int path_written = snprintf(path, sizeof(path), "%s/%s",
                                          directory_path, entry->d_name);
        if (path_written < 0 || (size_t)path_written >= sizeof(path)) continue;
        LsmDrmClient client;
        memset(&client, 0, sizeof(client));
        if (!read_client_file(path, &client) ||
            client_seen(seen, seen_count, client.key))
            continue;
        /* Every accumulated client must also have a retained deduplication
         * key. Stop at the bounded working set instead of accepting clients
         * that subsequent descriptors could count again. */
        if (seen_count >= LSM_PROCESS_GPU_MAX_CLIENTS) break;
        (void)snprintf(seen[seen_count], sizeof(seen[seen_count]),
                       "%s", client.key);
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
            snapshot->memory_bytes = add_saturating(
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
        (void)snprintf(engine, engine_size, "%s", display_name);
    }
    return true;
}
