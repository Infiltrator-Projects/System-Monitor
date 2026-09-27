// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file nvml.h
 * @brief Optional native NVIDIA Management Library adapter.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_NVML_H
#define INFILTRATOR_SYSTEM_MONITOR_NVML_H

#include "monitor_types.h"

typedef struct LsmNvmlContext LsmNvmlContext;

/**
 * Create an independent optional NVML runtime context.
 *
 * The context owns the dynamically loaded NVML library state for one monitor.
 * Failure to allocate the context leaves optional NVIDIA telemetry unavailable
 * without affecting generic DRM discovery.
 *
 * @return Newly allocated NVML context, or NULL on allocation failure.
 */
LsmNvmlContext *lsm_nvml_create(void);

/**
 * Refresh NVIDIA-only optional metrics through one monitor-owned NVML context.
 *
 * Failure to load NVML or match a PCI identity leaves NVIDIA extension fields
 * unavailable without affecting generic DRM discovery.
 *
 * @param [in,out] context Monitor-owned NVML runtime context.
 * @param [in,out] monitor Snapshot containing enumerated graphics adapters.
 */
void lsm_nvml_refresh(LsmNvmlContext *context, LsmMonitor *monitor);

/**
 * Release one NVML runtime context and its driver state.
 *
 * @param [in,out] context Context to destroy, or NULL.
 */
void lsm_nvml_destroy(LsmNvmlContext *context);

#endif
