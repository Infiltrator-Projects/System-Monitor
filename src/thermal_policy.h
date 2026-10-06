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
#ifndef INFILTRATOR_SYSTEM_MONITOR_THERMAL_POLICY_H
#define INFILTRATOR_SYSTEM_MONITOR_THERMAL_POLICY_H

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
 * Kernel-provided warning/critical thresholds win. Missing sensor limits are
 * filled from the internal processor table. Unknown processors retain the
 * conservative legacy 80/95 C fallback.
 *
 * @param [in] cpu Current processor identity and optional sensor limits.
 * @param [out] policy Receives a complete warning/fault policy.
 * @return true when both pointers are valid.
 */
bool lsm_cpu_thermal_policy(const LsmCpuInfo *cpu, LsmThermalPolicy *policy);

#endif
