// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_recorder.c
 * @brief Detached CSV process-recording writer.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_recorder.h"
#include "common.h"

#include <infiltratr/core.h>
#include <infiltratr/format.h>

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct RecordNode {
    time_t timestamp;
    LsmProcessId pid;
    double cpu_percent;
    double memory_percent;
    uint64_t rss_bytes;
    uint64_t read_bytes;
    uint64_t write_bytes;
    unsigned threads;
    struct RecordNode *next;
} RecordNode;

/* Recorder-owned on-disk schema. The writer and plot importer must not drift
 * into different column layouts or locale-dependent numeric conventions. */
static const char recorder_csv_header[] =
    "timestamp,pid,cpu_percent,memory_percent,rss_bytes,"
    "read_bytes,write_bytes,threads";

#define LSM_PROCESS_RECORDER_MAX_QUEUED 4096U
#define LSM_PROCESS_RECORDER_SHUTDOWN_WAIT_MS 250U

struct LsmProcessRecorder {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    FILE *file;
    RecordNode *head;
    RecordNode *tail;
    size_t queued;
    atomic_uint references;
    int failure;
    bool stop_requested;
};

bool lsm_process_recorder_header_matches(const char *line)
{
    if (!line) return false;
    const size_t header_length = sizeof(recorder_csv_header) - 1U;
    if (strncmp(line, recorder_csv_header, header_length) != 0)
        return false;
    return strcmp(line + header_length, "\n") == 0 ||
           strcmp(line + header_length, "\r\n") == 0;
}

/* Common owns exact, finite, locale-independent numeric parsing. Copying a
 * bounded field avoids sscanf's locale dependence and narrowed PID conversion.
 * All validation is completed before either output parameter is committed. */
static bool recorder_field(const char *start, const char *end,
                           char *text, size_t capacity)
{
    if (!start || !end || !text || end <= start) return false;
    const size_t length = (size_t)(end - start);
    if (length >= capacity) return false;
    memcpy(text, start, length);
    text[length] = '\0';
    return true;
}

bool lsm_process_recorder_parse_metrics(const char *line,
                                        double *cpu_percent,
                                        double *memory_percent)
{
    if (!line || !cpu_percent || !memory_percent) return false;
    const size_t length = strlen(line);
    if (length < 2U || line[length - 1U] != '\n') return false;
    const char *end = line + length - 1U;
    if (end > line && end[-1] == '\r') end--;

    const char *fields[8];
    const char *field_ends[8];
    const char *cursor = line;
    for (size_t index = 0U; index < 7U; index++) {
        const char *comma = memchr(cursor, ',', (size_t)(end - cursor));
        if (!comma) return false;
        fields[index] = cursor;
        field_ends[index] = comma;
        cursor = comma + 1;
    }
    fields[7] = cursor;
    field_ends[7] = end;
    if (memchr(cursor, ',', (size_t)(end - cursor)) ||
        fields[0] == field_ends[0] ||
        (size_t)(field_ends[0] - fields[0]) > 64U)
        return false;

    char value_text[64];
    uint64_t integer_value = 0U;
    const size_t integral_fields[] = {1U, 4U, 5U, 6U, 7U};
    for (size_t index = 0U; index <
            sizeof(integral_fields) / sizeof(integral_fields[0]); index++) {
        const size_t column = integral_fields[index];
        if (!recorder_field(fields[column], field_ends[column],
                            value_text, sizeof(value_text)) ||
            !infiltratr_parse_u64(value_text, 10U, &integer_value))
            return false;
        if (column == 1U && integer_value == 0U) return false;
        if (column == 7U && integer_value > UINT_MAX) return false;
    }

    double cpu = 0.0;
    double memory = 0.0;
    if (!recorder_field(fields[2], field_ends[2],
                        value_text, sizeof(value_text)) ||
        !infiltratr_parse_double_range(value_text, 0.0, DBL_MAX, &cpu) ||
        !recorder_field(fields[3], field_ends[3],
                        value_text, sizeof(value_text)) ||
        !infiltratr_parse_double_range(value_text, 0.0, 100.0, &memory))
        return false;
    *cpu_percent = cpu;
    *memory_percent = memory;
    return true;
}

static void free_nodes(RecordNode *node)
{
    while (node) {
        RecordNode *next = node->next;
        free(node);
        node = next;
    }
}

static void recorder_release(LsmProcessRecorder *recorder)
{
    if (!recorder ||
        atomic_fetch_sub_explicit(&recorder->references, 1U,
                                  memory_order_acq_rel) != 1U)
        return;
    if (recorder->file) (void)fclose(recorder->file);
    free_nodes(recorder->head);
    (void)pthread_cond_destroy(&recorder->condition);
    (void)pthread_mutex_destroy(&recorder->mutex);
    free(recorder);
}

static bool write_record(LsmProcessRecorder *recorder, const RecordNode *node)
{
    struct tm local;
    char timestamp[64];
    if (!localtime_r(&node->timestamp, &local)) {
        errno = EINVAL;
        return false;
    }
    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z",
                 &local) == 0U) {
        errno = EOVERFLOW;
        return false;
    }

    char cpu_percent[64];
    char memory_percent[64];
    if (!infiltratr_format_fixed_ascii(
            node->cpu_percent, 3U, cpu_percent, sizeof(cpu_percent)) ||
        !infiltratr_format_fixed_ascii(
            node->memory_percent, 3U, memory_percent,
            sizeof(memory_percent))) {
        errno = ERANGE;
        return false;
    }
    errno = 0;
    const int written = fprintf(
        recorder->file, "%s,%llu,%s,%s,%llu,%llu,%llu,%u\n",
        timestamp, (unsigned long long)node->pid,
        cpu_percent, memory_percent,
        (unsigned long long)node->rss_bytes,
        (unsigned long long)node->read_bytes,
        (unsigned long long)node->write_bytes, node->threads);
    if (written < 0 || fflush(recorder->file) != 0) {
        if (errno == 0) errno = EIO;
        return false;
    }
    return true;
}

static void *recorder_thread_main(void *user_data)
{
    LsmProcessRecorder *recorder = user_data;
    if (!recorder) return NULL;

    for (;;) {
        (void)pthread_mutex_lock(&recorder->mutex);
        while (!recorder->head && !recorder->stop_requested)
            (void)pthread_cond_wait(&recorder->condition, &recorder->mutex);
        if (!recorder->head && recorder->stop_requested) {
            (void)pthread_mutex_unlock(&recorder->mutex);
            break;
        }
        RecordNode *node = recorder->head;
        if (!node) {
            (void)pthread_mutex_unlock(&recorder->mutex);
            continue;
        }
        recorder->head = node->next;
        if (!recorder->head) recorder->tail = NULL;
        if (recorder->queued > 0U) recorder->queued--;
        (void)pthread_mutex_unlock(&recorder->mutex);

        if (!write_record(recorder, node)) {
            const int failure = errno ? errno : EIO;
            free(node);
            (void)pthread_mutex_lock(&recorder->mutex);
            if (recorder->failure == 0) recorder->failure = failure;
            recorder->stop_requested = true;
            free_nodes(recorder->head);
            recorder->head = NULL;
            recorder->tail = NULL;
            recorder->queued = 0U;
            (void)pthread_mutex_unlock(&recorder->mutex);
            break;
        }
        free(node);
    }

    if (recorder->file) {
        errno = 0;
        if (fclose(recorder->file) != 0) {
            const int failure = errno ? errno : EIO;
            (void)pthread_mutex_lock(&recorder->mutex);
            if (recorder->failure == 0) recorder->failure = failure;
            (void)pthread_mutex_unlock(&recorder->mutex);
        }
        recorder->file = NULL;
    }
    recorder_release(recorder);
    return NULL;
}

LsmProcessRecorder *lsm_process_recorder_create(const char *path,
                                                int *error_code)
{
    if (error_code) *error_code = 0;
    if (!path || !*path) {
        if (error_code) *error_code = EINVAL;
        return NULL;
    }

    LsmProcessRecorder *recorder = calloc(1U, sizeof(*recorder));
    if (!recorder) {
        if (error_code) *error_code = ENOMEM;
        return NULL;
    }
    if (pthread_mutex_init(&recorder->mutex, NULL) != 0) {
        if (error_code) *error_code = EAGAIN;
        free(recorder);
        return NULL;
    }
    if (pthread_cond_init(&recorder->condition, NULL) != 0) {
        if (error_code) *error_code = EAGAIN;
        (void)pthread_mutex_destroy(&recorder->mutex);
        free(recorder);
        return NULL;
    }

    /* Exclusive creation is atomic: a repeated session must not truncate an
     * earlier recording, and a symlink at the chosen path is not followed. */
    recorder->file = fopen(path, "wx");
    if (!recorder->file) {
        const int failure = errno ? errno : EIO;
        (void)pthread_cond_destroy(&recorder->condition);
        (void)pthread_mutex_destroy(&recorder->mutex);
        free(recorder);
        if (error_code) *error_code = failure;
        return NULL;
    }
    errno = 0;
    const int header = fprintf(
        recorder->file,
        "%s\n", recorder_csv_header);
    if (header < 0 || fflush(recorder->file) != 0) {
        const int failure = errno ? errno : EIO;
        (void)fclose(recorder->file);
        (void)pthread_cond_destroy(&recorder->condition);
        (void)pthread_mutex_destroy(&recorder->mutex);
        free(recorder);
        if (error_code) *error_code = failure;
        return NULL;
    }

    atomic_init(&recorder->references, 2U);
    const int thread_error = pthread_create(
        &recorder->thread, NULL, recorder_thread_main, recorder);
    if (thread_error != 0) {
        if (error_code) *error_code = thread_error;
        atomic_store_explicit(&recorder->references, 1U, memory_order_release);
        recorder_release(recorder);
        return NULL;
    }
    return recorder;
}

bool lsm_process_recorder_append(LsmProcessRecorder *recorder,
                                 const LsmProcessInfo *process)
{
    if (!recorder || !process) return false;
    RecordNode *node = calloc(1U, sizeof(*node));
    if (!node) return false;
    node->timestamp = time(NULL);
    node->pid = process->pid;
    node->cpu_percent = process->cpu_percent;
    node->memory_percent = process->memory_percent;
    node->rss_bytes = process->rss_bytes;
    node->read_bytes = process->read_bytes;
    node->write_bytes = process->write_bytes;
    node->threads = process->threads;

    (void)pthread_mutex_lock(&recorder->mutex);
    if (recorder->stop_requested || recorder->failure != 0) {
        (void)pthread_mutex_unlock(&recorder->mutex);
        free(node);
        return false;
    }
    if (recorder->queued >= LSM_PROCESS_RECORDER_MAX_QUEUED) {
        recorder->failure = ENOBUFS;
        recorder->stop_requested = true;
        (void)pthread_cond_signal(&recorder->condition);
        (void)pthread_mutex_unlock(&recorder->mutex);
        free(node);
        return false;
    }
    if (recorder->tail)
        recorder->tail->next = node;
    else
        recorder->head = node;
    recorder->tail = node;
    recorder->queued++;
    (void)pthread_cond_signal(&recorder->condition);
    (void)pthread_mutex_unlock(&recorder->mutex);
    return true;
}

int lsm_process_recorder_error(LsmProcessRecorder *recorder)
{
    if (!recorder) return EINVAL;
    (void)pthread_mutex_lock(&recorder->mutex);
    const int failure = recorder->failure;
    (void)pthread_mutex_unlock(&recorder->mutex);
    return failure;
}

void lsm_process_recorder_stop(LsmProcessRecorder *recorder)
{
    if (!recorder) return;
    (void)pthread_mutex_lock(&recorder->mutex);
    recorder->stop_requested = true;
    (void)pthread_cond_signal(&recorder->condition);
    (void)pthread_mutex_unlock(&recorder->mutex);

    /* Give ordinary local storage a bounded opportunity to drain the queued
     * recording before process exit. A blocked filesystem must not turn GUI
     * shutdown into an unbounded join, so ownership falls back to the worker
     * after the same short-wait pattern used by the monitor sampler. */
    struct timespec deadline;
    int join_result = lsm_posix_deadline_after_milliseconds(
        CLOCK_REALTIME, LSM_PROCESS_RECORDER_SHUTDOWN_WAIT_MS, &deadline);
    if (join_result == 0)
        join_result = pthread_timedjoin_np(recorder->thread, NULL, &deadline);
    if (join_result != 0)
        (void)pthread_detach(recorder->thread);

    recorder_release(recorder);
}
