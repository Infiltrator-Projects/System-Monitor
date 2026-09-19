// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file numeric_io.h
 * @brief Locale-independent numeric persistence helpers.
 *
 * Human-facing presentation may follow the active locale. Machine-readable
 * preferences, history and CSV files must not: decimal punctuation is part of
 * their on-disk grammar. These helpers format fixed-point values without the C
 * locale and accept the legacy decimal-comma files emitted before 1.0.39.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_NUMERIC_IO_H
#define LINUX_SYSTEM_MONITOR_NUMERIC_IO_H

#include <infiltratr/core.h>

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * Format a finite floating-point value for machine-readable storage.
 *
 * @param buffer Destination buffer.
 * @param size Destination-buffer capacity in bytes.
 * @param value Finite value to format.
 * @param precision Number of digits after the decimal point.
 * @return true when the complete representation was written.
 */
static inline bool numeric_io_format_fixed(char *buffer, size_t size,
                                            double value, unsigned precision)
{
    if (!buffer || size == 0U || !isfinite(value) || precision > 9U) return false;
    uint64_t scale = 1U;
    for (unsigned index = 0U; index < precision; index++) scale *= 10U;
    const bool negative = signbit(value) && value != 0.0;
    const long double magnitude = fabsl((long double)value);
    const long double whole_ld = floorl(magnitude);
    if (whole_ld > (long double)UINT64_MAX) return false;
    uint64_t whole = (uint64_t)whole_ld;
    uint64_t fraction = precision > 0U
        ? (uint64_t)llroundl((magnitude - whole_ld) * (long double)scale)
        : 0U;
    if (precision > 0U && fraction >= scale) {
        if (whole == UINT64_MAX) return false;
        whole++;
        fraction = 0U;
    }
    const int written = precision > 0U
        ? snprintf(buffer, size, "%s%llu.%0*llu",
                   negative ? "-" : "",
                   (unsigned long long)whole, (int)precision,
                   (unsigned long long)fraction)
        : snprintf(buffer, size, "%s%llu",
                   negative ? "-" : "", (unsigned long long)whole);
    return written >= 0 && (size_t)written < size;
}

/**
 * Parse a persisted floating-point value, accepting the historical comma form.
 *
 * @param text Persisted numeric text.
 * @param value Receives the parsed finite value.
 * @param legacy_decimal_comma Optional flag set when comma recovery was used.
 * @return true when the entire value was parsed successfully.
 */
static inline bool numeric_io_parse_persisted_double(
    const char *text, double *value, bool *legacy_decimal_comma)
{
    if (legacy_decimal_comma) *legacy_decimal_comma = false;
    if (!text || !value) return false;
    if (infiltratr_parse_double(text, value) && isfinite(*value)) return true;
    const char *comma = strchr(text, ',');
    if (!comma || strchr(text, '.') || strchr(comma + 1, ',')) return false;
    const size_t length = strlen(text);
    if (length == 0U || length >= 128U) return false;
    char canonical[128];
    memcpy(canonical, text, length + 1U);
    canonical[(size_t)(comma - text)] = '.';
    if (!infiltratr_parse_double(canonical, value) || !isfinite(*value))
        return false;
    if (legacy_decimal_comma) *legacy_decimal_comma = true;
    return true;
}

/**
 * Parse a persisted floating-point value and enforce an inclusive range.
 *
 * @param text Persisted numeric text.
 * @param minimum Smallest accepted value.
 * @param maximum Largest accepted value.
 * @param value Receives the parsed value.
 * @param legacy_decimal_comma Optional flag set when comma recovery was used.
 * @return true when parsing succeeded and the value is within range.
 */
static inline bool numeric_io_parse_persisted_double_range(
    const char *text, double minimum, double maximum, double *value,
    bool *legacy_decimal_comma)
{
    double parsed = 0.0;
    bool legacy = false;
    if (!numeric_io_parse_persisted_double(text, &parsed, &legacy) ||
        parsed < minimum || parsed > maximum)
        return false;
    *value = parsed;
    if (legacy_decimal_comma) *legacy_decimal_comma = legacy;
    return true;
}

#endif
