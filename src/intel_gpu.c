// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file intel_gpu.c
 * @brief Direct Intel graphics PMU, GT-frequency and hwmon collection.
 *
 * i915 publishes cumulative engine-busy and frequency events through Linux's
 * perf-event ABI.  Event names and config values are discovered from the PMU's
 * kernel-owned event directory once, then retained perf descriptors are read
 * without launching intel_gpu_top or linking an Intel userspace library.
 *
 * Xe currently has less uniform unprivileged PMU exposure.  The same module
 * still supplies retained GT-frequency and hwmon telemetry and degrades each
 * missing counter independently.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */

#include "intel_gpu.h"

#include "common.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/syscall.h>
#include <unistd.h>

#define LSM_INTEL_MAX_COUNTERS 64U
#define LSM_INTEL_EVENT_NAME 96U

typedef enum {
    LSM_INTEL_ENGINE_RENDER = 0,
    LSM_INTEL_ENGINE_COMPUTE,
    LSM_INTEL_ENGINE_VIDEO,
    LSM_INTEL_ENGINE_VIDEO_ENHANCE,
    LSM_INTEL_ENGINE_COPY,
    LSM_INTEL_ENGINE_OTHER
} LsmIntelEngineClass;

typedef struct {
    char name[LSM_INTEL_EVENT_NAME];
    int fd;
    char *mock_value_path;
    uint64_t previous_value;
    uint64_t previous_enabled;
    uint64_t previous_running;
    bool initialised;
    LsmIntelEngineClass engine_class;
} LsmIntelCounter;

struct LsmIntelGpuBackend {
    char identity[LSM_PATH_LEN];
    char pmu_root[LSM_PATH_LEN];
    unsigned int pmu_type;
    int pmu_cpu;
    bool mock_mode;
    bool shared_system_memory;
    bool perf_access_denied;
    int perf_open_error;
    LsmIntelCounter engines[LSM_INTEL_MAX_COUNTERS];
    size_t engine_count;
    LsmIntelCounter graphics_frequency;
    LsmIntelCounter media_frequency;
    char *graphics_frequency_path;
    char *media_frequency_path;
    char *temperature_path;
    char *power_average_path;
    char *energy_path;
    char *energy_range_path;
    uint64_t previous_energy_uj;
    bool energy_initialised;
};

typedef struct {
    uint64_t value;
    uint64_t time_enabled;
    uint64_t time_running;
} LsmPerfRead;

static char *existing_path(const char *base, const char *suffix)
{
    char path[LSM_PATH_LEN];
    if (!lsm_join_path(path, sizeof(path), base, suffix) ||
        access(path, R_OK) != 0)
        return NULL;
    return strdup(path);
}

static char *first_existing_path(const char *base,
                                 const char *const *suffixes,
                                 size_t suffix_count)
{
    for (size_t index = 0U; index < suffix_count; index++) {
        char *path = existing_path(base, suffixes[index]);
        if (path) return path;
    }
    return NULL;
}

static int perf_event_open_counter(unsigned int type, uint64_t config, int cpu)
{
#if defined(__NR_perf_event_open)
    struct perf_event_attr attributes;
    memset(&attributes, 0, sizeof(attributes));
    attributes.type = type;
    attributes.size = sizeof(attributes);
    attributes.config = config;
    attributes.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
                             PERF_FORMAT_TOTAL_TIME_RUNNING;
    attributes.disabled = 0U;
    attributes.exclude_hv = 1U;
    return (int)syscall(__NR_perf_event_open, &attributes, -1, cpu, -1, 0UL);
#else
    (void)type;
    (void)config;
    (void)cpu;
    errno = ENOSYS;
    return -1;
#endif
}

static bool read_pmu_config(const char *path, uint64_t *config)
{
    char text[128];
    if (!lsm_read_text_file(path, text, sizeof(text))) return false;
    char *assignment = strstr(text, "config=");
    if (!assignment) return false;
    assignment += strlen("config=");
    return lsm_parse_u64(assignment, 0U, config);
}

static int first_cpu_in_mask(const char *text)
{
    if (!text) return 0;
    while (*text && isspace((unsigned char)*text)) text++;
    errno = 0;
    char *end = NULL;
    const long cpu = strtol(text, &end, 10);
    if (errno != 0 || end == text || cpu < 0 || cpu > INT32_MAX) return 0;
    return (int)cpu;
}

static bool pmu_root_matches_identity(const char *entry,
                                      const LsmGpuInfo *gpu)
{
    if (!entry || !gpu) return false;
    if (lsm_string_equal(entry, "i915") || lsm_string_equal(entry, "xe"))
        return true;
    if (!gpu->platform_identity[0]) return false;

    const char *component = strrchr(gpu->platform_identity, '/');
    component = component ? component + 1 : gpu->platform_identity;
    char normalised[64];
    lsm_copy_string(normalised, sizeof(normalised), component);
    for (char *cursor = normalised; *cursor; cursor++)
        if (*cursor == ':') *cursor = '_';
    return strstr(entry, normalised) != NULL;
}

static bool discover_pmu_root(const LsmGpuInfo *gpu,
                              char *destination, size_t destination_size)
{
    const char *override = getenv("LSM_INTEL_GPU_PMU_ROOT");
    if (override && *override) {
        if (strlen(override) >= destination_size) return false;
        lsm_copy_string(destination, destination_size, override);
        return true;
    }

    static const char root[] = "/sys/bus/event_source/devices";
    DIR *directory = opendir(root);
    if (!directory) return false;
    bool found = false;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (!lsm_string_starts_with(entry->d_name, "i915") &&
            !lsm_string_starts_with(entry->d_name, "xe"))
            continue;
        if (!pmu_root_matches_identity(entry->d_name, gpu)) continue;
        const int written = snprintf(destination, destination_size, "%s/%s",
                                     root, entry->d_name);
        found = written >= 0 && (size_t)written < destination_size;
        if (found) break;
    }
    closedir(directory);
    return found;
}

static LsmIntelEngineClass classify_engine(const char *event_name)
{
    if (!event_name) return LSM_INTEL_ENGINE_OTHER;
    if (lsm_string_starts_with(event_name, "rcs"))
        return LSM_INTEL_ENGINE_RENDER;
    if (lsm_string_starts_with(event_name, "ccs"))
        return LSM_INTEL_ENGINE_COMPUTE;
    if (lsm_string_starts_with(event_name, "vecs"))
        return LSM_INTEL_ENGINE_VIDEO_ENHANCE;
    if (lsm_string_starts_with(event_name, "vcs"))
        return LSM_INTEL_ENGINE_VIDEO;
    if (lsm_string_starts_with(event_name, "bcs"))
        return LSM_INTEL_ENGINE_COPY;
    return LSM_INTEL_ENGINE_OTHER;
}

static void counter_reset(LsmIntelCounter *counter)
{
    if (!counter) return;
    if (counter->fd >= 0) close(counter->fd);
    free(counter->mock_value_path);
    memset(counter, 0, sizeof(*counter));
    counter->fd = -1;
}

static bool initialise_counter(LsmIntelGpuBackend *backend,
                               LsmIntelCounter *counter,
                               const char *event_name, uint64_t config,
                               LsmIntelEngineClass engine_class)
{
    if (!backend || !counter || !event_name) return false;
    memset(counter, 0, sizeof(*counter));
    counter->fd = -1;
    counter->engine_class = engine_class;
    lsm_copy_string(counter->name, sizeof(counter->name), event_name);

    if (backend->mock_mode) {
        char suffix[LSM_INTEL_EVENT_NAME + 32U];
        const int written = snprintf(suffix, sizeof(suffix),
                                     "/mock-values/%s", event_name);
        if (written < 0 || (size_t)written >= sizeof(suffix)) return false;
        counter->mock_value_path = existing_path(backend->pmu_root, suffix);
        return counter->mock_value_path != NULL;
    }

    counter->fd = perf_event_open_counter(backend->pmu_type, config,
                                          backend->pmu_cpu);
    if (counter->fd < 0) {
        backend->perf_open_error = errno;
        if (errno == EACCES || errno == EPERM)
            backend->perf_access_denied = true;
    }
    return counter->fd >= 0;
}

static bool read_counter_sample(const LsmIntelCounter *counter,
                                LsmPerfRead *sample)
{
    if (!counter || !sample) return false;
    memset(sample, 0, sizeof(*sample));
    if (counter->mock_value_path) {
        if (!lsm_read_u64_file(counter->mock_value_path, &sample->value))
            return false;
        sample->time_enabled = 1U;
        sample->time_running = 1U;
        return true;
    }
    if (counter->fd < 0) return false;
    const ssize_t count = read(counter->fd, sample, sizeof(*sample));
    return count == (ssize_t)sizeof(*sample);
}

static bool counter_delta(LsmIntelCounter *counter, double elapsed,
                          double divisor, double scale, double *result)
{
    if (!counter || !result || elapsed <= 0.0) return false;
    LsmPerfRead sample;
    if (!read_counter_sample(counter, &sample)) return false;

    if (!counter->initialised || sample.value < counter->previous_value ||
        sample.time_enabled < counter->previous_enabled ||
        sample.time_running < counter->previous_running) {
        counter->previous_value = sample.value;
        counter->previous_enabled = sample.time_enabled;
        counter->previous_running = sample.time_running;
        counter->initialised = true;
        *result = 0.0;
        return true;
    }

    long double delta = (long double)(sample.value - counter->previous_value);
    const uint64_t enabled = sample.time_enabled - counter->previous_enabled;
    const uint64_t running = sample.time_running - counter->previous_running;
    if (!counter->mock_value_path && running > 0U && enabled > running)
        delta *= (long double)enabled / (long double)running;

    counter->previous_value = sample.value;
    counter->previous_enabled = sample.time_enabled;
    counter->previous_running = sample.time_running;
    *result = (double)(delta / divisor / (long double)elapsed * scale);
    return isfinite(*result);
}

static bool event_path(char *destination, size_t size, const char *pmu_root,
                       const char *event_name)
{
    const int written = snprintf(destination, size, "%s/events/%s",
                                 pmu_root, event_name);
    return written >= 0 && (size_t)written < size;
}


static void discover_events(LsmIntelGpuBackend *backend)
{
    char events_path[LSM_PATH_LEN];
    if (!lsm_join_path(events_path, sizeof(events_path), backend->pmu_root,
                       "/events"))
        return;
    DIR *directory = opendir(events_path);
    if (!directory) return;

    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.' ||
            lsm_string_ends_with(entry->d_name, ".unit"))
            continue;
        char path[LSM_PATH_LEN];
        uint64_t config = 0U;
        if (!event_path(path, sizeof(path), backend->pmu_root, entry->d_name) ||
            !read_pmu_config(path, &config))
            continue;

        if (lsm_string_ends_with(entry->d_name, "-busy") &&
            backend->engine_count < LSM_INTEL_MAX_COUNTERS) {
            LsmIntelCounter *counter =
                &backend->engines[backend->engine_count];
            if (initialise_counter(backend, counter, entry->d_name, config,
                                   classify_engine(entry->d_name)))
                backend->engine_count++;
            else
                counter_reset(counter);
            continue;
        }

        const bool graphics_frequency =
            strcmp(entry->d_name, "actual-frequency") == 0 ||
            strcmp(entry->d_name, "actual-frequency-gt0") == 0;
        const bool media_frequency =
            strcmp(entry->d_name, "actual-frequency-gt1") == 0;
        if (graphics_frequency && backend->graphics_frequency.fd < 0 &&
            !backend->graphics_frequency.mock_value_path) {
            if (!initialise_counter(backend, &backend->graphics_frequency,
                                    entry->d_name, config,
                                    LSM_INTEL_ENGINE_OTHER))
                counter_reset(&backend->graphics_frequency);
        } else if (media_frequency && backend->media_frequency.fd < 0 &&
                   !backend->media_frequency.mock_value_path) {
            if (!initialise_counter(backend, &backend->media_frequency,
                                    entry->d_name, config,
                                    LSM_INTEL_ENGINE_OTHER))
                counter_reset(&backend->media_frequency);
        }
    }
    closedir(directory);
}

static void discover_driver_paths(LsmIntelGpuBackend *backend,
                                  const LsmGpuInfo *gpu)
{
    static const char *const graphics_frequency[] = {
        "/gt/gt0/rps_act_freq_mhz",
        "/gt/gt0/rps_cur_freq_mhz",
        "/gt_cur_freq_mhz"
    };
    static const char *const media_frequency[] = {
        "/gt/gt1/rps_act_freq_mhz",
        "/gt/gt1/rps_cur_freq_mhz"
    };
    const char *base = gpu->platform_identity;
    if (!base[0]) return;

    backend->graphics_frequency_path = first_existing_path(
        base, graphics_frequency, LSM_ARRAY_LENGTH(graphics_frequency));
    backend->media_frequency_path = first_existing_path(
        base, media_frequency, LSM_ARRAY_LENGTH(media_frequency));

    char vram_path[LSM_PATH_LEN];
    uint64_t vram_total = 0U;
    backend->shared_system_memory =
        !lsm_join_path(vram_path, sizeof(vram_path), base,
                       "/mem_info_vram_total") ||
        !lsm_read_u64_file(vram_path, &vram_total) || vram_total == 0U;

    char hwmon_root[LSM_PATH_LEN];
    if (!lsm_join_path(hwmon_root, sizeof(hwmon_root), base, "/hwmon")) return;
    DIR *directory = opendir(hwmon_root);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
        char node[LSM_PATH_LEN];
        const int written = snprintf(node, sizeof(node), "%s/%s",
                                     hwmon_root, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(node)) continue;
        if (!backend->temperature_path)
            backend->temperature_path = existing_path(node, "/temp1_input");
        if (!backend->power_average_path)
            backend->power_average_path = existing_path(node, "/power1_average");
        if (!backend->energy_path)
            backend->energy_path = existing_path(node, "/energy1_input");
        if (!backend->energy_range_path)
            backend->energy_range_path =
                existing_path(node, "/max_energy_range_uj");
    }
    closedir(directory);
}

bool lsm_intel_gpu_driver_supported(const char *driver)
{
    return driver && (strcasecmp(driver, "i915") == 0 ||
                      strcasecmp(driver, "xe") == 0);
}

LsmIntelGpuBackend *lsm_intel_gpu_create(const LsmGpuInfo *gpu)
{
    if (!gpu || !lsm_intel_gpu_driver_supported(gpu->driver)) return NULL;
    LsmIntelGpuBackend *backend = calloc(1U, sizeof(*backend));
    if (!backend) return NULL;
    backend->pmu_cpu = 0;
    backend->graphics_frequency.fd = -1;
    backend->media_frequency.fd = -1;
    for (size_t index = 0U; index < LSM_INTEL_MAX_COUNTERS; index++)
        backend->engines[index].fd = -1;
    lsm_copy_string(backend->identity, sizeof(backend->identity),
                    gpu->platform_identity[0] ? gpu->platform_identity : gpu->display_identifier);

    if (discover_pmu_root(gpu, backend->pmu_root, sizeof(backend->pmu_root))) {
        const char *pmu_override = getenv("LSM_INTEL_GPU_PMU_ROOT");
        char mock_values[LSM_PATH_LEN];
        backend->mock_mode = pmu_override && *pmu_override &&
            lsm_join_path(mock_values, sizeof(mock_values), backend->pmu_root,
                          "/mock-values") && access(mock_values, R_OK) == 0;
        char path[LSM_PATH_LEN];
        uint64_t value = 0U;
        if (lsm_join_path(path, sizeof(path), backend->pmu_root, "/type") &&
            lsm_read_u64_file(path, &value) && value <= UINT32_MAX)
            backend->pmu_type = (unsigned int)value;
        if (lsm_join_path(path, sizeof(path), backend->pmu_root, "/cpumask")) {
            char mask[128];
            if (lsm_read_text_file(path, mask, sizeof(mask)))
                backend->pmu_cpu = first_cpu_in_mask(mask);
        }
        discover_events(backend);
    }
    discover_driver_paths(backend, gpu);
    return backend;
}

static void update_frequency(LsmIntelCounter *counter, const char *fallback_path,
                             double elapsed, double *frequency,
                             bool *available)
{
    double value = 0.0;
    if (counter && (counter->fd >= 0 || counter->mock_value_path) &&
        counter_delta(counter, elapsed, 1.0L, 1.0, &value)) {
        *frequency = fmax(0.0, value);
        *available = true;
        return;
    }
    if (fallback_path && lsm_read_double_file(fallback_path, &value) &&
        value >= 0.0) {
        *frequency = value;
        *available = true;
    }
}

static bool update_power(LsmIntelGpuBackend *backend, LsmGpuInfo *gpu,
                         double elapsed)
{
    uint64_t value = 0U;
    if (backend->power_average_path &&
        lsm_read_u64_file(backend->power_average_path, &value)) {
        gpu->power_watts = (double)value / 1000000.0;
        gpu->power_available = true;
        return true;
    }
    if (!backend->energy_path || elapsed <= 0.0 ||
        !lsm_read_u64_file(backend->energy_path, &value))
        return false;

    if (!backend->energy_initialised) {
        backend->previous_energy_uj = value;
        backend->energy_initialised = true;
        return true;
    }

    uint64_t delta = 0U;
    if (value >= backend->previous_energy_uj) {
        delta = value - backend->previous_energy_uj;
    } else if (backend->energy_range_path) {
        uint64_t range = 0U;
        if (lsm_read_u64_file(backend->energy_range_path, &range) &&
            range > backend->previous_energy_uj)
            delta = (range - backend->previous_energy_uj) + value;
    }
    backend->previous_energy_uj = value;
    if (delta == 0U) return true;
    gpu->power_watts = (double)delta / 1000000.0 / elapsed;
    gpu->power_available = isfinite(gpu->power_watts) &&
                           gpu->power_watts >= 0.0;
    return true;
}

bool lsm_intel_gpu_refresh(LsmIntelGpuBackend *backend, LsmGpuInfo *gpu,
                           double elapsed)
{
    if (!backend || !gpu || elapsed <= 0.0) return false;
    double class_max[LSM_INTEL_ENGINE_OTHER + 1U] = {0};
    bool class_available[LSM_INTEL_ENGINE_OTHER + 1U] = {false};
    double overall = 0.0;
    bool engine_sampled = false;
    bool any = false;

    for (size_t index = 0U; index < backend->engine_count; index++) {
        double percentage = 0.0;
        if (!counter_delta(&backend->engines[index], elapsed, 1000000000.0L,
                           100.0, &percentage))
            continue;
        percentage = lsm_clamp_double(percentage, 0.0, 100.0);
        const LsmIntelEngineClass engine_class =
            backend->engines[index].engine_class;
        class_available[engine_class] = true;
        class_max[engine_class] = fmax(class_max[engine_class], percentage);
        overall = fmax(overall, percentage);
        engine_sampled = true;
    }

    if (engine_sampled) {
        gpu->utilization_percent = overall;
        gpu->utilization_available = true;
        gpu->render_percent = class_max[LSM_INTEL_ENGINE_RENDER];
        gpu->render_available = class_available[LSM_INTEL_ENGINE_RENDER];
        gpu->compute_percent = class_max[LSM_INTEL_ENGINE_COMPUTE];
        gpu->compute_available = class_available[LSM_INTEL_ENGINE_COMPUTE];
        gpu->video_percent = class_max[LSM_INTEL_ENGINE_VIDEO];
        gpu->video_available = class_available[LSM_INTEL_ENGINE_VIDEO];
        gpu->video_enhance_percent =
            class_max[LSM_INTEL_ENGINE_VIDEO_ENHANCE];
        gpu->video_enhance_available =
            class_available[LSM_INTEL_ENGINE_VIDEO_ENHANCE];
        gpu->copy_percent = class_max[LSM_INTEL_ENGINE_COPY];
        gpu->copy_available = class_available[LSM_INTEL_ENGINE_COPY];
        any = true;
    }

    update_frequency(&backend->graphics_frequency,
                     backend->graphics_frequency_path, elapsed,
                     &gpu->core_clock_mhz, &gpu->core_clock_available);
    update_frequency(&backend->media_frequency,
                     backend->media_frequency_path, elapsed,
                     &gpu->memory_clock_mhz, &gpu->memory_clock_available);
    any = any || gpu->core_clock_available || gpu->memory_clock_available;

    uint64_t value = 0U;
    if (backend->temperature_path &&
        lsm_read_u64_file(backend->temperature_path, &value)) {
        gpu->temperature_c = (double)value / 1000.0;
        gpu->temperature_available = true;
        any = true;
    }
    any = update_power(backend, gpu, elapsed) || any;

    gpu->engine_metrics_capable = true;
    gpu->shared_system_memory = backend->shared_system_memory;
    gpu->integrated_cooling = backend->shared_system_memory;
    if (any) {
        lsm_copy_string(gpu->metrics_source, sizeof(gpu->metrics_source),
                        engine_sampled ? "Native Intel PMU"
                                       : "Native Intel driver telemetry");
        gpu->supported_metrics = true;
    } else if (backend->perf_access_denied) {
        lsm_copy_string(gpu->metrics_source, sizeof(gpu->metrics_source),
                        "Intel PMU restricted by system policy");
    }
    return any;
}

void lsm_intel_gpu_destroy(LsmIntelGpuBackend *backend)
{
    if (!backend) return;
    for (size_t index = 0U; index < backend->engine_count; index++)
        counter_reset(&backend->engines[index]);
    counter_reset(&backend->graphics_frequency);
    counter_reset(&backend->media_frequency);
    free(backend->graphics_frequency_path);
    free(backend->media_frequency_path);
    free(backend->temperature_path);
    free(backend->power_average_path);
    free(backend->energy_path);
    free(backend->energy_range_path);
    free(backend);
}
