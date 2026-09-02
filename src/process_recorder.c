// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_recorder.c
 * @brief Detached CSV process-recording writer.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_recorder.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
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

#define LSM_PROCESS_RECORDER_MAX_QUEUED 4096U

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

    errno = 0;
    const int written = fprintf(
        recorder->file, "%s,%llu,%.3f,%.3f,%llu,%llu,%llu,%u\n",
        timestamp, (unsigned long long)node->pid,
        node->cpu_percent, node->memory_percent,
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

    recorder->file = fopen(path, "w");
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
        "timestamp,pid,cpu_percent,memory_percent,rss_bytes,read_bytes,write_bytes,threads\n");
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
    if (pthread_detach(recorder->thread) != 0) {
        (void)pthread_mutex_lock(&recorder->mutex);
        recorder->stop_requested = true;
        (void)pthread_cond_signal(&recorder->condition);
        (void)pthread_mutex_unlock(&recorder->mutex);
        (void)pthread_join(recorder->thread, NULL);
        atomic_store_explicit(&recorder->references, 1U, memory_order_release);
        recorder_release(recorder);
        if (error_code) *error_code = EAGAIN;
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
    recorder_release(recorder);
}
