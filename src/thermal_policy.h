// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file thermal_policy.h
 * @brief Processor-aware CPU thermal warning/fault threshold policy.
 *
 * Runtime sensor limits are authoritative when Linux exposes them. An internal
 * model table supplies documented limits when the kernel does not, and a
 * conservative generic fallback keeps the UI deterministic for unknown CPUs.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef SYSTEM_MONITOR_THERMAL_POLICY_H
#define SYSTEM_MONITOR_THERMAL_POLICY_H

#include "monitor_types.h"

/** Provenance of the thresholds selected for one CPU. */
typedef enum {
    LSM_THERMAL_POLICY_FALLBACK = 0,
    LSM_THERMAL_POLICY_MODEL_TABLE,
    LSM_THERMAL_POLICY_SENSOR
} LsmThermalPolicySource;

/** Warning/fault thresholds used by CPU thermal presentation. */
typedef struct {
    double warning_c;
    double fault_c;
    LsmThermalPolicySource source;
} LsmThermalPolicy;

/**
 * Resolve CPU thermal thresholds.
 *
 * Valid kernel sensor limits override the model-table and generic fallbacks.
 * When only one sensor limit exists, the other is derived from that limit;
 * different threshold sources are not combined. Output warning remains below
 * fault even when a valid critical sensor reports the minimum accepted input
 * temperature. Unknown processors without sensors use the 80/95 C fallback.
 *
 * @param [in] cpu Current processor identity and optional sensor limits.
 * @param [out] policy Receives a complete warning/fault policy.
 * @return true when both pointers are valid.
 */
bool lsm_cpu_thermal_policy(const LsmCpuInfo *cpu, LsmThermalPolicy *policy);

#endif
