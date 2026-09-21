// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pressure.c
 * @brief Linux Pressure Stall Information parser and collector.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "pressure.h"

#include "common.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    double avg10;
    double avg60;
    double avg300;
    uint64_t total_us;
    bool have_avg10;
    bool have_avg60;
    bool have_avg300;
    bool have_total;
} PressureLine;

static bool parse_pressure_line(char *line, const char *kind,
                                PressureLine *parsed)
{
    if (!line || !kind || !parsed) return false;
    *parsed = (PressureLine){0};

    char *save = NULL;
    char *token = strtok_r(line, " \t", &save);
    if (!token || strcmp(token, kind) != 0) return false;

    while ((token = strtok_r(NULL, " \t", &save))) {
        char *equals = strchr(token, '=');
        if (!equals || equals == token || !equals[1]) continue;
        *equals = '\0';
        const char *value = equals + 1;

        if (strcmp(token, "avg10") == 0) {
            double number = 0.0;
            if (!infiltratr_parse_double_range(value, 0.0, 100.0, &number))
                return false;
            parsed->avg10 = number;
            parsed->have_avg10 = true;
        } else if (strcmp(token, "avg60") == 0) {
            double number = 0.0;
            if (!infiltratr_parse_double_range(value, 0.0, 100.0, &number))
                return false;
            parsed->avg60 = number;
            parsed->have_avg60 = true;
        } else if (strcmp(token, "avg300") == 0) {
            double number = 0.0;
            if (!infiltratr_parse_double_range(value, 0.0, 100.0, &number))
                return false;
            parsed->avg300 = number;
            parsed->have_avg300 = true;
        } else if (strcmp(token, "total") == 0) {
            uint64_t total = 0U;
            if (!infiltratr_parse_u64(value, 10U, &total)) return false;
            parsed->total_us = total;
            parsed->have_total = true;
        }
    }

    return parsed->have_avg10 && parsed->have_avg60 &&
           parsed->have_avg300 && parsed->have_total;
}

bool lsm_pressure_parse(const char *text, LsmPressureInfo *pressure)
{
    if (!pressure) return false;
    *pressure = (LsmPressureInfo){0};
    if (!text || !*text) return false;

    char *copy = strdup(text);
    if (!copy) return false;

    LsmPressureInfo result = {0};
    bool have_some = false;
    bool have_full = false;
    char *save = NULL;
    for (char *line = strtok_r(copy, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        lsm_trim(line);
        if (!*line) continue;

        PressureLine parsed = {0};
        if (lsm_string_starts_with(line, "some ")) {
            if (!parse_pressure_line(line, "some", &parsed)) {
                free(copy);
                return false;
            }
            result.some_avg10 = parsed.avg10;
            result.some_avg60 = parsed.avg60;
            result.some_avg300 = parsed.avg300;
            result.some_total_us = parsed.total_us;
            have_some = true;
        } else if (lsm_string_starts_with(line, "full ")) {
            if (!parse_pressure_line(line, "full", &parsed)) {
                free(copy);
                return false;
            }
            result.full_avg10 = parsed.avg10;
            result.full_avg60 = parsed.avg60;
            result.full_avg300 = parsed.avg300;
            result.full_total_us = parsed.total_us;
            have_full = true;
        }
    }
    free(copy);

    if (!have_some) return false;
    result.available = true;
    result.full_available = have_full;
    *pressure = result;
    return true;
}

bool lsm_pressure_read(const char *path, LsmPressureInfo *pressure)
{
    if (!pressure) return false;
    *pressure = (LsmPressureInfo){0};
    if (!path || !*path) return false;

    char *text = NULL;
    size_t length = 0U;
    if (lsm_read_text_file_alloc(path, &text, &length) != INFILTRATR_IO_OK)
        return false;
    if (!text || memchr(text, '\0', length) != NULL) {
        free(text);
        return false;
    }

    const bool okay = lsm_pressure_parse(text, pressure);
    free(text);
    return okay;
}
