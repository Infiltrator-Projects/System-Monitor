// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_hardware.c
 * @brief GPU/NPU collection and dynamic hardware topology coordination.
 *
 * Optional driver metrics are capability-detected. Battery collection is owned
 * by monitor_battery.c; this module coordinates GPU/NPU adapters and generic
 * sensor fallbacks. Discovery and sampling are separate operations: expensive
 * path resolution occurs only on topology changes, while the live cadence reads
 * cached attributes and retained cumulative counters. Vendor backends update
 * independent availability flags so one failed metric or adapter cannot poison
 * neighbouring data.
 *
 * Telemetry caches are owned by the active Linux monitor backend rather than by
 * process-global mutable storage. A monitor therefore destroys exactly the
 * resources it created, while the public snapshot remains plain C data.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_linux_internal.h"
#include "common.h"
#include "intel_gpu.h"
#include "hardware_topology.h"
#include "npu_telemetry.h"
#include "nvml.h"
#include "pci_names.h"
#include "system_sources.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define LSM_MAX_GPU_ENGINE_COUNTERS 32U

typedef struct {
    char identity[LSM_PATH_LEN];
    char *gpu_busy;
    char *vram_used;
    char *vram_total;
    char *memory_busy;
    char *core_clock;
    char *core_clock_dpm;
    char *memory_clock;
    char *memory_clock_dpm;
    char *temperature;
    char *power;
    char *pwm;
    char *pwm_max;
    LsmIntelGpuBackend *intel_backend;
    char *engine_busy[LSM_MAX_GPU_ENGINE_COUNTERS];
    size_t engine_count;
} LsmGpuTelemetryCache;

typedef struct {
    char identity[LSM_PATH_LEN];
    LsmNpuTelemetry *backend;
} LsmNpuTelemetryCache;

struct LsmLinuxHardwareState {
    LsmGpuTelemetryCache gpu_telemetry[LSM_MAX_GPUS];
    size_t gpu_telemetry_count;
    LsmNpuTelemetryCache npu_telemetry[LSM_MAX_NPUS];
    size_t npu_telemetry_count;
};

static LsmLinuxHardwareState *hardware_state(LsmMonitor *monitor)
{
    LsmLinuxMonitorBackendState *backend = monitor_backend_state(monitor);
    return backend ? backend->hardware_state : NULL;
}

/* Telemetry caches retain resolved driver attribute paths between samples.
 * They are rebuilt only when accelerator membership changes. */
static char *existing_metric_path(const char *base, const char *suffix)
{
    char path[LSM_PATH_LEN];
    if (!base || !*base || !suffix ||
        !lsm_join_path(path, sizeof(path), base, suffix) ||
        access(path, F_OK) != 0)
        return NULL;
    return strdup(path);
}

static void destroy_gpu_telemetry(LsmGpuTelemetryCache *cache)
{
    if (!cache) return;
    free(cache->gpu_busy);
    free(cache->vram_used);
    free(cache->vram_total);
    free(cache->memory_busy);
    free(cache->core_clock);
    free(cache->core_clock_dpm);
    free(cache->memory_clock);
    free(cache->memory_clock_dpm);
    free(cache->temperature);
    free(cache->power);
    free(cache->pwm);
    free(cache->pwm_max);
    lsm_intel_gpu_destroy(cache->intel_backend);
    for (size_t index = 0U; index < cache->engine_count; index++)
        free(cache->engine_busy[index]);
    memset(cache, 0, sizeof(*cache));
}

static void destroy_npu_telemetry(LsmNpuTelemetryCache *cache)
{
    if (!cache) return;
    lsm_npu_telemetry_destroy(cache->backend);
    memset(cache, 0, sizeof(*cache));
}

static const char *gpu_stable_identity(const LsmGpuInfo *gpu)
{
    return gpu->platform_identity[0] ? gpu->platform_identity : gpu->display_identifier;
}

static const char *npu_stable_identity(const LsmNpuInfo *npu)
{
    return npu->platform_identity[0] ? npu->platform_identity : npu->display_identifier;
}

static LsmLinuxGpuState *find_gpu_state(LsmMonitor *monitor,
                                        const LsmGpuInfo *gpu)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state || !gpu) return NULL;
    const char *identity = gpu_stable_identity(gpu);
    for (size_t index = 0U; index < state->gpu_count; index++)
        if (strcmp(state->gpus[index].platform_identity, identity) == 0)
            return &state->gpus[index];
    return NULL;
}

static void reconcile_gpu_states(LsmMonitor *monitor)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return;
    LsmLinuxGpuState next[LSM_MAX_GPUS] = {0};
    for (size_t index = 0U; index < monitor->gpu_count && index < LSM_MAX_GPUS;
         index++) {
        LsmGpuInfo *gpu = &monitor->gpus[index];
        const char *identity = gpu_stable_identity(gpu);
        LsmLinuxGpuState *old = find_gpu_state(monitor, gpu);
        if (old) next[index] = *old;
        lsm_copy_string(next[index].platform_identity,
                        sizeof(next[index].platform_identity), identity);
        next[index].intel_native_backend =
            lsm_intel_gpu_driver_supported(gpu->driver);
        gpu->engine_metrics_capable = next[index].intel_native_backend;
    }
    memcpy(state->gpus, next, sizeof(next));
    state->gpu_count = monitor->gpu_count < LSM_MAX_GPUS
        ? monitor->gpu_count : LSM_MAX_GPUS;
}

static void build_gpu_telemetry_cache(const LsmGpuInfo *gpu,
                                      LsmGpuTelemetryCache *cache)
{
    if (!gpu || !cache) return;
    memset(cache, 0, sizeof(*cache));
    lsm_copy_string(cache->identity, sizeof(cache->identity),
                    gpu_stable_identity(gpu));

    char fallback_base[LSM_PATH_LEN];
    (void)snprintf(fallback_base, sizeof(fallback_base),
                   "/sys/class/drm/%.63s/device", gpu->display_identifier);
    const char *base = gpu->platform_identity[0] ? gpu->platform_identity : fallback_base;
    cache->gpu_busy = existing_metric_path(base, "/gpu_busy_percent");
    cache->vram_used = existing_metric_path(base, "/mem_info_vram_used");
    cache->vram_total = existing_metric_path(base, "/mem_info_vram_total");
    cache->memory_busy = existing_metric_path(base, "/mem_busy_percent");
    cache->core_clock = existing_metric_path(base, "/gt_cur_freq_mhz");
    cache->core_clock_dpm = existing_metric_path(base, "/pp_dpm_sclk");
    cache->memory_clock = existing_metric_path(base, "/mem_cur_freq_mhz");
    cache->memory_clock_dpm = existing_metric_path(base, "/pp_dpm_mclk");
    cache->intel_backend = lsm_intel_gpu_create(gpu);

    char engine_root[LSM_PATH_LEN];
    if (lsm_join_path(engine_root, sizeof(engine_root), base, "/engine")) {
        DIR *directory = opendir(engine_root);
        if (directory) {
            struct dirent *entry;
            while ((entry = readdir(directory)) &&
                   cache->engine_count < LSM_MAX_GPU_ENGINE_COUNTERS) {
                if (entry->d_name[0] == '.') continue;
                char suffix[300];
                (void)snprintf(suffix, sizeof(suffix), "/%.240s/busy",
                               entry->d_name);
                char *path = existing_metric_path(engine_root, suffix);
                if (path) cache->engine_busy[cache->engine_count++] = path;
            }
            closedir(directory);
        }
    }

    char hwmon_root[LSM_PATH_LEN];
    if (!lsm_join_path(hwmon_root, sizeof(hwmon_root), base, "/hwmon")) return;
    DIR *directory = opendir(hwmon_root);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
        char node[LSM_PATH_LEN];
        char suffix[300];
        (void)snprintf(suffix, sizeof(suffix), "/%.240s", entry->d_name);
        if (!lsm_join_path(node, sizeof(node), hwmon_root, suffix)) continue;
        if (!cache->temperature)
            cache->temperature = existing_metric_path(node, "/temp1_input");
        if (!cache->power)
            cache->power = existing_metric_path(node, "/power1_average");
        if (!cache->pwm) cache->pwm = existing_metric_path(node, "/pwm1");
        if (!cache->pwm_max)
            cache->pwm_max = existing_metric_path(node, "/pwm1_max");
    }
    closedir(directory);
}

static void build_npu_telemetry_cache(const LsmNpuInfo *npu,
                                      LsmNpuTelemetryCache *cache)
{
    if (!npu || !cache) return;
    memset(cache, 0, sizeof(*cache));
    lsm_copy_string(cache->identity, sizeof(cache->identity),
                    npu_stable_identity(npu));
    cache->backend = lsm_npu_telemetry_create(npu);
}

static void synchronise_telemetry_caches(LsmMonitor *monitor)
{
    LsmLinuxHardwareState *state = hardware_state(monitor);
    if (!state) return;

    LsmGpuTelemetryCache next_gpu[LSM_MAX_GPUS];
    LsmNpuTelemetryCache next_npu[LSM_MAX_NPUS];
    memset(next_gpu, 0, sizeof(next_gpu));
    memset(next_npu, 0, sizeof(next_npu));

    for (size_t index = 0U; index < monitor->gpu_count; index++) {
        const char *identity = gpu_stable_identity(&monitor->gpus[index]);
        size_t old_index = state->gpu_telemetry_count;
        for (size_t candidate = 0U; candidate < state->gpu_telemetry_count;
             candidate++) {
            if (strcmp(state->gpu_telemetry[candidate].identity, identity) == 0) {
                old_index = candidate;
                break;
            }
        }
        if (old_index < state->gpu_telemetry_count) {
            next_gpu[index] = state->gpu_telemetry[old_index];
            memset(&state->gpu_telemetry[old_index], 0,
                   sizeof(state->gpu_telemetry[old_index]));
        } else {
            build_gpu_telemetry_cache(&monitor->gpus[index], &next_gpu[index]);
        }
    }
    for (size_t index = 0U; index < state->gpu_telemetry_count; index++)
        destroy_gpu_telemetry(&state->gpu_telemetry[index]);
    memcpy(state->gpu_telemetry, next_gpu, sizeof(next_gpu));
    state->gpu_telemetry_count = monitor->gpu_count;

    for (size_t index = 0U; index < monitor->npu_count; index++) {
        const char *identity = npu_stable_identity(&monitor->npus[index]);
        size_t old_index = state->npu_telemetry_count;
        for (size_t candidate = 0U; candidate < state->npu_telemetry_count;
             candidate++) {
            if (strcmp(state->npu_telemetry[candidate].identity, identity) == 0) {
                old_index = candidate;
                break;
            }
        }
        if (old_index < state->npu_telemetry_count) {
            next_npu[index] = state->npu_telemetry[old_index];
            memset(&state->npu_telemetry[old_index], 0,
                   sizeof(state->npu_telemetry[old_index]));
        } else {
            build_npu_telemetry_cache(&monitor->npus[index], &next_npu[index]);
        }
    }
    for (size_t index = 0U; index < state->npu_telemetry_count; index++)
        destroy_npu_telemetry(&state->npu_telemetry[index]);
    memcpy(state->npu_telemetry, next_npu, sizeof(next_npu));
    state->npu_telemetry_count = monitor->npu_count;
}

static void read_gpu_identity_details(LsmGpuInfo *gpu)
{
    if (!gpu) return;
    gpu->driver_version[0] = '\0';
    if (gpu->driver[0]) {
        char path[LSM_PATH_LEN];
        (void)snprintf(path, sizeof(path), "/sys/module/%.63s/version",
                       gpu->driver);
        if (!lsm_read_text_file(path, gpu->driver_version,
                                sizeof(gpu->driver_version))) {
            (void)snprintf(path, sizeof(path), "/sys/module/%.63s/srcversion",
                           gpu->driver);
            (void)lsm_read_text_file(path, gpu->driver_version,
                                     sizeof(gpu->driver_version));
        }
    }

    gpu->pci_location[0] = '\0';
    const char *base = strrchr(gpu->platform_identity, '/');
    base = base ? base + 1U : gpu->platform_identity;
    if (base && strchr(base, ':') && strchr(base, '.'))
        lsm_copy_string(gpu->pci_location, sizeof(gpu->pci_location), base);
}

static void update_gpu_active_engine(LsmGpuInfo *gpu)
{
    if (!gpu) return;
    struct EngineValue {
        const char *name;
        double value;
        bool available;
    };
    const struct EngineValue engines[] = {
        {"Render", gpu->render_percent, gpu->render_available},
        {"Compute", gpu->compute_percent, gpu->compute_available},
        {"Video", gpu->video_percent, gpu->video_available},
        {"Video enhance", gpu->video_enhance_percent,
         gpu->video_enhance_available},
        {"Copy", gpu->copy_percent, gpu->copy_available},
        {"Memory", gpu->memory_busy_percent, gpu->memory_busy_available},
        {"Encode", gpu->encoder_percent, gpu->encoder_available},
        {"Decode", gpu->decoder_percent, gpu->decoder_available},
        {"Overall", gpu->utilization_percent, gpu->utilization_available}
    };
    const char *name = "N/A";
    double peak = 0.0;
    bool found = false;
    for (size_t index = 0U; index < LSM_ARRAY_LENGTH(engines); index++) {
        if (!engines[index].available || !isfinite(engines[index].value))
            continue;
        if (!found || engines[index].value > peak) {
            peak = engines[index].value;
            name = engines[index].name;
            found = true;
        }
    }
    gpu->active_engine_percent = found ? peak : 0.0;
    lsm_copy_string(gpu->active_engine, sizeof(gpu->active_engine),
                    !found ? "N/A" : peak < 0.5 ? "Idle" : name);
}

/* Vendor-neutral discovery starts with DRM; vendor-specific telemetry is
 * capability-detected after the stable device identity is known. */
static void enumerate_gpus(LsmMonitor *monitor)
{
    monitor->gpu_count = 0;
    LsmGpuRecord records[LSM_MAX_GPUS] = {0};
    const size_t count = lsm_sources_list_gpus(
        monitor_system_sources(monitor), records, LSM_MAX_GPUS);
    for (size_t index = 0; index < count; index++) {
        LsmGpuInfo *gpu = &monitor->gpus[monitor->gpu_count++];
        memset(gpu, 0, sizeof(*gpu));
        lsm_copy_string(gpu->display_identifier, sizeof(gpu->display_identifier),
                        records[index].card);
        lsm_copy_string(gpu->driver, sizeof(gpu->driver), records[index].driver);
        lsm_copy_string(gpu->platform_identity, sizeof(gpu->platform_identity),
                        records[index].device_syspath);
        read_gpu_identity_details(gpu);
        if (records[index].product[0] &&
            strcmp(records[index].product, "N/A") != 0)
            lsm_copy_string(gpu->name, sizeof(gpu->name), records[index].product);
        else if (records[index].vendor[0] &&
                 strcmp(records[index].vendor, "N/A") != 0) {
            lsm_copy_string(gpu->name, sizeof(gpu->name), records[index].vendor);
            strncat(gpu->name, " graphics", sizeof(gpu->name) - strlen(gpu->name) - 1U);
        } else {
            snprintf(gpu->name, sizeof(gpu->name), "GPU %zu", index);
        }
        if (lsm_intel_gpu_driver_supported(gpu->driver)) {
            gpu->engine_metrics_capable = true;
            char vram_path[LSM_PATH_LEN];
            uint64_t vram_total = 0U;
            gpu->shared_system_memory =
                !lsm_join_path(vram_path, sizeof(vram_path),
                               gpu->platform_identity, "/mem_info_vram_total") ||
                !lsm_read_u64_file(vram_path, &vram_total) || vram_total == 0U;
            gpu->integrated_cooling = gpu->shared_system_memory;
            lsm_copy_string(gpu->metrics_source,
                            sizeof(gpu->metrics_source),
                            "Native Intel backend");
        }
    }
}

static double read_active_dpm_clock(const char *path)
{
    FILE *file = fopen(path, "r");
    if (!file) return NAN;
    char line[256];
    double clock = NAN;
    while (fgets(line, sizeof(line), file)) {
        if (!strchr(line, '*')) continue;
        double value = 0.0;
        if (sscanf(line, "%*u: %lf", &value) == 1) {
            clock = value;
            break;
        }
    }
    fclose(file);
    return clock;
}

static bool read_gpu_engine_busy(const LsmGpuTelemetryCache *cache,
                                 uint64_t *total)
{
    if (!cache || !total || cache->engine_count == 0U) return false;
    uint64_t sum = 0U;
    for (size_t index = 0U; index < cache->engine_count; index++) {
        uint64_t value = 0U;
        if (!cache->engine_busy[index] ||
            !lsm_read_u64_file(cache->engine_busy[index], &value))
            return false;
        if (UINT64_MAX - sum < value) return false;
        sum += value;
    }
    *total = sum;
    return true;
}

static void update_gpus(LsmMonitor *monitor, double elapsed)
{
    LsmLinuxHardwareState *state = hardware_state(monitor);
    for (size_t index = 0U; index < monitor->gpu_count; index++) {
        LsmGpuInfo *gpu = &monitor->gpus[index];
        LsmLinuxGpuState *gpu_state = find_gpu_state(monitor, gpu);
        const LsmGpuTelemetryCache *telemetry =
            state && index < state->gpu_telemetry_count
                ? &state->gpu_telemetry[index] : NULL;

        /* Availability is sampled independently from the numeric value. A
         * valid zero-percent reading must not be presented as unavailable. */
        gpu->utilization_available = false;
        gpu->memory_busy_available = false;
        gpu->encoder_available = false;
        gpu->decoder_available = false;
        gpu->render_available = false;
        gpu->compute_available = false;
        gpu->video_available = false;
        gpu->video_enhance_available = false;
        gpu->copy_available = false;
        gpu->temperature_available = false;
        gpu->core_clock_available = false;
        gpu->memory_clock_available = false;
        gpu->power_available = false;
        gpu->fan_available = false;
        gpu->temperature_c = NAN;
        gpu->core_clock_mhz = NAN;
        gpu->memory_clock_mhz = NAN;
        gpu->power_watts = NAN;
        gpu->fan_percent = NAN;
        gpu->supported_metrics = false;
        if (!gpu_state || !gpu_state->intel_native_backend) {
            gpu->shared_system_memory = false;
            gpu->integrated_cooling = false;
            gpu->metrics_source[0] = '\0';
        }

        const uint64_t used = telemetry && telemetry->vram_used
            ? lsm_read_u64_or_zero(telemetry->vram_used) : 0U;
        const uint64_t total = telemetry && telemetry->vram_total
            ? lsm_read_u64_or_zero(telemetry->vram_total) : 0U;
        gpu->memory_used_bytes = used;
        gpu->memory_total_bytes = total;
        gpu->memory_percent = lsm_percent_u64(used, total);

        bool native_intel = false;
        if (telemetry && telemetry->intel_backend) {
            native_intel = lsm_intel_gpu_refresh(
                telemetry->intel_backend, gpu, elapsed);
        }

        if (!native_intel) {
            uint64_t busy = 0U;
            const bool busy_available = telemetry && telemetry->gpu_busy &&
                lsm_read_u64_file(telemetry->gpu_busy, &busy);
            if (busy_available) {
                gpu->utilization_percent = fmin(100.0, (double)busy);
                gpu->utilization_available = true;
            } else {
                uint64_t engine_busy = 0U;
                const bool engine_busy_available =
                    read_gpu_engine_busy(telemetry, &engine_busy);
                if (engine_busy_available && gpu_state) {
                    if (gpu_state->engine_busy_initialized &&
                        engine_busy >= gpu_state->previous_engine_busy_ns &&
                        elapsed > 0.0) {
                        const double percent = 100.0 *
                            (double)(engine_busy - gpu_state->previous_engine_busy_ns) /
                            (elapsed * 1000000000.0);
                        gpu->utilization_percent =
                            fmin(100.0, fmax(0.0, percent));
                    } else {
                        gpu->utilization_percent = 0.0;
                    }
                    gpu_state->previous_engine_busy_ns = engine_busy;
                    gpu_state->engine_busy_initialized = true;
                    gpu->utilization_available = true;
                } else {
                    /* Never turn a failed cumulative read into a zero sample.
                     * Reset the baseline so recovery cannot create a false spike. */
                    if (gpu_state) {
                        gpu_state->previous_engine_busy_ns = 0U;
                        gpu_state->engine_busy_initialized = false;
                    }
                    gpu->utilization_available = false;
                }
            }

            uint64_t optional_value = 0U;
            if (telemetry && telemetry->memory_busy &&
                lsm_read_u64_file(telemetry->memory_busy, &optional_value)) {
                gpu->memory_busy_percent =
                    fmin(100.0, (double)optional_value);
                gpu->memory_busy_available = true;
            }

            if (telemetry && telemetry->core_clock) {
                gpu->core_clock_mhz = lsm_read_double_or_nan(telemetry->core_clock);
                gpu->core_clock_available = isfinite(gpu->core_clock_mhz) &&
                                            gpu->core_clock_mhz > 0.0;
            }
            if (!gpu->core_clock_available && telemetry && telemetry->core_clock_dpm) {
                gpu->core_clock_mhz = read_active_dpm_clock(telemetry->core_clock_dpm);
                gpu->core_clock_available = isfinite(gpu->core_clock_mhz) &&
                                            gpu->core_clock_mhz > 0.0;
            }
            if (telemetry && telemetry->memory_clock) {
                gpu->memory_clock_mhz = lsm_read_double_or_nan(telemetry->memory_clock);
                gpu->memory_clock_available = isfinite(gpu->memory_clock_mhz) &&
                                              gpu->memory_clock_mhz > 0.0;
            }
            if (!gpu->memory_clock_available && telemetry && telemetry->memory_clock_dpm) {
                gpu->memory_clock_mhz = read_active_dpm_clock(telemetry->memory_clock_dpm);
                gpu->memory_clock_available = isfinite(gpu->memory_clock_mhz) &&
                                              gpu->memory_clock_mhz > 0.0;
            }

            gpu->supported_metrics = gpu->utilization_available || total > 0U ||
                                     (gpu_state && gpu_state->engine_busy_initialized) ||
                                     strcmp(gpu->driver, "amdgpu") == 0;
            if (gpu->supported_metrics && !gpu->metrics_source[0])
                lsm_copy_string(gpu->metrics_source,
                                sizeof(gpu->metrics_source),
                                strcmp(gpu->driver, "amdgpu") == 0
                                    ? "Native AMD driver telemetry"
                                    : "Native DRM driver telemetry");
        }

        uint64_t value = 0U;
        if (!gpu->temperature_available && telemetry && telemetry->temperature &&
            lsm_read_u64_file(telemetry->temperature, &value)) {
            gpu->temperature_c = (double)value / 1000.0;
            gpu->temperature_available = true;
        }
        if (!gpu->power_available && telemetry && telemetry->power &&
            lsm_read_u64_file(telemetry->power, &value)) {
            gpu->power_watts = (double)value / 1000000.0;
            gpu->power_available = true;
        }
        uint64_t pwm = 0U;
        uint64_t pwm_max = 0U;
        const bool have_pwm = telemetry && telemetry->pwm &&
                              lsm_read_u64_file(telemetry->pwm, &pwm);
        const bool have_pwm_max = telemetry && telemetry->pwm_max &&
                                  lsm_read_u64_file(telemetry->pwm_max, &pwm_max);
        if (have_pwm && have_pwm_max && pwm_max > 0U) {
            gpu->fan_percent = lsm_percent_u64(pwm, pwm_max);
            gpu->fan_available = true;
        }
    }

    /* NVML is loaded in-process when the NVIDIA driver supplies it. */
    lsm_nvml_refresh(monitor);
    for (size_t index = 0U; index < monitor->gpu_count; index++) {
        LsmGpuInfo *gpu = &monitor->gpus[index];
        if (strncasecmp(gpu->driver, "NVIDIA", 6U) == 0 &&
            gpu->supported_metrics)
            lsm_copy_string(gpu->metrics_source, sizeof(gpu->metrics_source),
                            "Native NVIDIA NVML");
        update_gpu_active_engine(gpu);
    }
}

/* NPU discovery deliberately tolerates incomplete early driver ABIs. */
static void enumerate_npus(LsmMonitor *monitor)
{
    monitor->npu_count = 0;
    DIR *directory = opendir("/sys/class/accel");
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory)) && monitor->npu_count < LSM_MAX_NPUS) {
        if (strncmp(entry->d_name, "accel", 5) != 0 ||
            !isdigit((unsigned char)entry->d_name[5])) continue;
        LsmNpuInfo *npu = &monitor->npus[monitor->npu_count++];
        memset(npu, 0, sizeof(*npu));
        lsm_copy_string(npu->display_identifier, sizeof(npu->display_identifier),
                        entry->d_name);
        snprintf(npu->device_identifier, sizeof(npu->device_identifier),
                 "/dev/accel/%s", entry->d_name);

        char link[LSM_PATH_LEN], resolved[LSM_PATH_LEN];
        snprintf(link, sizeof(link), "/sys/class/accel/%s/device", entry->d_name);
        if (lsm_realpath_copy(link, resolved, sizeof(resolved)))
            lsm_copy_string(npu->platform_identity, sizeof(npu->platform_identity), resolved);
        else
            lsm_copy_string(npu->platform_identity, sizeof(npu->platform_identity), link);

        char driver_link[LSM_PATH_LEN];
        (void)lsm_join_path(link, sizeof(link), npu->platform_identity, "/driver");
        ssize_t length = readlink(link, driver_link, sizeof(driver_link) - 1);
        if (length > 0) {
            driver_link[length] = '\0';
            const char *base = strrchr(driver_link, '/');
            lsm_copy_string(npu->driver, sizeof(npu->driver), base ? base + 1 : driver_link);
        }

        char vendor_id[32] = "", device_id[32] = "", vendor[LSM_NAME_LEN] = "";
        char product[LSM_NAME_LEN] = "";
        (void)lsm_join_path(link, sizeof(link), npu->platform_identity, "/vendor");
        lsm_read_text_file(link, vendor_id, sizeof(vendor_id));
        (void)lsm_join_path(link, sizeof(link), npu->platform_identity, "/device");
        lsm_read_text_file(link, device_id, sizeof(device_id));
        (void)lsm_pci_names_lookup(vendor_id, device_id, vendor, sizeof(vendor),
                                   product, sizeof(product));
        if (product[0]) lsm_copy_string(npu->name, sizeof(npu->name), product);
        else if (npu->driver[0]) {
            char driver_name[64];
            lsm_copy_string(driver_name, sizeof(driver_name), npu->driver);
            snprintf(npu->name, sizeof(npu->name), "%.96s accelerator", driver_name);
        }
        else snprintf(npu->name, sizeof(npu->name), "NPU %zu", monitor->npu_count - 1);
    }
    closedir(directory);
}

static void update_npus(LsmMonitor *monitor, double elapsed)
{
    LsmLinuxHardwareState *state = hardware_state(monitor);
    for (size_t index = 0U; index < monitor->npu_count; index++) {
        LsmNpuInfo *npu = &monitor->npus[index];
        LsmNpuTelemetryCache *telemetry =
            state && index < state->npu_telemetry_count
                ? &state->npu_telemetry[index] : NULL;

        if (!telemetry || !telemetry->backend) {
            npu->supported_metrics = false;
            npu->metrics_source[0] = '\0';
            continue;
        }
        (void)lsm_npu_telemetry_refresh(telemetry->backend, npu, elapsed);
    }
}

static bool bluetooth_device_state_matches(
    const LsmLinuxBluetoothDeviceState *state,
    const LsmBluetoothDeviceInfo *device)
{
    return state && device &&
        strcmp(state->controller, device->controller) == 0 &&
        strcmp(state->address, device->address) == 0;
}

static LsmLinuxBluetoothDeviceState *find_bluetooth_device_state(
    LsmLinuxMonitorBackendState *state,
    const LsmBluetoothDeviceInfo *device)
{
    if (!state || !device) return NULL;
    for (size_t index = 0U; index < state->bluetooth_device_count; index++)
        if (bluetooth_device_state_matches(
                &state->bluetooth_devices[index], device))
            return &state->bluetooth_devices[index];
    return NULL;
}

static void reconcile_bluetooth_device_states(LsmMonitor *monitor)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return;

    LsmLinuxBluetoothDeviceState next[LSM_MAX_BLUETOOTH_DEVICES] = {0};
    const size_t count =
        monitor->bluetooth_device_count < LSM_MAX_BLUETOOTH_DEVICES
            ? monitor->bluetooth_device_count : LSM_MAX_BLUETOOTH_DEVICES;
    for (size_t index = 0U; index < count; index++) {
        const LsmBluetoothDeviceInfo *device =
            &monitor->bluetooth_devices[index];
        LsmLinuxBluetoothDeviceState *old =
            find_bluetooth_device_state(state, device);
        if (old) next[index] = *old;
        lsm_copy_string(next[index].controller,
                        sizeof(next[index].controller),
                        device->controller);
        lsm_copy_string(next[index].address, sizeof(next[index].address),
                        device->address);
    }
    memcpy(state->bluetooth_devices, next, sizeof(next));
    state->bluetooth_device_count = count;
}

static void update_bluetooth_traffic(LsmMonitor *monitor, double elapsed)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return;

    for (size_t index = 0U; index < monitor->bluetooth_count; index++)
        (void)lsm_bluetooth_traffic_refresh_connections(
            monitor->bluetooth[index].name);

    const size_t count =
        monitor->bluetooth_device_count < state->bluetooth_device_count
            ? monitor->bluetooth_device_count
            : state->bluetooth_device_count;
    for (size_t index = 0U; index < count; index++) {
        LsmBluetoothDeviceInfo *device =
            &monitor->bluetooth_devices[index];
        LsmBluetoothTrafficCounters counters;
        if (lsm_bluetooth_traffic_read_device(
                device->controller, device->address, &counters)) {
            lsm_bluetooth_traffic_apply_device(
                device, &state->bluetooth_devices[index].accounting,
                &counters, elapsed);
        } else {
            lsm_bluetooth_traffic_mark_device_unavailable(
                device, &state->bluetooth_devices[index].accounting);
        }
    }
}

static bool bluetooth_membership_changed(
    const LsmBluetoothDeviceInfo *old_records, size_t old_count,
    const LsmMonitor *monitor)
{
    if (!monitor || old_count != monitor->bluetooth_device_count) return true;
    for (size_t index = 0U; index < monitor->bluetooth_device_count; index++) {
        const LsmBluetoothDeviceInfo *current =
            &monitor->bluetooth_devices[index];
        bool found = false;
        for (size_t old_index = 0U; old_index < old_count; old_index++) {
            const LsmBluetoothDeviceInfo *old = &old_records[old_index];
            if (strcmp(current->controller, old->controller) == 0 &&
                strcmp(current->address, old->address) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return true;
    }
    return false;
}


/** Rescan optional hardware while retaining live metric baselines by stable ID. */
/* Rebuild into temporary arrays, carry forward matching state, then commit. */
static void refresh_hardware_topology(LsmMonitor *monitor)
{
    LsmGpuInfo old_gpus[LSM_MAX_GPUS];
    LsmBatteryInfo old_batteries[LSM_MAX_BATTERIES];
    LsmNpuInfo old_npus[LSM_MAX_NPUS];
    LsmBluetoothDeviceInfo old_bluetooth_devices[LSM_MAX_BLUETOOTH_DEVICES];
    const size_t old_gpu_count = monitor->gpu_count;
    const size_t old_battery_count = monitor->battery_count;
    const size_t old_npu_count = monitor->npu_count;
    const size_t old_bluetooth_device_count = monitor->bluetooth_device_count;
    memcpy(old_gpus, monitor->gpus, sizeof(old_gpus));
    memcpy(old_batteries, monitor->batteries, sizeof(old_batteries));
    memcpy(old_npus, monitor->npus, sizeof(old_npus));
    memcpy(old_bluetooth_devices, monitor->bluetooth_devices,
           sizeof(old_bluetooth_devices));

    enumerate_gpus(monitor);
    lsm_bluetooth_enumerate(monitor);
    reconcile_bluetooth_device_states(monitor);
    lsm_battery_enumerate(monitor);
    enumerate_npus(monitor);
    const bool changed = lsm_hardware_topology_reconcile(
        monitor, old_gpus, old_gpu_count, old_batteries, old_battery_count,
        old_npus, old_npu_count) ||
        bluetooth_membership_changed(
            old_bluetooth_devices, old_bluetooth_device_count, monitor);

    reconcile_gpu_states(monitor);
    synchronise_telemetry_caches(monitor);

    if (monitor->gpu_count < LSM_MAX_GPUS)
        memset(&monitor->gpus[monitor->gpu_count], 0,
               (LSM_MAX_GPUS - monitor->gpu_count) * sizeof(monitor->gpus[0]));
    if (monitor->battery_count < LSM_MAX_BATTERIES)
        memset(&monitor->batteries[monitor->battery_count], 0,
               (LSM_MAX_BATTERIES - monitor->battery_count) *
               sizeof(monitor->batteries[0]));
    if (monitor->npu_count < LSM_MAX_NPUS)
        memset(&monitor->npus[monitor->npu_count], 0,
               (LSM_MAX_NPUS - monitor->npu_count) * sizeof(monitor->npus[0]));
    if (changed) monitor->topology_generation++;
}

/* Public hardware lifecycle. */
void lsm_hardware_initialise(LsmMonitor *monitor)
{
    if (!monitor) return;
    LsmLinuxMonitorBackendState *backend = monitor_backend_state(monitor);
    if (!backend) return;
    if (!backend->hardware_state)
        backend->hardware_state = calloc(1U, sizeof(*backend->hardware_state));

    lsm_battery_start();
    refresh_hardware_topology(monitor);
    update_bluetooth_traffic(monitor, 0.0);
    lsm_battery_update(monitor);
    update_npus(monitor, 1.0);
}

void lsm_hardware_update(LsmMonitor *monitor, double elapsed,
                         bool refresh_topology, bool refresh_batteries)
{
    if (!monitor) return;
    if (refresh_topology) refresh_hardware_topology(monitor);
    update_bluetooth_traffic(monitor, elapsed);
    update_gpus(monitor, elapsed);
    if (refresh_batteries) lsm_battery_update(monitor);
    update_npus(monitor, elapsed);
}

void lsm_hardware_shutdown(LsmMonitor *monitor)
{
    LsmLinuxMonitorBackendState *backend = monitor_backend_state(monitor);
    LsmLinuxHardwareState *state = backend ? backend->hardware_state : NULL;
    if (state) {
        for (size_t index = 0U; index < state->gpu_telemetry_count; index++)
            destroy_gpu_telemetry(&state->gpu_telemetry[index]);
        for (size_t index = 0U; index < state->npu_telemetry_count; index++)
            destroy_npu_telemetry(&state->npu_telemetry[index]);
        free(state);
        backend->hardware_state = NULL;
    }
    lsm_battery_shutdown();
    lsm_nvml_shutdown();
}
