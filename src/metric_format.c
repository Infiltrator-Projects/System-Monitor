// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file metric_format.c
 * @brief Linux System Monitor compatibility facade over shared formatting.
 *
 * Formatting policy is owned by Infiltratr Common so Calendar Plus and future
 * applications can reuse the same dependency-free implementation. This file
 * preserves the established `lsm_metric_` API for existing monitor call sites.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "metric_format.h"

#include <infiltratr/format.h>

#include <math.h>
#include <stdio.h>

char *lsm_metric_format_memory_gb(uint64_t bytes, char *buffer, size_t size)
{
    return infiltratr_format_memory_gb(bytes, buffer, size);
}

char *lsm_metric_format_disk_capacity(uint64_t bytes, char *buffer, size_t size)
{
    return infiltratr_format_disk_capacity(bytes, buffer, size);
}

static long double network_value(long double bytes, bool use_bits)
{
    if (!isfinite(bytes) || bytes <= 0.0L) return 0.0L;
    return use_bits ? bytes * 8.0L : bytes;
}

static char *format_network_value(long double bytes, bool use_bits,
                                  bool per_second, char *buffer, size_t size)
{
    if (!buffer || size == 0U) return buffer;
    static const char *const byte_units[] = {"KB", "MB", "GB", "TB"};
    static const char *const bit_units[] = {"Kb", "Mb", "Gb", "Tb"};
    const char *const *units = use_bits ? bit_units : byte_units;
    long double value = network_value(bytes, use_bits) / 1000.0L;
    size_t unit = 0U;
    while (unit + 1U < 4U && value >= 1000.0L) {
        value /= 1000.0L;
        unit++;
    }
    (void)snprintf(buffer, size, "%.1Lf %s%s", value, units[unit],
                   per_second ? "/s" : "");
    return buffer;
}

char *lsm_metric_format_network(long double bytes, bool use_bits,
                                bool per_second, char *buffer, size_t size)
{
    return format_network_value(bytes, use_bits, per_second, buffer, size);
}

char *lsm_metric_format_network_pair(long double send_bytes,
                                     long double receive_bytes, bool use_bits,
                                     char *buffer, size_t size)
{
    if (!buffer || size == 0U) return buffer;
    static const char *const byte_units[] = {"KB", "MB", "GB", "TB"};
    static const char *const bit_units[] = {"Kb", "Mb", "Gb", "Tb"};
    const char *const *units = use_bits ? bit_units : byte_units;
    const long double send = network_value(send_bytes, use_bits);
    const long double receive = network_value(receive_bytes, use_bits);
    const long double largest = send > receive ? send : receive;
    long double divisor = 1000.0L;
    size_t unit = 0U;
    while (unit + 1U < 4U && largest / divisor >= 1000.0L) {
        divisor *= 1000.0L;
        unit++;
    }
    (void)snprintf(buffer, size, "S:%.1Lf R:%.1Lf %s/s",
                   send / divisor, receive / divisor, units[unit]);
    return buffer;
}

char *lsm_metric_format_link_speed_mbps(double megabits_per_second,
                                        char *buffer, size_t size)
{
    if (!buffer || size == 0U) return buffer;
    if (!isfinite(megabits_per_second) || megabits_per_second <= 0.0) {
        (void)snprintf(buffer, size, "N/A");
        return buffer;
    }
    static const char *const units[] = {"Kb/s", "Mb/s", "Gb/s", "Tb/s"};
    long double value = (long double)megabits_per_second;
    size_t unit = 1U;
    if (value < 1.0L) {
        value *= 1000.0L;
        unit = 0U;
    } else {
        while (unit + 1U < 4U && value >= 1000.0L) {
            value /= 1000.0L;
            unit++;
        }
    }
    (void)snprintf(buffer, size, "%.2Lf %s", value, units[unit]);
    return buffer;
}

char *lsm_metric_format_percent(bool available, double value,
                                char *buffer, size_t size)
{
    return infiltratr_format_percent(available, value, buffer, size);
}

char *lsm_metric_format_mhz(bool available, double value,
                            char *buffer, size_t size)
{
    return infiltratr_format_mhz(available, value, buffer, size);
}

char *lsm_metric_format_ghz(bool available, double value,
                            char *buffer, size_t size)
{
    InfiltratrScalarFormatOptions options = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    options.decimal_places = 2U;
    options.suffix = " GHz";
    (void)infiltratr_format_scalar(available, (long double)value, &options,
                                   buffer, size);
    return buffer;
}

char *lsm_metric_format_celsius(bool available, double value,
                                char *buffer, size_t size)
{
    return infiltratr_format_celsius(available, value, buffer, size);
}

char *lsm_metric_format_watts(bool available, double value,
                              char *buffer, size_t size)
{
    return infiltratr_format_watts(available, value, buffer, size);
}
