// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file thermal_policy.c
 * @brief Processor-aware CPU thermal warning/fault threshold policy.
 *
 * The model table contains only limits backed by manufacturer documentation.
 * It is intentionally a fallback: Linux hwmon limits are preferred because
 * firmware/OEM thermal programming can be more specific than a retail SKU.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "thermal_policy.h"

#include "common.h"

#include <math.h>
#include <stddef.h>

#define LSM_THERMAL_FALLBACK_WARNING_C 80.0
#define LSM_THERMAL_FALLBACK_FAULT_C 95.0
#define LSM_THERMAL_WARNING_MARGIN_C 10.0
#define LSM_THERMAL_MIN_LIMIT_C 50.0
#define LSM_THERMAL_MAX_LIMIT_C 150.0

typedef struct {
    const char *model_fragment;
    double critical_c;
} LsmCpuThermalTableEntry;

static const LsmCpuThermalTableEntry cpu_thermal_table[] = {
    {"Core(TM) Ultra 5 125U", 110.0},
    {"Core(TM) Ultra 5 125H", 110.0},
    {"Core(TM) Ultra 5 135U", 110.0},
    {"Core(TM) Ultra 5 135H", 110.0},
    {"Core(TM) Ultra 7 155U", 110.0},
    {"Core(TM) Ultra 7 155H", 110.0},
    {"Core(TM) Ultra 7 165U", 110.0},
    {"Core(TM) Ultra 7 165H", 110.0},
    {"Core(TM) Ultra 9 185H", 110.0},
    {"Ryzen 7 7700X3D", 89.0},
    {"Ryzen 7 7800X3D", 89.0},
    {"Ryzen 9 7900X3D", 89.0},
    {"Ryzen 9 7950X3D", 89.0},
    {"Ryzen 5 7400", 95.0},
    {"Ryzen 5 7535HS", 95.0},
    {"Ryzen 7 7700X", 95.0},
    {"Ryzen 9 7900", 95.0},
    {"Ryzen 9 7950X", 95.0}
};

static bool valid_limit(double celsius)
{
    return isfinite(celsius) &&
           celsius >= LSM_THERMAL_MIN_LIMIT_C &&
           celsius <= LSM_THERMAL_MAX_LIMIT_C;
}

static bool table_limit(const char *model, double *critical_c)
{
    if (!model || !model[0] || !critical_c) return false;
    for (size_t index = 0U;
         index < LSM_ARRAY_LENGTH(cpu_thermal_table); index++) {
        if (lsm_ascii_contains_ci(
                model, cpu_thermal_table[index].model_fragment)) {
            *critical_c = cpu_thermal_table[index].critical_c;
            return true;
        }
    }
    return false;
}

bool lsm_cpu_thermal_policy(const LsmCpuInfo *cpu, LsmThermalPolicy *policy)
{
    if (!cpu || !policy) return false;

    double warning = LSM_THERMAL_FALLBACK_WARNING_C;
    double fault = LSM_THERMAL_FALLBACK_FAULT_C;
    LsmThermalPolicySource source = LSM_THERMAL_POLICY_FALLBACK;

    double table_critical = NAN;
    if (table_limit(cpu->model, &table_critical) &&
        valid_limit(table_critical)) {
        fault = table_critical;
        warning = table_critical - LSM_THERMAL_WARNING_MARGIN_C;
        source = LSM_THERMAL_POLICY_MODEL_TABLE;
    }

    const bool have_sensor_warning =
        cpu->temperature_warning_available &&
        valid_limit(cpu->temperature_warning_c);
    const bool have_sensor_critical =
        cpu->temperature_critical_available &&
        valid_limit(cpu->temperature_critical_c);

    if (have_sensor_warning) {
        warning = cpu->temperature_warning_c;
        source = LSM_THERMAL_POLICY_SENSOR;
    }
    if (have_sensor_critical) {
        fault = cpu->temperature_critical_c;
        if (!have_sensor_warning)
            warning = fault - LSM_THERMAL_WARNING_MARGIN_C;
        source = LSM_THERMAL_POLICY_SENSOR;
    } else if (have_sensor_warning && warning >= fault) {
        fault = warning + LSM_THERMAL_WARNING_MARGIN_C;
        if (fault > LSM_THERMAL_MAX_LIMIT_C)
            fault = LSM_THERMAL_MAX_LIMIT_C;
    }

    if (warning >= fault)
        warning = fault - 5.0;
    if (warning < LSM_THERMAL_MIN_LIMIT_C)
        warning = LSM_THERMAL_MIN_LIMIT_C;

    policy->warning_c = warning;
    policy->fault_c = fault;
    policy->source = source;
    return true;
}
