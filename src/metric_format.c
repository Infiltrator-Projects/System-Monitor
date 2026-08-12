// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file metric_format.c
 * @brief Standard optional-metric formatting implementation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "metric_format.h"

#include <math.h>
#include <stdio.h>

static char *unavailable(char *buffer, size_t size)
{
    if (buffer && size > 0U) (void)snprintf(buffer, size, "N/A");
    return buffer;
}

char *lsm_metric_format_memory_gb(uint64_t bytes, char *buffer, size_t size)
{
    if (!buffer || size == 0U) return buffer;
    (void)snprintf(buffer, size, "%.1Lf GB",
                   (long double)bytes / 1073741824.0L);
    return buffer;
}

char *lsm_metric_format_disk_capacity(uint64_t bytes, char *buffer, size_t size)
{
    static const char *const units[] = {"B", "KB", "MB", "GB", "TB"};
    if (!buffer || size == 0U) return buffer;
    size_t unit = 0U;
    uint64_t divisor = 1U;
    while (bytes / divisor >= 1024U &&
           unit + 1U < sizeof(units) / sizeof(units[0])) {
        divisor *= 1024U;
        unit++;
    }
    const long double value = (long double)bytes / (long double)divisor;
    (void)snprintf(buffer, size,
                   value >= 100.0L || unit == 0U ? "%.0Lf %s" : "%.1Lf %s",
                   value, units[unit]);
    return buffer;
}

char *lsm_metric_format_network(long double bytes, bool use_bits,
                                bool per_second, char *buffer, size_t size)
{
    static const char *const byte_units[] = {"KB", "MB", "GB", "TB"};
    static const char *const bit_units[] = {"Kb", "Mb", "Gb", "Tb"};
    if (!buffer || size == 0U) return buffer;
    if (!isfinite(bytes) || bytes < 0.0L) bytes = 0.0L;

    long double value = use_bits ? bytes * 8.0L : bytes;
    size_t unit = 0U;
    if (!isfinite(value) || value < 1024.0L) {
        value = 0.0L;
    } else {
        value /= 1024.0L;
        while (value >= 1024.0L &&
               unit + 1U < sizeof(byte_units) / sizeof(byte_units[0])) {
            value /= 1024.0L;
            unit++;
        }
    }
    const char *const *units = use_bits ? bit_units : byte_units;
    (void)snprintf(buffer, size, "%.1Lf %s%s", value, units[unit],
                   per_second ? "/s" : "");
    return buffer;
}

char *lsm_metric_format_network_pair(long double send_bytes,
                                     long double receive_bytes, bool use_bits,
                                     char *buffer, size_t size)
{
    static const char *const byte_units[] = {"KB", "MB", "GB", "TB"};
    static const char *const bit_units[] = {"Kb", "Mb", "Gb", "Tb"};
    if (!buffer || size == 0U) return buffer;
    if (!isfinite(send_bytes) || send_bytes < 0.0L) send_bytes = 0.0L;
    if (!isfinite(receive_bytes) || receive_bytes < 0.0L) receive_bytes = 0.0L;

    long double send = use_bits ? send_bytes * 8.0L : send_bytes;
    long double receive = use_bits ? receive_bytes * 8.0L : receive_bytes;
    long double maximum = fmaxl(send, receive);
    size_t unit = 0U;
    if (!isfinite(maximum) || maximum < 1024.0L) {
        send = 0.0L;
        receive = 0.0L;
    } else {
        send /= 1024.0L;
        receive /= 1024.0L;
        maximum /= 1024.0L;
        while (maximum >= 1024.0L &&
               unit + 1U < sizeof(byte_units) / sizeof(byte_units[0])) {
            send /= 1024.0L;
            receive /= 1024.0L;
            maximum /= 1024.0L;
            unit++;
        }
    }
    const char *const *units = use_bits ? bit_units : byte_units;
    (void)snprintf(buffer, size, "S:%.1Lf R:%.1Lf %s/s", send, receive,
                   units[unit]);
    return buffer;
}

char *lsm_metric_format_link_speed_mbps(double megabits_per_second,
                                        char *buffer, size_t size)
{
    static const char *const units[] = {"Kb/s", "Mb/s", "Gb/s", "Tb/s"};
    if (!buffer || size == 0U) return buffer;
    if (!isfinite(megabits_per_second) || megabits_per_second <= 0.0)
        return unavailable(buffer, size);

    /* Linux reports negotiated link speed in decimal Mb/s. Convert that
     * reported rate back to bits/s first, then apply the application's
     * deliberately 1024-based network display units. This avoids treating a
     * decimal source unit as though it were already binary. */
    long double value =
        (long double)megabits_per_second * 1000000.0L / 1024.0L;
    size_t unit = 0U;
    while (value >= 1024.0L &&
           unit + 1U < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0L;
        unit++;
    }

    /* Keep useful decimal detail, but never promote into a unit whose value
     * would begin with 0.xxx; the loop above promotes only at >= 1024. */
    (void)snprintf(buffer, size, "%.2Lf %s", value, units[unit]);
    return buffer;
}

char *lsm_metric_format_percent(bool available, double value,
                                char *buffer, size_t size)
{
    if (!available || !isfinite(value)) return unavailable(buffer, size);
    value = fmin(100.0, fmax(0.0, value));
    (void)snprintf(buffer, size, "%.0f%%", value);
    return buffer;
}

char *lsm_metric_format_mhz(bool available, double value,
                            char *buffer, size_t size)
{
    if (!available || !isfinite(value)) return unavailable(buffer, size);
    (void)snprintf(buffer, size, "%.0f MHz", value);
    return buffer;
}

char *lsm_metric_format_celsius(bool available, double value,
                                char *buffer, size_t size)
{
    if (!available || !isfinite(value)) return unavailable(buffer, size);
    (void)snprintf(buffer, size, "%.0f °C", value);
    return buffer;
}

char *lsm_metric_format_watts(bool available, double value,
                              char *buffer, size_t size)
{
    if (!available || !isfinite(value)) return unavailable(buffer, size);
    (void)snprintf(buffer, size, "%.1f W", value);
    return buffer;
}
