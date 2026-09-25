// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file cpu_accounting.c
 * @brief Defensive parser for Linux scheduler accounting.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
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

static bool parse_cpu_row(const char *line, LsmCpuCounters *counter)
{
    if (!line || !counter || !lsm_string_starts_with(line, "cpu"))
        return false;
    const char *cursor = line + 3U;
    while (lsm_ascii_is_digit((unsigned char)*cursor)) cursor++;
    if (*cursor && !lsm_ascii_is_space((unsigned char)*cursor)) return false;

    uint64_t fields[8] = {0U};
    size_t count = 0U;
    while (count < 8U && lsm_parse_u64_token(&cursor, 10U, &fields[count])) count++;
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
    if (!lsm_string_starts_with(line, name) ||
        !lsm_ascii_is_space((unsigned char)line[length])) return false;
    const char *cursor = line + length;
    return lsm_parse_u64_token(&cursor, 10U, value);
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
        if (lsm_string_starts_with(line, "cpu") &&
            (lsm_ascii_is_space((unsigned char)line[3]) ||
             lsm_ascii_is_digit((unsigned char)line[3]))) {
            LsmCpuCounters counter;
            if (!parse_cpu_row(line, &counter)) continue;
            if (lsm_ascii_is_space((unsigned char)line[3])) {
                sample->cpus[0] = counter;
                if (sample->cpu_count == 0U) sample->cpu_count = 1U;
                aggregate_found = true;
            } else {
                const char *cursor = line + 3U;
                uint64_t parsed_index = 0U;
                if (!lsm_parse_u64_token(&cursor, 10U, &parsed_index) ||
                    !lsm_ascii_is_space((unsigned char)*cursor) ||
                    parsed_index >= LSM_MAX_CPUS)
                    continue;
                const size_t index = (size_t)parsed_index;
                sample->cpus[index + 1U] = counter;
                if (sample->cpu_count < index + 2U)
                    sample->cpu_count = index + 2U;
            }
        } else if (lsm_string_starts_with(line, "intr")) {
            (void)parse_named_counter(line, "intr", &sample->interrupts);
        } else if (lsm_string_starts_with(line, "ctxt")) {
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
    char *text = NULL;
    size_t length = 0U;
    if (lsm_read_text_file_alloc(path, &text, &length) != INFILTRATR_IO_OK)
        return false;
    const bool valid_text = memchr(text, '\0', length) == NULL;
    const bool okay = valid_text && lsm_cpu_accounting_parse(text, sample);
    free(text);
    return okay;
}

static double counter_percent(uint64_t current, uint64_t previous,
                              uint64_t total_delta)
{
    uint64_t delta = 0U;
    if (total_delta == 0U ||
        !lsm_u64_counter_delta(current, previous, &delta))
        return 0.0;
    return lsm_percent_u64(delta, total_delta);
}

void lsm_cpu_accounting_apply(LsmCpuInfo *cpu,
                              LsmCpuAccountingState *state,
                              const LsmCpuAccountingSample *sample,
                              bool initial, double elapsed_seconds)
{
    if (!cpu || !state || !sample || sample->cpu_count == 0U) return;
    const size_t usable = sample->cpu_count < (size_t)cpu->logical_cores + 1U
        ? sample->cpu_count : (size_t)cpu->logical_cores + 1U;
    /* A reset must not retain the preceding busy sample. Idle includes
     * iowait, which Linux can decrease; reject that interval instead of
     * interpreting the failed delta as zero idle (100% busy). */
    cpu->usage_percent = 0.0;
    cpu->user_percent = 0.0;
    cpu->kernel_percent = 0.0;
    memset(cpu->core_usage, 0, sizeof(cpu->core_usage));
    for (size_t index = 0U;
         index < usable && index < LSM_MAX_CPUS + 1U; index++) {
        const LsmCpuCounters *current = &sample->cpus[index];
        const LsmCpuCounters *previous = &state->previous[index];
        uint64_t total_delta = 0U;
        uint64_t idle_delta = 0U;
        if (!initial &&
            lsm_u64_counter_delta(current->total, previous->total,
                                  &total_delta) &&
            lsm_u64_counter_delta(current->idle, previous->idle,
                                  &idle_delta)) {
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
    cpu->interrupts_per_sec_available = false;
    cpu->context_switches_per_sec_available = false;
    if (!initial && state->scheduler_events_initialized &&
        isfinite(elapsed_seconds) && elapsed_seconds > 0.0) {
        cpu->interrupts_per_sec_available = lsm_u64_counter_rate(
            sample->interrupts, state->previous_interrupt_count, 1.0L,
            elapsed_seconds, &cpu->interrupts_per_sec);
        cpu->context_switches_per_sec_available = lsm_u64_counter_rate(
            sample->context_switches, state->previous_context_switch_count,
            1.0L, elapsed_seconds, &cpu->context_switches_per_sec);
    }
    state->previous_interrupt_count = sample->interrupts;
    state->previous_context_switch_count = sample->context_switches;
    state->scheduler_events_initialized = true;
}
