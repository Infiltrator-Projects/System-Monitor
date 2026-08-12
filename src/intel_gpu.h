// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file intel_gpu.h
 * @brief Native Intel i915/Xe telemetry backend.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_INTEL_GPU_H
#define LINUX_SYSTEM_MONITOR_INTEL_GPU_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct LsmIntelGpuBackend LsmIntelGpuBackend;

/**
 * Test whether a DRM driver name is supported by the native Intel backend.
 *
 * @param [in] driver Kernel DRM driver name, normally "i915" or "xe".
 * @return true for a recognised Intel driver; false for NULL or other vendors.
 */
bool lsm_intel_gpu_driver_supported(const char *driver);

/**
 * Create retained PMU and sysfs sampling state for one Intel graphics adapter.
 *
 * Creation performs discovery only and never elevates privileges. Restricted
 * counters remain unavailable and may become usable only under the credentials
 * of the single GUI process itself.
 *
 * @param [in] gpu Enumerated adapter identity and canonical device path.
 * @return New backend on recognised hardware, or NULL when unsupported or when
 *         retained state cannot be allocated.
 */
LsmIntelGpuBackend *lsm_intel_gpu_create(const LsmGpuInfo *gpu);

/**
 * Refresh independently available Intel engine, frequency and sensor metrics.
 *
 * Cumulative PMU counters use retained baselines; a failed read invalidates the
 * corresponding metric rather than manufacturing a zero sample.
 *
 * @param [in,out] backend Retained backend returned by lsm_intel_gpu_create().
 * @param [in,out] gpu Destination adapter snapshot.
 * @param [in] elapsed_seconds Monotonic interval since the previous refresh.
 * @return true when at least one native metric was sampled successfully.
 */
bool lsm_intel_gpu_refresh(LsmIntelGpuBackend *backend, LsmGpuInfo *gpu,
                           double elapsed_seconds);

/**
 * Close PMU descriptors and release one Intel backend.
 *
 * @param [in,out] backend Backend to release, or NULL.
 */
void lsm_intel_gpu_destroy(LsmIntelGpuBackend *backend);


#endif
