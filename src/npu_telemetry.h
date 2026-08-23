// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file npu_telemetry.h
 * @brief Native, optional telemetry for Linux accelerator/NPU devices.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_NPU_TELEMETRY_H
#define LINUX_SYSTEM_MONITOR_NPU_TELEMETRY_H

#include "monitor_types.h"

#include <stdbool.h>

typedef struct LsmNpuTelemetry LsmNpuTelemetry;

/**
 * Resolve documented telemetry attributes for one accelerator device.
 *
 * Driver-specific profiles are preferred. Generic attributes are accepted only
 * when their filenames encode unambiguous units. Resolved paths are retained
 * in storage owned directly by the returned context.
 *
 * @param [in] npu Enumerated accelerator identity and device path.
 * @return Retained telemetry context, or NULL when no safe source is available.
 */
LsmNpuTelemetry *lsm_npu_telemetry_create(const LsmNpuInfo *npu);
/**
 * Refresh independent NPU activity, memory, frequency and sensor values.
 *
 * Missing attributes invalidate only their own fields. Cumulative activity
 * counters retain a monotonic baseline and never convert a failed read to zero.
 *
 * @param [in,out] telemetry Retained telemetry context.
 * @param [in,out] npu Destination accelerator snapshot.
 * @param [in] elapsed_seconds Monotonic interval since the previous sample.
 * @return true when at least one current metric was obtained.
 */
bool lsm_npu_telemetry_refresh(LsmNpuTelemetry *telemetry,
                               LsmNpuInfo *npu,
                               double elapsed_seconds);
/**
 * Release the retained NPU telemetry context.
 *
 * @param [in,out] telemetry Context to release, or NULL.
 */
void lsm_npu_telemetry_destroy(LsmNpuTelemetry *telemetry);

#endif
