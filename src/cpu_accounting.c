// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file cpu_accounting.c
 * @brief Defensive parser for Linux scheduler accounting.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "cpu_accounting.h"

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_u64_field(const char **cursor, uint64_t *value)
{
    return lsm_parse_u64_token(cursor, 10U, value);
}

static bool parse_cpu_row(const char *line, LsmCpuCounters *counter)
{
    if (!line || !counter || strncmp(line, "cpu", 3U) != 0) return false;
    const char *cursor = line + 3U;
    while (isdigit((unsigned char)*cursor)) cursor++;
    if (*cursor && !isspace((unsigned char)*cursor)) return false;

    uint64_t fields[8] = {0U};
    size_t count = 0U;
    while (count < 8U && parse_u64_field(&cursor, &fields[count])) count++;
    if (count < 4U) return false;

    memset(counter, 0, sizeof(*counter));
    for (size_t index = 0U; index < count; index++)
        counter->total = lsm_u64_add_saturating(counter->total, fields[index]);
    counter->idle = lsm_u64_add_saturating(fields[3], fields[4]);
    counter->user = lsm_u64_add_saturating(fields[0], fields[1]);
    counter->kernel = fields[2];
    counter->kernel = lsm_u64_add_saturating(counter->kernel, fields[5]);
    counter->kernel = lsm_u64_add_saturating(counter->kernel, fields[6]);
    return true;
}

static bool parse_named_counter(const char *line, const char *name,
                                uint64_t *value)
{
    const size_t length = strlen(name);
    if (strncmp(line, name, length) != 0 ||
        !isspace((unsigned char)line[length])) return false;
    const char *cursor = line + length;
    return parse_u64_field(&cursor, value);
}

bool lsm_cpu_accounting_parse(const char *text,
                              LsmCpuAccountingSample *sample)
{
    if (!text || !sample) return false;
    memset(sample, 0, sizeof(*sample));
    char *copy = strdup(text);
    if (!copy) return false;

    bool aggregate_found = false;
    char *save = NULL;
    for (char *line = strtok_r(copy, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "cpu", 3U) == 0 &&
            (isspace((unsigned char)line[3]) ||
             isdigit((unsigned char)line[3]))) {
            LsmCpuCounters counter;
            if (!parse_cpu_row(line, &counter)) continue;
            if (isspace((unsigned char)line[3])) {
                sample->cpus[0] = counter;
                if (sample->cpu_count == 0U) sample->cpu_count = 1U;
                aggregate_found = true;
            } else {
                const char *cursor = line + 3U;
                errno = 0;
                char *end = NULL;
                const unsigned long index = strtoul(cursor, &end, 10);
                if (errno || end == cursor ||
                    !isspace((unsigned char)*end) || index >= LSM_MAX_CPUS)
                    continue;
                sample->cpus[index + 1U] = counter;
                if (sample->cpu_count < index + 2U)
                    sample->cpu_count = index + 2U;
            }
        } else if (strncmp(line, "intr", 4U) == 0) {
            (void)parse_named_counter(line, "intr", &sample->interrupts);
        } else if (strncmp(line, "ctxt", 4U) == 0) {
            (void)parse_named_counter(line, "ctxt", &sample->context_switches);
        }
    }
    free(copy);
    return aggregate_found;
}

bool lsm_cpu_accounting_read(const char *path,
                             LsmCpuAccountingSample *sample)
{
    if (!path || !*path || !sample) return false;
    FILE *file = fopen(path, "r");
    if (!file) return false;
    char *text = NULL;
    size_t capacity = 0U;
    size_t length = 0U;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), file)) {
        const size_t chunk = strlen(buffer);
        if (length > SIZE_MAX - chunk - 1U ||
            !lsm_array_reserve((void **)&text, &capacity, sizeof(*text),
                               length + chunk + 1U, 8192U)) {
            free(text);
            fclose(file);
            return false;
        }
        memcpy(text + length, buffer, chunk);
        length += chunk;
        text[length] = '\0';
    }
    fclose(file);
    if (!text) return false;
    const bool okay = lsm_cpu_accounting_parse(text, sample);
    free(text);
    return okay;
}

static double counter_percent(uint64_t current, uint64_t previous,
                              uint64_t total_delta)
{
    if (current < previous || total_delta == 0U) return 0.0;
    return lsm_percent_u64(current - previous, total_delta);
}

void lsm_cpu_accounting_apply(LsmCpuInfo *cpu,
                              LsmCpuAccountingState *state,
                              const LsmCpuAccountingSample *sample,
                              bool initial, double elapsed_seconds)
{
    if (!cpu || !state || !sample || sample->cpu_count == 0U) return;
    const size_t usable = sample->cpu_count < (size_t)cpu->logical_cores + 1U
        ? sample->cpu_count : (size_t)cpu->logical_cores + 1U;
    for (size_t index = 0U; index < usable; index++) {
        const LsmCpuCounters *current = &sample->cpus[index];
        const LsmCpuCounters *previous = &state->previous[index];
        if (!initial && current->total >= previous->total) {
            const uint64_t total_delta = current->total - previous->total;
            const uint64_t idle_delta = current->idle >= previous->idle
                ? current->idle - previous->idle : total_delta;
            const double usage = idle_delta <= total_delta
                ? lsm_percent_u64(total_delta - idle_delta, total_delta) : 0.0;
            if (index == 0U) {
                cpu->usage_percent = usage;
                cpu->user_percent = counter_percent(
                    current->user, previous->user, total_delta);
                cpu->kernel_percent = counter_percent(
                    current->kernel, previous->kernel, total_delta);
            } else {
                cpu->core_usage[index - 1U] = usage;
            }
        }
        state->previous[index] = *current;
    }

    cpu->interrupt_count = sample->interrupts;
    cpu->context_switch_count = sample->context_switches;
    if (!initial && state->scheduler_events_initialized &&
        isfinite(elapsed_seconds) && elapsed_seconds > 0.0) {
        (void)lsm_u64_counter_rate(
            sample->interrupts, state->previous_interrupt_count, 1.0L,
            elapsed_seconds, &cpu->interrupts_per_sec);
        (void)lsm_u64_counter_rate(
            sample->context_switches, state->previous_context_switch_count,
            1.0L, elapsed_seconds, &cpu->context_switches_per_sec);
    }
    state->previous_interrupt_count = sample->interrupts;
    state->previous_context_switch_count = sample->context_switches;
    state->scheduler_events_initialized = true;
}
