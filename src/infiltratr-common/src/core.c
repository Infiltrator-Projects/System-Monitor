// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file core.c
 * @brief Dependency-free implementation of shared C project primitives.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "infiltratr/core.h"
#include "infiltratr/compiler.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool populated(const char *text)
{
    return text && text[0] != '\0';
}

bool infiltratr_project_info_is_valid(const InfiltratrProjectInfo *info)
{
    return info && info->struct_size >= sizeof(*info) &&
           info->abi_version == INFILTRATR_PROJECT_INFO_ABI &&
           populated(info->program_name) && populated(info->executable_name) &&
           populated(info->application_id) && populated(info->version) &&
           populated(info->source_id) && populated(info->build_profile) &&
           populated(info->author) && populated(info->website) &&
           populated(info->license_id) && populated(info->comments) &&
           populated(info->icon_name) && populated(info->copyright_text);
}

static int print_field(FILE *stream, const char *name, const char *value)
{
    if (fprintf(stream, "%s=", name) < 0) return -1;
    for (const unsigned char *cursor = (const unsigned char *)value;
         *cursor; cursor++) {
        const int character = *cursor < 32U || *cursor == 127U ? ' ' : *cursor;
        if (fputc(character, stream) == EOF) return -1;
    }
    return fputc('\n', stream) == EOF ? -1 : 0;
}

int infiltratr_project_info_print(FILE *stream,
                                  const InfiltratrProjectInfo *info)
{
    if (!stream || !infiltratr_project_info_is_valid(info)) return -1;
    if (print_field(stream, "name", info->program_name) != 0 ||
        print_field(stream, "version", info->version) != 0 ||
        print_field(stream, "common-library",
                    "infiltratr-common-" INFILTRATR_COMMON_VERSION) != 0 ||
        print_field(stream, "source-id", info->source_id) != 0 ||
        print_field(stream, "build-profile", info->build_profile) != 0 ||
        print_field(stream, "application-id", info->application_id) != 0 ||
        print_field(stream, "author", info->author) != 0 ||
        print_field(stream, "website", info->website) != 0 ||
        print_field(stream, "license", info->license_id) != 0)
        return -1;
    return ferror(stream) == 0 ? 0 : -1;
}

void infiltratr_copy_string(char *destination, size_t size,
                            const char *source)
{
    if (INFILTRATR_UNLIKELY(!destination || size == 0U)) return;
    if (!source) source = "";

    size_t length = 0U;
    while (length + 1U < size && source[length] != '\0') length++;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

void infiltratr_trim(char *text)
{
    if (!text) return;

    char *start = text;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;

    const size_t length = (size_t)(end - start);
    if (start != text && length > 0U) memmove(text, start, length);
    text[length] = '\0';
}

void infiltratr_trim_line_end(char *text)
{
    if (!text) return;
    size_t length = strlen(text);
    while (length > 0U &&
           (text[length - 1U] == '\n' || text[length - 1U] == '\r'))
        text[--length] = '\0';
}

bool infiltratr_string_equal(const char *left, const char *right)
{
    if (left == right) return true;
    return left && right && strcmp(left, right) == 0;
}

bool infiltratr_string_starts_with(const char *text, const char *prefix)
{
    if (!text || !prefix) return false;
    const size_t prefix_length = strlen(prefix);
    return strncmp(text, prefix, prefix_length) == 0;
}

bool infiltratr_string_ends_with(const char *text, const char *suffix)
{
    if (!text || !suffix) return false;
    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length &&
           strcmp(text + text_length - suffix_length, suffix) == 0;
}

bool infiltratr_parse_u64(const char *text, unsigned int base,
                          uint64_t *value)
{
    if (!text || !value || base == 1U || base > 36U) return false;
    while (*text && isspace((unsigned char)*text)) text++;
    if (*text == '+' || *text == '-' || *text == '\0') return false;

    errno = 0;
    char *end = NULL;
    const unsigned long long parsed = strtoull(text, &end, (int)base);
    if (errno != 0 || end == text || parsed > (unsigned long long)UINT64_MAX)
        return false;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return false;
    *value = (uint64_t)parsed;
    return true;
}

bool infiltratr_parse_double(const char *text, double *value)
{
    if (!text || !value) return false;
    while (*text && isspace((unsigned char)*text)) text++;
    errno = 0;
    char *end = NULL;
    const double parsed = strtod(text, &end);
    if (errno != 0 || end == text || !isfinite(parsed)) return false;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return false;
    *value = parsed;
    return true;
}

double infiltratr_clamp_double(double value, double lower, double upper)
{
    if (isnan(value) || isnan(lower) || isnan(upper) || lower > upper)
        return value;
    if (value < lower) return lower;
    return value > upper ? upper : value;
}

uint64_t infiltratr_u64_add_saturating(uint64_t left, uint64_t right)
{
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
}

uint64_t infiltratr_u64_multiply_saturating(uint64_t left, uint64_t right)
{
    return left != 0U && right > UINT64_MAX / left
        ? UINT64_MAX : left * right;
}

double infiltratr_percent_u64(uint64_t part, uint64_t whole)
{
    if (whole == 0U) return 0.0;
    const long double percentage =
        100.0L * (long double)part / (long double)whole;
    if (percentage <= 0.0L) return 0.0;
    return percentage >= 100.0L ? 100.0 : (double)percentage;
}

bool infiltratr_u64_counter_rate(uint64_t current, uint64_t previous,
                                 long double units_per_count,
                                 double elapsed_seconds, double *rate)
{
    if (!rate) return false;
    *rate = 0.0;
    if (current < previous || units_per_count < 0.0L ||
        !isfinite(units_per_count) || elapsed_seconds <= 0.0 ||
        !isfinite(elapsed_seconds))
        return false;

    const long double calculated =
        (long double)(current - previous) * units_per_count /
        (long double)elapsed_seconds;
    if (!isfinite(calculated) || calculated > (long double)DBL_MAX)
        return false;
    *rate = (double)calculated;
    return true;
}

static char *format_binary(long double value, const char *suffix,
                           char *buffer, size_t buffer_size)
{
    static const char *const units[] = {"B", "KB", "MB", "GB", "TB"};
    if (!buffer || buffer_size == 0U) return buffer;

    size_t unit = 0U;
    while (value >= 1024.0L && unit + 1U < INFILTRATR_ARRAY_LENGTH(units)) {
        value /= 1024.0L;
        unit++;
    }
    (void)snprintf(buffer, buffer_size,
                   value >= 100.0L || unit == 0U ? "%.0Lf %s%s" :
                                                   "%.1Lf %s%s",
                   value, units[unit], suffix);
    return buffer;
}

char *infiltratr_format_bytes(uint64_t bytes, char *buffer,
                              size_t buffer_size)
{
    return format_binary((long double)bytes, "", buffer, buffer_size);
}

char *infiltratr_format_rate(double bytes_per_second, char *buffer,
                             size_t buffer_size)
{
    if (!isfinite(bytes_per_second) || bytes_per_second < 0.0)
        bytes_per_second = 0.0;
    return format_binary((long double)bytes_per_second, "/s", buffer,
                         buffer_size);
}
