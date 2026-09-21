// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file numeric_io.h
 * @brief Backward-compatible parsing for persisted numeric values.
 *
 * Machine-readable writes use Common's locale-independent fixed-point
 * formatter directly. This header retains only the System Monitor-specific
 * recovery of legacy decimal-comma files emitted before 1.0.39.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_NUMERIC_IO_H
#define INFILTRATOR_SYSTEM_MONITOR_NUMERIC_IO_H

#include <infiltratr/core.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
