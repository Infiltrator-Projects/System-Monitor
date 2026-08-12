// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file npu_telemetry.c
 * @brief Driver-neutral NPU telemetry with Intel IVPU cumulative busy support.
 *
 * Linux accelerator drivers expose different subsets of read-only sysfs
 * attributes. Intel's IVPU ABI provides cumulative npu_busy_time_us plus
 * current frequency and resident NPU memory. Other drivers may provide an
 * instantaneous utilization percentage. Every attribute is discovered once
 * and sampled independently without loading a vendor userspace library.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */

#include "npu_telemetry.h"

#include "common.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LSM_NPU_MIN_BUSY_SAMPLE_SECONDS 0.9

struct LsmNpuTelemetry {
    char *utilization;
    char *busy_time_us;
    char *memory_used;
    char *memory_total;
    char *clock;
    char *temperature;
    char *power;
    uint64_t previous_busy_time_us;
    double busy_elapsed_seconds;
    double last_busy_percent;
    bool busy_initialised;
    bool last_busy_valid;
    bool intel_ivpu;
};

static char *existing_path(const char *base, const char *suffix)
{
    char path[LSM_PATH_LEN];
    if (!base || !*base || !suffix ||
        !lsm_join_path(path, sizeof(path), base, suffix) ||
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

static const char *telemetry_base(const LsmNpuInfo *npu)
{
    const char *override = getenv("LSM_NPU_SYSFS_ROOT");
    if (override && override[0]) return override;
    return npu ? npu->platform_identity : NULL;
}

LsmNpuTelemetry *lsm_npu_telemetry_create(const LsmNpuInfo *npu)
{
    static const char *const generic_utilization_paths[] = {
        "/utilization_percent", "/busy_percent", "/npu_busy_percent"
    };
    static const char *const generic_busy_time_paths[] = {
        "/busy_time_us"
    };
    static const char *const generic_memory_used_paths[] = {
        "/memory_used_bytes", "/mem_used_bytes"
    };
    static const char *const generic_memory_total_paths[] = {
        "/memory_total_bytes", "/mem_total_bytes"
    };
    static const char *const generic_clock_paths[] = {
        "/clock_mhz", "/cur_freq_mhz"
    };
    static const char *const generic_temperature_paths[] = {
        "/temperature_millicelsius", "/temp_millicelsius", "/temp1_input"
    };
    static const char *const generic_power_paths[] = {
        "/power_uw", "/power1_input"
    };

    if (!npu) return NULL;
    const char *base = telemetry_base(npu);
    if (!base || !base[0]) return NULL;

    LsmNpuTelemetry *telemetry = calloc(1U, sizeof(*telemetry));
    if (!telemetry) return NULL;
    telemetry->intel_ivpu = lsm_string_equal(npu->driver, "intel_vpu") ||
                            lsm_string_equal(npu->driver, "ivpu");

    if (telemetry->intel_ivpu) {
        /* These names and units are part of the documented Intel IVPU ABI:
         * busy time is cumulative microseconds, memory is bytes and the
         * current frequency is MHz. Avoid guessing units from unrelated
         * vendor attributes. */
        telemetry->busy_time_us = existing_path(base, "/npu_busy_time_us");
        telemetry->memory_used =
            existing_path(base, "/npu_memory_utilization");
        telemetry->clock = existing_path(base, "/freq/current_freq");
    } else {
        /* Unknown drivers are accepted only when the attribute name states
         * its unit. Driver-specific profiles can extend this list safely. */
        telemetry->utilization = first_existing_path(
            base, generic_utilization_paths,
            LSM_ARRAY_LENGTH(generic_utilization_paths));
        telemetry->busy_time_us = first_existing_path(
            base, generic_busy_time_paths,
            LSM_ARRAY_LENGTH(generic_busy_time_paths));
        telemetry->memory_used = first_existing_path(
            base, generic_memory_used_paths,
            LSM_ARRAY_LENGTH(generic_memory_used_paths));
        telemetry->memory_total = first_existing_path(
            base, generic_memory_total_paths,
            LSM_ARRAY_LENGTH(generic_memory_total_paths));
        telemetry->clock = first_existing_path(
            base, generic_clock_paths, LSM_ARRAY_LENGTH(generic_clock_paths));
        telemetry->temperature = first_existing_path(
            base, generic_temperature_paths,
            LSM_ARRAY_LENGTH(generic_temperature_paths));
        telemetry->power = first_existing_path(
            base, generic_power_paths, LSM_ARRAY_LENGTH(generic_power_paths));
    }

    if (!telemetry->utilization && !telemetry->busy_time_us &&
        !telemetry->memory_used && !telemetry->memory_total &&
        !telemetry->clock && !telemetry->temperature && !telemetry->power) {
        lsm_npu_telemetry_destroy(telemetry);
        return NULL;
    }
    return telemetry;
}

static bool refresh_busy_time(LsmNpuTelemetry *telemetry,
                              LsmNpuInfo *npu,
                              double elapsed_seconds)
{
    if (!telemetry->busy_time_us || elapsed_seconds <= 0.0) return false;
    telemetry->busy_elapsed_seconds += elapsed_seconds;
    if (telemetry->busy_elapsed_seconds < LSM_NPU_MIN_BUSY_SAMPLE_SECONDS) {
        if (!telemetry->last_busy_valid) return false;
        npu->utilization_percent = telemetry->last_busy_percent;
        npu->utilization_available = true;
        return true;
    }

    uint64_t current = 0U;
    if (!lsm_read_u64_file(telemetry->busy_time_us, &current)) {
        telemetry->busy_elapsed_seconds = 0.0;
        telemetry->busy_initialised = false;
        telemetry->last_busy_valid = false;
        return false;
    }

    if (!telemetry->busy_initialised ||
        current < telemetry->previous_busy_time_us) {
        telemetry->previous_busy_time_us = current;
        telemetry->busy_elapsed_seconds = 0.0;
        telemetry->busy_initialised = true;
        telemetry->last_busy_valid = false;
        return false;
    }

    const uint64_t delta = current - telemetry->previous_busy_time_us;
    const double interval = telemetry->busy_elapsed_seconds;
    telemetry->previous_busy_time_us = current;
    telemetry->busy_elapsed_seconds = 0.0;
    telemetry->last_busy_percent = lsm_clamp_double(
        100.0 * (double)delta / (interval * 1000000.0), 0.0, 100.0);
    telemetry->last_busy_valid = true;
    npu->utilization_percent = telemetry->last_busy_percent;
    npu->utilization_available = true;
    return true;
}

bool lsm_npu_telemetry_refresh(LsmNpuTelemetry *telemetry,
                               LsmNpuInfo *npu,
                               double elapsed_seconds)
{
    if (!telemetry || !npu) return false;

    /* Availability belongs to this sample. Never retain a value merely
     * because an earlier sysfs read succeeded. */
    npu->utilization_available = false;
    npu->memory_used_available = false;
    npu->memory_total_available = false;
    npu->temperature_available = false;
    npu->clock_available = false;
    npu->power_available = false;
    npu->supported_metrics = false;
    npu->utilization_percent = 0.0;
    npu->memory_percent = 0.0;
    npu->memory_used_bytes = 0U;
    npu->memory_total_bytes = 0U;
    npu->temperature_c = NAN;
    npu->clock_mhz = NAN;
    npu->power_watts = NAN;
    npu->metrics_source[0] = '\0';

    bool any = false;
    uint64_t value = 0U;

    if (telemetry->utilization &&
        lsm_read_u64_file(telemetry->utilization, &value)) {
        npu->utilization_percent = lsm_clamp_double(
            (double)value, 0.0, 100.0);
        npu->utilization_available = true;
        any = true;
    } else {
        any = refresh_busy_time(telemetry, npu, elapsed_seconds) || any;
    }

    if (telemetry->memory_used &&
        lsm_read_u64_file(telemetry->memory_used, &value)) {
        npu->memory_used_bytes = value;
        npu->memory_used_available = true;
        any = true;
    }
    if (telemetry->memory_total &&
        lsm_read_u64_file(telemetry->memory_total, &value) && value > 0U) {
        npu->memory_total_bytes = value;
        npu->memory_total_available = true;
        any = true;
    }
    if (npu->memory_used_available && npu->memory_total_available) {
        npu->memory_percent = lsm_percent_u64(
            npu->memory_used_bytes, npu->memory_total_bytes);
    }

    if (telemetry->clock && lsm_read_u64_file(telemetry->clock, &value)) {
        npu->clock_mhz = (double)value;
        npu->clock_available = true;
        any = true;
    }
    if (telemetry->temperature &&
        lsm_read_u64_file(telemetry->temperature, &value)) {
        npu->temperature_c = (double)value / 1000.0;
        npu->temperature_available = true;
        any = true;
    }
    if (telemetry->power && lsm_read_u64_file(telemetry->power, &value)) {
        npu->power_watts = (double)value / 1000000.0;
        npu->power_available = true;
        any = true;
    }

    if (any) {
        lsm_copy_string(npu->metrics_source, sizeof(npu->metrics_source),
                        telemetry->intel_ivpu
                            ? "Native Intel IVPU sysfs"
                            : "Native accelerator sysfs (explicit units)");
        npu->supported_metrics = true;
    }
    return any;
}

void lsm_npu_telemetry_destroy(LsmNpuTelemetry *telemetry)
{
    if (!telemetry) return;
    free(telemetry->utilization);
    free(telemetry->busy_time_us);
    free(telemetry->memory_used);
    free(telemetry->memory_total);
    free(telemetry->clock);
    free(telemetry->temperature);
    free(telemetry->power);
    free(telemetry);
}
