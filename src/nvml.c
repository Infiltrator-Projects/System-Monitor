// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file nvml.c
 * @brief Runtime-loaded NVIDIA Management Library metrics backend.
 *
 * NVIDIA's proprietary driver normally supplies libnvidia-ml.so.1. Loading
 * that native API directly avoids launching and parsing nvidia-smi once per
 * refresh. The adapter is entirely optional: systems without NVML continue
 * using DRM/sysfs metrics and gain no new package dependency.
 *
 * Infiltratr Common owns cross-platform module lifetime and symbol transfer.
 * This module retains only NVML-specific discovery, required/optional symbol
 * policy and metrics semantics.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */

#include "nvml.h"
#include "common.h"

#include <infiltratr/dynlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define NVML_SUCCESS 0
#define NVML_TEMPERATURE_GPU 0
#define NVML_CLOCK_GRAPHICS 0
#define NVML_CLOCK_MEM 2
#define NVML_NAME_BUFFER_SIZE 96
#define NVML_DRIVER_BUFFER_SIZE 96

typedef int nvmlReturn_t;
typedef struct nvmlDevice_st *nvmlDevice_t;
typedef struct {
    unsigned int gpu;
    unsigned int memory;
} nvmlUtilization_t;
typedef struct {
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} nvmlMemory_t;
typedef struct {
    char bus_id_legacy[16];
    unsigned int domain;
    unsigned int bus;
    unsigned int device;
    unsigned int pci_device_id;
    unsigned int pci_subsystem_id;
    char bus_id[32];
} nvmlPciInfo_t;

typedef struct {
    InfiltratrDynlib library;
    bool attempted;
    bool initialised;
    nvmlReturn_t (*init_v2)(void);
    nvmlReturn_t (*shutdown)(void);
    nvmlReturn_t (*device_get_count_v2)(unsigned int *);
    nvmlReturn_t (*device_get_handle_by_index_v2)(unsigned int, nvmlDevice_t *);
    nvmlReturn_t (*device_get_name)(nvmlDevice_t, char *, unsigned int);
    nvmlReturn_t (*device_get_pci_info_v3)(nvmlDevice_t, nvmlPciInfo_t *);
    nvmlReturn_t (*system_get_driver_version)(char *, unsigned int);
    nvmlReturn_t (*device_get_utilization_rates)(nvmlDevice_t, nvmlUtilization_t *);
    nvmlReturn_t (*device_get_encoder_utilization)(nvmlDevice_t, unsigned int *, unsigned int *);
    nvmlReturn_t (*device_get_decoder_utilization)(nvmlDevice_t, unsigned int *, unsigned int *);
    nvmlReturn_t (*device_get_memory_info)(nvmlDevice_t, nvmlMemory_t *);
    nvmlReturn_t (*device_get_temperature)(nvmlDevice_t, unsigned int, unsigned int *);
    nvmlReturn_t (*device_get_clock_info)(nvmlDevice_t, unsigned int, unsigned int *);
    nvmlReturn_t (*device_get_power_usage)(nvmlDevice_t, unsigned int *);
    nvmlReturn_t (*device_get_fan_speed)(nvmlDevice_t, unsigned int *);
} LsmNvmlApi;

static LsmNvmlApi api;

static bool load_symbol(void *destination, size_t destination_size,
                        const char *name, bool required)
{
    const bool found = infiltratr_dynlib_symbol(&api.library, name,
                                                destination,
                                                destination_size);
    return found || !required;
}

static bool initialise(void)
{
    if (api.attempted) return api.initialised;
    api.attempted = true;

    const char *override = getenv("LSM_NVML_LIBRARY");
    if (!infiltratr_dynlib_open(
            &api.library,
            override && *override ? override : "libnvidia-ml.so.1"))
        return false;

#define LOAD_REQUIRED(field, symbol) \
    if (!load_symbol(&api.field, sizeof api.field, symbol, true)) goto fail
#define LOAD_OPTIONAL(field, symbol) \
    (void)load_symbol(&api.field, sizeof api.field, symbol, false)

    LOAD_REQUIRED(init_v2, "nvmlInit_v2");
    LOAD_REQUIRED(shutdown, "nvmlShutdown");
    LOAD_REQUIRED(device_get_count_v2, "nvmlDeviceGetCount_v2");
    LOAD_REQUIRED(device_get_handle_by_index_v2, "nvmlDeviceGetHandleByIndex_v2");
    LOAD_REQUIRED(device_get_name, "nvmlDeviceGetName");
    LOAD_OPTIONAL(device_get_pci_info_v3, "nvmlDeviceGetPciInfo_v3");
    LOAD_OPTIONAL(system_get_driver_version, "nvmlSystemGetDriverVersion");
    LOAD_OPTIONAL(device_get_utilization_rates, "nvmlDeviceGetUtilizationRates");
    LOAD_OPTIONAL(device_get_encoder_utilization, "nvmlDeviceGetEncoderUtilization");
    LOAD_OPTIONAL(device_get_decoder_utilization, "nvmlDeviceGetDecoderUtilization");
    LOAD_OPTIONAL(device_get_memory_info, "nvmlDeviceGetMemoryInfo");
    LOAD_OPTIONAL(device_get_temperature, "nvmlDeviceGetTemperature");
    LOAD_OPTIONAL(device_get_clock_info, "nvmlDeviceGetClockInfo");
    LOAD_OPTIONAL(device_get_power_usage, "nvmlDeviceGetPowerUsage");
    LOAD_OPTIONAL(device_get_fan_speed, "nvmlDeviceGetFanSpeed");
#undef LOAD_REQUIRED
#undef LOAD_OPTIONAL

    if (api.init_v2() != NVML_SUCCESS) goto fail;
    api.initialised = true;
    return true;

fail:
    infiltratr_dynlib_close(&api.library);
    memset(&api, 0, sizeof(api));
    api.attempted = true;
    return false;
}

static bool is_nvidia_gpu(const LsmGpuInfo *gpu)
{
    return gpu && (strncasecmp(gpu->driver, "nvidia", 6) == 0 ||
                   strncasecmp(gpu->name, "nvidia", 6) == 0);
}

static bool normalise_pci_bus_id(const char *text, char *buffer, size_t size)
{
    if (!text || !*text || !buffer || size == 0) return false;
    unsigned int domain = 0, bus = 0, device = 0, function = 0;
    if (sscanf(text, "%x:%x:%x.%x", &domain, &bus, &device, &function) != 4)
        return false;
    snprintf(buffer, size, "%08x:%02x:%02x.%x", domain, bus, device, function);
    return true;
}

static bool gpu_pci_bus_id(const LsmGpuInfo *gpu, char *buffer, size_t size)
{
    if (!gpu || !gpu->platform_identity[0]) return false;
    const char *component = strrchr(gpu->platform_identity, '/');
    component = component ? component + 1 : gpu->platform_identity;
    return normalise_pci_bus_id(component, buffer, size);
}

static LsmGpuInfo *append_nvml_gpu(LsmMonitor *monitor, const char *card)
{
    for (size_t index = 0; index < monitor->gpu_count; index++)
        if (strcmp(monitor->gpus[index].display_identifier, card) == 0)
            return &monitor->gpus[index];
    if (monitor->gpu_count >= LSM_MAX_GPUS) return NULL;
    LsmGpuInfo *gpu = &monitor->gpus[monitor->gpu_count++];
    memset(gpu, 0, sizeof(*gpu));
    snprintf(gpu->display_identifier, sizeof(gpu->display_identifier), "%s", card);
    snprintf(gpu->driver, sizeof(gpu->driver), "NVIDIA");
    monitor->topology_generation++;
    return gpu;
}

static LsmGpuInfo *gpu_for_nvml_device(LsmMonitor *monitor, unsigned int index,
                                       nvmlDevice_t device)
{
    if (api.device_get_pci_info_v3) {
        nvmlPciInfo_t pci = {0};
        if (api.device_get_pci_info_v3(device, &pci) == NVML_SUCCESS) {
            char nvml_id[32];
            const char *reported_id = pci.bus_id[0]
                ? pci.bus_id : pci.bus_id_legacy;
            if (normalise_pci_bus_id(reported_id, nvml_id, sizeof(nvml_id))) {
                for (size_t gpu_index = 0; gpu_index < monitor->gpu_count;
                     gpu_index++) {
                    char sysfs_id[32];
                    if (gpu_pci_bus_id(&monitor->gpus[gpu_index], sysfs_id,
                                       sizeof(sysfs_id)) &&
                        strcmp(nvml_id, sysfs_id) == 0)
                        return &monitor->gpus[gpu_index];
                }

                /* A valid PCI identity must never be assigned by array order.
                 * Keep an unmatched NVML device as its own stable page. */
                char card[64];
                snprintf(card, sizeof(card), "nvml-%s", nvml_id);
                return append_nvml_gpu(monitor, card);
            }
        }
    }

    /* Older NVML libraries may not expose a usable PCI identity. Retain the
     * NVIDIA-only ordinal fallback rather than dropping otherwise valid data. */
    unsigned int current = 0;
    for (size_t gpu_index = 0; gpu_index < monitor->gpu_count; gpu_index++) {
        if (!is_nvidia_gpu(&monitor->gpus[gpu_index])) continue;
        if (current++ == index) return &monitor->gpus[gpu_index];
    }
    char card[64];
    snprintf(card, sizeof(card), "nvidia%u", index);
    return append_nvml_gpu(monitor, card);
}

void lsm_nvml_refresh(LsmMonitor *monitor)
{
    if (!monitor || !initialise()) return;

    unsigned int count = 0;
    if (api.device_get_count_v2(&count) != NVML_SUCCESS) return;

    char driver_version[NVML_DRIVER_BUFFER_SIZE] = "";
    if (api.system_get_driver_version)
        (void)api.system_get_driver_version(driver_version, sizeof(driver_version));

    for (unsigned int index = 0; index < count; index++) {
        nvmlDevice_t device = NULL;
        if (api.device_get_handle_by_index_v2(index, &device) != NVML_SUCCESS || !device)
            continue;
        LsmGpuInfo *gpu = gpu_for_nvml_device(monitor, index, device);
        if (!gpu) continue;

        char name[NVML_NAME_BUFFER_SIZE] = "";
        if (api.device_get_name(device, name, sizeof(name)) == NVML_SUCCESS && name[0])
            snprintf(gpu->name, sizeof(gpu->name), "%s", name);
        if (driver_version[0])
            snprintf(gpu->driver, sizeof(gpu->driver), "NVIDIA %.55s", driver_version);
        else if (!gpu->driver[0])
            snprintf(gpu->driver, sizeof(gpu->driver), "NVIDIA");

        if (api.device_get_utilization_rates) {
            nvmlUtilization_t utilization;
            if (api.device_get_utilization_rates(device, &utilization) == NVML_SUCCESS) {
                gpu->utilization_percent = utilization.gpu;
                gpu->utilization_available = true;
                gpu->memory_busy_percent = utilization.memory;
                gpu->memory_busy_available = true;
            }
        }

        if (api.device_get_encoder_utilization) {
            unsigned int percent = 0, sampling_period = 0;
            if (api.device_get_encoder_utilization(device, &percent, &sampling_period) == NVML_SUCCESS) {
                gpu->encoder_percent = percent;
                gpu->encoder_available = true;
            }
        }
        if (api.device_get_decoder_utilization) {
            unsigned int percent = 0, sampling_period = 0;
            if (api.device_get_decoder_utilization(device, &percent, &sampling_period) == NVML_SUCCESS) {
                gpu->decoder_percent = percent;
                gpu->decoder_available = true;
            }
        }

        if (api.device_get_memory_info) {
            nvmlMemory_t memory;
            if (api.device_get_memory_info(device, &memory) == NVML_SUCCESS) {
                gpu->memory_used_bytes = memory.used;
                gpu->memory_total_bytes = memory.total;
                gpu->memory_percent = lsm_percent_u64(memory.used, memory.total);
            }
        }

        if (api.device_get_temperature) {
            unsigned int value = 0;
            if (api.device_get_temperature(device, NVML_TEMPERATURE_GPU, &value) == NVML_SUCCESS) {
                gpu->temperature_c = value;
                gpu->temperature_available = true;
            }
        }
        if (api.device_get_clock_info) {
            unsigned int value = 0;
            if (api.device_get_clock_info(device, NVML_CLOCK_GRAPHICS, &value) == NVML_SUCCESS) {
                gpu->core_clock_mhz = value;
                gpu->core_clock_available = true;
            }
            if (api.device_get_clock_info(device, NVML_CLOCK_MEM, &value) == NVML_SUCCESS) {
                gpu->memory_clock_mhz = value;
                gpu->memory_clock_available = true;
            }
        }
        if (api.device_get_power_usage) {
            unsigned int milliwatts = 0;
            if (api.device_get_power_usage(device, &milliwatts) == NVML_SUCCESS) {
                gpu->power_watts = (double)milliwatts / 1000.0;
                gpu->power_available = true;
            }
        }
        if (api.device_get_fan_speed) {
            unsigned int percent = 0;
            if (api.device_get_fan_speed(device, &percent) == NVML_SUCCESS) {
                gpu->fan_percent = percent;
                gpu->fan_available = true;
            }
        }
        gpu->supported_metrics = true;
    }
}

void lsm_nvml_shutdown(void)
{
    if (api.initialised && api.shutdown) (void)api.shutdown();
    infiltratr_dynlib_close(&api.library);
    memset(&api, 0, sizeof(api));
}
