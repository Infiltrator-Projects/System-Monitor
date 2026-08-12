// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file memory_accounting.c
 * @brief Overflow-safe Linux meminfo parsing and byte accounting.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "memory_accounting.h"

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_quantity(char *line, char **key, uint64_t *bytes)
{
    if (!line || !key || !bytes) return false;
    char *separator = strchr(line, ':');
    if (!separator) return false;
    *separator = '\0';
    lsm_trim(line);

    char *cursor = separator + 1;
    while (*cursor && isspace((unsigned char)*cursor)) cursor++;
    if (!isdigit((unsigned char)*cursor)) return false;

    errno = 0;
    char *end = NULL;
    const unsigned long long parsed = strtoull(cursor, &end, 10);
    if (errno != 0 || end == cursor) return false;
    while (*end && isspace((unsigned char)*end)) end++;

    uint64_t multiplier = 1U;
    if (*end) {
        char *unit_end = end;
        while (*unit_end && !isspace((unsigned char)*unit_end)) unit_end++;
        const char saved = *unit_end;
        *unit_end = '\0';
        if (strcmp(end, "kB") == 0)
            multiplier = 1024U;
        else {
            *unit_end = saved;
            return false;
        }
        *unit_end = saved;
        while (*unit_end && isspace((unsigned char)*unit_end)) unit_end++;
        if (*unit_end) return false;
    }

    *key = line;
    *bytes = lsm_u64_multiply_saturating((uint64_t)parsed, multiplier);
    return true;
}

bool lsm_memory_accounting_read(const char *path, LsmMemoryInfo *memory,
                                bool refresh_details)
{
    if (!path || !memory) return false;
    FILE *file = fopen(path, "r");
    if (!file) return false;

    uint64_t available = 0U;
    uint64_t cached = 0U;
    uint64_t reclaimable = 0U;
    uint64_t shared = 0U;
    uint64_t corrupted = 0U;
    uint64_t committed = 0U;
    uint64_t commit_limit = 0U;
    uint64_t nonreclaimable = 0U;
    uint64_t page_tables = 0U;
    bool have_available = false;
    bool have_cached = false;
    bool have_reclaimable = false;
    bool have_shared = false;
    bool have_corrupted = false;
    bool have_committed = false;
    bool have_commit_limit = false;
    bool have_nonreclaimable = false;
    bool have_page_tables = false;

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char *key = NULL;
        uint64_t bytes = 0U;
        if (!parse_quantity(line, &key, &bytes)) continue;
        if (strcmp(key, "MemAvailable") == 0) {
            available = bytes;
            have_available = true;
        } else if (strcmp(key, "Committed_AS") == 0) {
            committed = bytes;
            have_committed = true;
        } else if (strcmp(key, "CommitLimit") == 0) {
            commit_limit = bytes;
            have_commit_limit = true;
        } else if (refresh_details && strcmp(key, "Cached") == 0) {
            cached = bytes;
            have_cached = true;
        } else if (refresh_details && strcmp(key, "SReclaimable") == 0) {
            reclaimable = bytes;
            have_reclaimable = true;
        } else if (refresh_details && strcmp(key, "Shmem") == 0) {
            shared = bytes;
            have_shared = true;
        } else if (refresh_details && strcmp(key, "SUnreclaim") == 0) {
            nonreclaimable = bytes;
            have_nonreclaimable = true;
        } else if (refresh_details && strcmp(key, "PageTables") == 0) {
            page_tables = bytes;
            have_page_tables = true;
        } else if (refresh_details &&
                   strcmp(key, "HardwareCorrupted") == 0) {
            corrupted = bytes;
            have_corrupted = true;
        }
    }
    fclose(file);

    if (have_available) memory->available_bytes = available;
    if (have_committed) memory->committed_bytes = committed;
    if (have_commit_limit) memory->commit_limit_bytes = commit_limit;
    if (refresh_details) {
        if (have_cached || have_reclaimable || have_shared) {
            const uint64_t combined =
                lsm_u64_add_saturating(cached, reclaimable);
            memory->cached_bytes = combined >= shared
                ? combined - shared : 0U;
        }
        memory->kernel_reclaimable_bytes =
            have_reclaimable ? reclaimable : 0U;
        memory->kernel_nonreclaimable_bytes =
            have_nonreclaimable ? nonreclaimable : 0U;
        memory->page_tables_bytes = have_page_tables ? page_tables : 0U;
        memory->hardware_corrupted_bytes =
            have_corrupted ? corrupted : 0U;
    }
    return have_available;
}
