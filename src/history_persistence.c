// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file history_persistence.c
 * @brief Durable App History snapshot serialization and publication.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "history_persistence.h"

#include "atomic_file.h"
#include "common.h"

#include <infiltratr/format.h>

#include <errno.h>
#include <glib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct LsmHistorySaveCoordinator {
    pthread_mutex_t write_mutex;
    atomic_uint references;
    atomic_uint latest_scheduled_generation;
    atomic_uint latest_written_generation;
};

typedef struct {
    char *key;
    char *name;
    char *user;
    char *identity;
    double cpu_seconds;
    double active_seconds;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t peak_rss_bytes;
    int64_t first_seen;
    int64_t last_seen;
} LsmHistoryPersistEntry;

struct LsmHistorySaveRequest {
    char config_dir[LSM_PATH_LEN];
    char path[LSM_PATH_LEN];
    LsmHistoryPersistEntry *entries;
    char *strings;
    size_t count;
    unsigned generation;
    struct LsmHistorySaveCoordinator *coordinator;
};

struct LsmHistorySaveCoordinator *
lsm_history_persistence_coordinator_create(void)
{
    struct LsmHistorySaveCoordinator *coordinator =
        g_new0(struct LsmHistorySaveCoordinator, 1U);
    if (!coordinator) return NULL;
    if (pthread_mutex_init(&coordinator->write_mutex, NULL) != 0) {
        g_free(coordinator);
        return NULL;
    }
    atomic_init(&coordinator->references, 1U);
    atomic_init(&coordinator->latest_scheduled_generation, 0U);
    atomic_init(&coordinator->latest_written_generation, 0U);
    return coordinator;
}

void lsm_history_persistence_coordinator_retain(
    struct LsmHistorySaveCoordinator *coordinator)
{
    if (coordinator)
        (void)atomic_fetch_add_explicit(
            &coordinator->references, 1U, memory_order_relaxed);
}

void lsm_history_persistence_coordinator_release(
    struct LsmHistorySaveCoordinator *coordinator)
{
    if (!coordinator ||
        atomic_fetch_sub_explicit(&coordinator->references, 1U,
                                  memory_order_acq_rel) != 1U)
        return;
    (void)pthread_mutex_destroy(&coordinator->write_mutex);
    g_free(coordinator);
}

void lsm_history_persistence_note_scheduled(
    struct LsmHistorySaveCoordinator *coordinator, unsigned generation)
{
    if (!coordinator) return;
    atomic_store_explicit(
        &coordinator->latest_scheduled_generation,
        generation, memory_order_release);
}

unsigned lsm_history_persistence_latest_written(
    const struct LsmHistorySaveCoordinator *coordinator)
{
    if (!coordinator) return 0U;
    return atomic_load_explicit(
        &coordinator->latest_written_generation, memory_order_acquire);
}

static bool count_request_strings(
    const LsmHistoryPersistenceRecord *records, size_t count,
    size_t *string_bytes)
{
    if (!string_bytes || (count > 0U && !records)) return false;
    *string_bytes = 0U;
    for (size_t index = 0U; index < count; index++) {
        const char *fields[] = {
            records[index].key,
            records[index].name,
            records[index].user,
            records[index].identity
        };
        for (size_t field = 0U; field < LSM_ARRAY_LENGTH(fields); field++) {
            const size_t length = strlen(fields[field] ? fields[field] : "") + 1U;
            if (!lsm_size_add_checked(*string_bytes, length, string_bytes))
                return false;
        }
    }
    return true;
}

static char *copy_record_string(char *cursor, const char *source)
{
    source = source ? source : "";
    const size_t length = strlen(source) + 1U;
    memcpy(cursor, source, length);
    for (size_t byte = 0U; byte + 1U < length; byte++) {
        if (cursor[byte] == '\t' || cursor[byte] == '\r' ||
            cursor[byte] == '\n')
            cursor[byte] = ' ';
    }
    return cursor + length;
}

LsmHistorySaveRequest *lsm_history_persistence_request_create(
    struct LsmHistorySaveCoordinator *coordinator,
    const char *config_dir, const char *path,
    const LsmHistoryPersistenceRecord *records, size_t count,
    unsigned generation)
{
    if (!config_dir || !path || (count > 0U && !records)) return NULL;

    size_t string_bytes = 0U;
    if (!count_request_strings(records, count, &string_bytes)) return NULL;

    LsmHistorySaveRequest *request = calloc(1U, sizeof(*request));
    if (!request) return NULL;
    if (strlen(config_dir) >= sizeof(request->config_dir) ||
        strlen(path) >= sizeof(request->path)) {
        free(request);
        return NULL;
    }
    lsm_copy_string(
        request->config_dir, sizeof(request->config_dir), config_dir);
    lsm_copy_string(request->path, sizeof(request->path), path);
    request->generation = generation;
    request->count = count;
    request->coordinator = coordinator;
    lsm_history_persistence_coordinator_retain(coordinator);

    if (count > 0U) {
        request->entries = calloc(count, sizeof(*request->entries));
        if (!request->entries) {
            lsm_history_persistence_request_free(request);
            return NULL;
        }
    }
    if (string_bytes > 0U) {
        request->strings = malloc(string_bytes);
        if (!request->strings) {
            lsm_history_persistence_request_free(request);
            return NULL;
        }
    }

    char *cursor = request->strings;
    for (size_t index = 0U; index < count; index++) {
        LsmHistoryPersistEntry *copy = &request->entries[index];
        copy->key = cursor;
        cursor = copy_record_string(cursor, records[index].key);
        copy->name = cursor;
        cursor = copy_record_string(cursor, records[index].name);
        copy->user = cursor;
        cursor = copy_record_string(cursor, records[index].user);
        copy->identity = cursor;
        cursor = copy_record_string(cursor, records[index].identity);
        copy->cpu_seconds = records[index].cpu_seconds;
        copy->active_seconds = records[index].active_seconds;
        copy->read_bytes = records[index].read_bytes;
        copy->write_bytes = records[index].write_bytes;
        copy->peak_rss_bytes = records[index].peak_rss_bytes;
        copy->first_seen = records[index].first_seen;
        copy->last_seen = records[index].last_seen;
    }
    return request;
}

void lsm_history_persistence_request_free(LsmHistorySaveRequest *request)
{
    if (!request) return;
    free(request->strings);
    free(request->entries);
    lsm_history_persistence_coordinator_release(request->coordinator);
    free(request);
}

unsigned lsm_history_persistence_request_generation(
    const LsmHistorySaveRequest *request)
{
    return request ? request->generation : 0U;
}

int lsm_history_persistence_request_write(
    LsmHistorySaveRequest *request, bool *written)
{
    if (written) *written = false;
    if (!request) return EINVAL;

    if (request->coordinator &&
        (request->generation < atomic_load_explicit(
            &request->coordinator->latest_scheduled_generation,
            memory_order_acquire) ||
         request->generation <= atomic_load_explicit(
            &request->coordinator->latest_written_generation,
            memory_order_acquire)))
        return 0;

    const int mkdir_failure =
        lsm_mkdir_parents(request->config_dir, 0700U);
    if (mkdir_failure != 0) return mkdir_failure;

    GString *output = g_string_new("# System-Monitor App History v1\n");
    if (!output) return ENOMEM;
    for (size_t index = 0U; index < request->count; index++) {
        const LsmHistoryPersistEntry *entry = &request->entries[index];
        char cpu_seconds[64];
        char active_seconds[64];
        if (!infiltratr_format_fixed_ascii(
                entry->cpu_seconds, 6U, cpu_seconds, sizeof(cpu_seconds)) ||
            !infiltratr_format_fixed_ascii(
                entry->active_seconds, 6U, active_seconds,
                sizeof(active_seconds))) {
            g_string_free(output, TRUE);
            return ERANGE;
        }
        g_string_append_printf(output,
            "%s\t%s\t%s\t%s\t%s\t%s\t%llu\t%llu\t%llu\t%lld\t%lld\n",
            entry->key, entry->name, entry->user, entry->identity,
            cpu_seconds, active_seconds,
            (unsigned long long)entry->read_bytes,
            (unsigned long long)entry->write_bytes,
            (unsigned long long)entry->peak_rss_bytes,
            (long long)entry->first_seen, (long long)entry->last_seen);
    }

    if (request->coordinator) {
        (void)pthread_mutex_lock(&request->coordinator->write_mutex);
        const unsigned latest = atomic_load_explicit(
            &request->coordinator->latest_scheduled_generation,
            memory_order_acquire);
        const unsigned latest_written = atomic_load_explicit(
            &request->coordinator->latest_written_generation,
            memory_order_acquire);
        if (request->generation < latest ||
            request->generation <= latest_written) {
            (void)pthread_mutex_unlock(&request->coordinator->write_mutex);
            g_string_free(output, TRUE);
            return 0;
        }
    }

    const int failure = lsm_atomic_file_write_bytes(
        request->path, LSM_ATOMIC_FILE_PRIVATE, output->str, output->len);
    if (failure == 0 && request->coordinator)
        atomic_store_explicit(
            &request->coordinator->latest_written_generation,
            request->generation, memory_order_release);
    if (written && failure == 0) *written = true;
    if (request->coordinator)
        (void)pthread_mutex_unlock(&request->coordinator->write_mutex);
    g_string_free(output, TRUE);
    return failure;
}
