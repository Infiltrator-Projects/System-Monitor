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

char *lsm_metric_format_memory_gb(uint64_t bytes, char *buffer, size_t size)
{
    return infiltratr_format_memory_gb(bytes, buffer, size);
}

char *lsm_metric_format_disk_capacity(uint64_t bytes, char *buffer, size_t size)
{
    return infiltratr_format_disk_capacity(bytes, buffer, size);
}

char *lsm_metric_format_network(long double bytes, bool use_bits,
                                bool per_second, char *buffer, size_t size)
{
    return infiltratr_format_network(bytes, use_bits, per_second, buffer, size);
}

char *lsm_metric_format_network_pair(long double send_bytes,
                                     long double receive_bytes, bool use_bits,
                                     char *buffer, size_t size)
{
    return infiltratr_format_network_pair(send_bytes, receive_bytes, use_bits,
                                          buffer, size);
}

char *lsm_metric_format_link_speed_mbps(double megabits_per_second,
                                        char *buffer, size_t size)
{
    return infiltratr_format_link_speed_mbps(megabits_per_second, buffer, size);
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
