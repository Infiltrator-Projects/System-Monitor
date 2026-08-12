// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mock_nvml.c
 * @brief Mock NVML shared library used by the native adapter test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int nvmlReturn_t;
typedef void *nvmlDevice_t;
typedef struct { unsigned int gpu, memory; } nvmlUtilization_t;
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

nvmlReturn_t nvmlInit_v2(void);
nvmlReturn_t nvmlShutdown(void);
nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int *count);
nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(unsigned int index,
                                            nvmlDevice_t *device);
nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t device, char *name,
                                unsigned int length);
nvmlReturn_t nvmlDeviceGetPciInfo_v3(nvmlDevice_t device,
                                     nvmlPciInfo_t *pci);
nvmlReturn_t nvmlSystemGetDriverVersion(char *version, unsigned int length);
nvmlReturn_t nvmlDeviceGetUtilizationRates(nvmlDevice_t device,
                                            nvmlUtilization_t *rates);
nvmlReturn_t nvmlDeviceGetEncoderUtilization(nvmlDevice_t device,
                                              unsigned int *value,
                                              unsigned int *period);
nvmlReturn_t nvmlDeviceGetDecoderUtilization(nvmlDevice_t device,
                                              unsigned int *value,
                                              unsigned int *period);
nvmlReturn_t nvmlDeviceGetMemoryInfo(nvmlDevice_t device,
                                     nvmlMemory_t *memory);
nvmlReturn_t nvmlDeviceGetTemperature(nvmlDevice_t device, unsigned int sensor,
                                      unsigned int *value);
nvmlReturn_t nvmlDeviceGetClockInfo(nvmlDevice_t device, unsigned int clock,
                                    unsigned int *value);
nvmlReturn_t nvmlDeviceGetPowerUsage(nvmlDevice_t device, unsigned int *value);
nvmlReturn_t nvmlDeviceGetFanSpeed(nvmlDevice_t device, unsigned int *value);

static unsigned int device_index(nvmlDevice_t device)
{
    return (unsigned int)((uintptr_t)device - 1U);
}

nvmlReturn_t nvmlInit_v2(void) { return 0; }
nvmlReturn_t nvmlShutdown(void) { return 0; }
nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int *count) { *count = 2; return 0; }
nvmlReturn_t nvmlDeviceGetHandleByIndex_v2(unsigned int index, nvmlDevice_t *device)
{
    *device = index < 2 ? (void *)(uintptr_t)(index + 1U) : NULL;
    return index < 2 ? 0 : 1;
}
nvmlReturn_t nvmlDeviceGetName(nvmlDevice_t device, char *name, unsigned int length)
{
    snprintf(name, length, "Mock NVIDIA GPU %u", device_index(device));
    return 0;
}
nvmlReturn_t nvmlDeviceGetPciInfo_v3(nvmlDevice_t device, nvmlPciInfo_t *pci)
{
    memset(pci, 0, sizeof(*pci));
    const unsigned int bus = device_index(device) + 1U;
    pci->domain = 0;
    pci->bus = bus;
    pci->device = 0;
    snprintf(pci->bus_id, sizeof(pci->bus_id), "00000000:%02x:00.0", bus);
    snprintf(pci->bus_id_legacy, sizeof(pci->bus_id_legacy), "0000:%02x:00.0", bus);
    return 0;
}
nvmlReturn_t nvmlSystemGetDriverVersion(char *version, unsigned int length)
{ snprintf(version, length, "999.1"); return 0; }
nvmlReturn_t nvmlDeviceGetUtilizationRates(nvmlDevice_t device,
                                            nvmlUtilization_t *rates)
{
    const unsigned int index = device_index(device);
    rates->gpu = 42U + index * 42U;
    rates->memory = 17U + index;
    return 0;
}
nvmlReturn_t nvmlDeviceGetEncoderUtilization(nvmlDevice_t device,
                                              unsigned int *value,
                                              unsigned int *period)
{ *value = 5U + device_index(device); *period = 1000; return 0; }
nvmlReturn_t nvmlDeviceGetDecoderUtilization(nvmlDevice_t device,
                                              unsigned int *value,
                                              unsigned int *period)
{ *value = 7U + device_index(device); *period = 1000; return 0; }
nvmlReturn_t nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t *memory)
{
    const unsigned int index = device_index(device);
    memory->total = (8ULL + index) << 30;
    memory->used = (2ULL + index) << 30;
    memory->free = memory->total - memory->used;
    return 0;
}
nvmlReturn_t nvmlDeviceGetTemperature(nvmlDevice_t device, unsigned int sensor,
                                      unsigned int *value)
{ (void)sensor; *value = 65U + device_index(device); return 0; }
nvmlReturn_t nvmlDeviceGetClockInfo(nvmlDevice_t device, unsigned int clock,
                                    unsigned int *value)
{ *value = (clock == 2 ? 9000U : 2100U) + device_index(device); return 0; }
nvmlReturn_t nvmlDeviceGetPowerUsage(nvmlDevice_t device, unsigned int *value)
{ *value = 125000U + device_index(device) * 1000U; return 0; }
nvmlReturn_t nvmlDeviceGetFanSpeed(nvmlDevice_t device, unsigned int *value)
{ *value = 33U + device_index(device); return 0; }
