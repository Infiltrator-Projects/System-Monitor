// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file async_workers_smoke.c
 * @brief Exercise detached process scanning and ordered recording workers.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_recorder.h"
#include "process_scanner.h"
#include "process_backend.h"

#include <glib.h>

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define WORKER_POLL_ATTEMPTS 400U
#define WORKER_POLL_NS 25000000L

typedef struct {
    atomic_uint notifications;
    atomic_bool wrong_thread;
    pthread_t expected_thread;
} ScannerReadyState;

static void short_pause(void)
{
    const struct timespec pause = {
        .tv_sec = 0,
        .tv_nsec = WORKER_POLL_NS
    };
    (void)nanosleep(&pause, NULL);
}

static void scanner_ready(void *user_data)
{
    ScannerReadyState *state = user_data;
    if (!state) return;
    if (!pthread_equal(pthread_self(), state->expected_thread))
        atomic_store_explicit(&state->wrong_thread, true, memory_order_release);
    (void)atomic_fetch_add_explicit(
        &state->notifications, 1U, memory_order_release);
}

static int wait_for_ready(const ScannerReadyState *state)
{
    for (unsigned attempt = 0U; attempt < WORKER_POLL_ATTEMPTS; attempt++) {
        while (g_main_context_iteration(NULL, FALSE)) {
        }
        if (state &&
            atomic_load_explicit(&state->notifications, memory_order_acquire) > 0U)
            return 0;
        short_pause();
    }
    return ETIMEDOUT;
}

static int wait_for_snapshot(LsmProcessScanner *scanner,
                             LsmProcessInfo **processes,
                             size_t *count)
{
    for (unsigned attempt = 0U; attempt < WORKER_POLL_ATTEMPTS; attempt++) {
        if (lsm_process_scanner_take(scanner, processes, count)) return 0;
        short_pause();
    }
    return ETIMEDOUT;
}

static int wait_for_record_rows(const char *path, unsigned expected_rows)
{
    for (unsigned attempt = 0U; attempt < WORKER_POLL_ATTEMPTS; attempt++) {
        FILE *file = fopen(path, "r");
        if (file) {
            unsigned rows = 0U;
            char line[1024];
            while (fgets(line, sizeof(line), file))
                if (strncmp(line, "timestamp,", 10U) != 0 && line[0])
                    rows++;
            (void)fclose(file);
            if (rows >= expected_rows) return 0;
        }
        short_pause();
    }
    return ETIMEDOUT;
}

int main(void)
{
    LsmProcessScanner *scanner = lsm_process_scanner_create();
    if (!scanner) {
        fputs("Unable to create detached process scanner.\n", stderr);
        return EXIT_FAILURE;
    }

    ScannerReadyState ready_state = {
        .expected_thread = pthread_self()
    };
    atomic_init(&ready_state.notifications, 0U);
    atomic_init(&ready_state.wrong_thread, false);
    lsm_process_scanner_set_ready_callback(
        scanner, scanner_ready, &ready_state);

    if (!lsm_process_scanner_request(scanner, LSM_PROCESS_SCAN_NONE)) {
        fputs("Unable to request base process scan.\n", stderr);
        lsm_process_scanner_destroy(scanner);
        return EXIT_FAILURE;
    }

    if (wait_for_ready(&ready_state) != 0) {
        fputs("Detached process scan did not notify completion.\n", stderr);
        lsm_process_scanner_destroy(scanner);
        return EXIT_FAILURE;
    }
    if (atomic_load_explicit(&ready_state.wrong_thread, memory_order_acquire)) {
        fputs("Detached process scan notified outside the main-loop thread.\n",
              stderr);
        lsm_process_scanner_destroy(scanner);
        return EXIT_FAILURE;
    }

    LsmProcessInfo *processes = NULL;
    size_t count = 0U;
    if (wait_for_snapshot(scanner, &processes, &count) != 0) {
        fputs("Detached process scan did not publish a snapshot.\n", stderr);
        lsm_process_scanner_destroy(scanner);
        return EXIT_FAILURE;
    }
    lsm_process_list_free(processes);

    if (!lsm_process_scanner_request(
            scanner, LSM_PROCESS_SCAN_EXECUTABLE |
                     LSM_PROCESS_SCAN_HANDLE_COUNT)) {
        fputs("Unable to request enriched process scan.\n", stderr);
        lsm_process_scanner_destroy(scanner);
        return EXIT_FAILURE;
    }
    processes = NULL;
    count = 0U;
    if (wait_for_snapshot(scanner, &processes, &count) != 0) {
        fputs("Enriched detached process scan did not publish.\n", stderr);
        lsm_process_scanner_destroy(scanner);
        return EXIT_FAILURE;
    }
    lsm_process_list_free(processes);
    lsm_process_scanner_destroy(scanner);

    char path[] = "/tmp/lsm-process-recorder-XXXXXX";
    const int descriptor = mkstemp(path);
    if (descriptor < 0) {
        fputs("Unable to create recording fixture path.\n", stderr);
        return EXIT_FAILURE;
    }
    static const char original[] = "Earlier recording must remain intact\n";
    if (write(descriptor, original, sizeof(original) - 1U) !=
            (ssize_t)(sizeof(original) - 1U) ||
        close(descriptor) != 0) {
        (void)unlink(path);
        return EXIT_FAILURE;
    }

    int failure = 0;
    LsmProcessRecorder *collision =
        lsm_process_recorder_create(path, &failure);
    if (collision != NULL || failure != EEXIST) {
        if (collision) lsm_process_recorder_stop(collision);
        fputs("Recorder did not reject an existing recording.\n", stderr);
        (void)unlink(path);
        return EXIT_FAILURE;
    }
    FILE *original_file = fopen(path, "rb");
    char preserved[sizeof(original)] = { 0 };
    const bool intact = original_file != NULL &&
        fread(preserved, 1U, sizeof(original) - 1U, original_file) ==
            sizeof(original) - 1U &&
        strcmp(preserved, original) == 0;
    if (original_file != NULL) (void)fclose(original_file);
    if (!intact || unlink(path) != 0) {
        fputs("Recorder damaged an earlier recording.\n", stderr);
        return EXIT_FAILURE;
    }
    failure = 0;
    LsmProcessRecorder *recorder =
        lsm_process_recorder_create(path, &failure);
    if (!recorder) {
        fprintf(stderr, "Unable to create process recorder: %s\n",
                strerror(failure ? failure : EIO));
        (void)unlink(path);
        return EXIT_FAILURE;
    }

    LsmProcessInfo sample = {0};
    sample.pid = 4242U;
    sample.cpu_percent = 12.5;
    sample.memory_percent = 3.25;
    sample.rss_bytes = UINT64_C(64) * 1024U * 1024U;
    sample.read_bytes = 1000U;
    sample.write_bytes = 2000U;
    sample.threads = 7U;

    if (!lsm_process_recorder_append(recorder, &sample)) {
        fputs("Unable to queue first recording sample.\n", stderr);
        lsm_process_recorder_stop(recorder);
        (void)unlink(path);
        return EXIT_FAILURE;
    }
    sample.read_bytes = 3000U;
    sample.write_bytes = 4000U;
    if (!lsm_process_recorder_append(recorder, &sample)) {
        fputs("Unable to queue second recording sample.\n", stderr);
        lsm_process_recorder_stop(recorder);
        (void)unlink(path);
        return EXIT_FAILURE;
    }

    lsm_process_recorder_stop(recorder);
    if (wait_for_record_rows(path, 2U) != 0) {
        fputs("Detached recorder did not drain queued samples.\n", stderr);
        (void)unlink(path);
        return EXIT_FAILURE;
    }

    FILE *recorded = fopen(path, "r");
    if (!recorded) {
        (void)unlink(path);
        return EXIT_FAILURE;
    }
    char recorded_contents[4096];
    const size_t recorded_length =
        fread(recorded_contents, 1U, sizeof(recorded_contents) - 1U, recorded);
    recorded_contents[recorded_length] = '\0';
    (void)fclose(recorded);
    if (!strstr(recorded_contents, ",4242,12.500,3.250,")) {
        fputs("Recorder did not emit locale-independent decimal fields.\n", stderr);
        (void)unlink(path);
        return EXIT_FAILURE;
    }

    /* Validate the production writer's actual schema and second data row. */
    const char *first_end = strchr(recorded_contents, '\n');
    const char *second_end = first_end ? strchr(first_end + 1, '\n') : NULL;
    if (!first_end || !second_end) {
        fputs("Missing process-recording CSV lines.\n", stderr);
        (void)unlink(path);
        return EXIT_FAILURE;
    }
    char header_line[128];
    const size_t header_length = (size_t)(first_end - recorded_contents) + 1U;
    if (header_length >= sizeof(header_line)) {
        (void)unlink(path);
        return EXIT_FAILURE;
    }
    memcpy(header_line, recorded_contents, header_length);
    header_line[header_length] = '\0';
    double cpu = -1.0, memory = -1.0;
    if (!lsm_process_recorder_header_matches(header_line) ||
        !lsm_process_recorder_parse_metrics(second_end + 1, &cpu, &memory) ||
        cpu != 12.5 || memory != 3.25) {
        fputs("Recorder and graph reader disagree on CSV format.\n", stderr);
        (void)unlink(path);
        return EXIT_FAILURE;
    }

    static const char valid_large_pid[] =
        "2026-10-20T12:30:00+1100,4294967297,207.250,3.250,"
        "67108864,1000,2000,7\r\n";
    if (!lsm_process_recorder_parse_metrics(
            valid_large_pid, &cpu, &memory) ||
        cpu != 207.25 || memory != 3.25) {
        fputs("Large PID or ASCII decimal process log parsing failed.\n", stderr);
        (void)unlink(path);
        return EXIT_FAILURE;
    }
    static const char valid_leap_offset[] =
        "2024-02-29T23:59:59-001932,4294967297,207.250,3.250,"
        "67108864,1000,2000,7\n";
    if (!lsm_process_recorder_parse_metrics(
            valid_leap_offset, &cpu, &memory) ||
        cpu != 207.25 || memory != 3.25) {
        fputs("Leap day or historical UTC offset rejected.\n", stderr);
        (void)unlink(path);
        return EXIT_FAILURE;
    }
    static const char *invalid_rows[] = {
        "not-a-timestamp,1,12.500,3.250,64,1,2,7\n",
        "2026-02-30T12:30:00+1100,1,12.500,3.250,64,1,2,7\n",
        "2025-02-29T12:30:00+1100,1,12.500,3.250,64,1,2,7\n",
        "2026-10-20T24:30:00+1100,1,12.500,3.250,64,1,2,7\n",
        "2026-10-20T12:30:00+1160,1,12.500,3.250,64,1,2,7\n",
        "2026-10-20T12:30:00+110060,1,12.500,3.250,64,1,2,7\n",
        "2026-10-20T12:30:00+1100junk,1,12.500,3.250,64,1,2,7\n",
        "2026-10-20T12:30:00+1100,4294967297,12,500,3.250,64,1,2,7\n",
        "2026-10-20T12:30:00+1100,4294967297,NaN,3.250,64,1,2,7\n",
        "2026-10-20T12:30:00+1100,4294967297,-1.0,3.250,64,1,2,7\n",
        "2026-10-20T12:30:00+1100,4294967297,12.500,101,64,1,2,7\n",
        "2026-10-20T12:30:00+1100,18446744073709551616,12.500,3.250,64,1,2,7\n",
        "2026-10-20T12:30:00+1100,1,12.500,3.250,64,1,2\n",
        "2026-10-20T12:30:00+1100,1,12.500,3.250,64,1,2,7",
        "2026-10-20T12:30:00+1100,1,12.500,3.250,64,1,2,7,99\n"
    };
    for (size_t index = 0U; index <
            sizeof(invalid_rows) / sizeof(invalid_rows[0]); index++) {
        cpu = 55.0;
        memory = 66.0;
        if (lsm_process_recorder_parse_metrics(
                invalid_rows[index], &cpu, &memory) ||
            cpu != 55.0 || memory != 66.0) {
            fputs("Malformed recorder row accepted or output corrupted.\n", stderr);
            (void)unlink(path);
            return EXIT_FAILURE;
        }
    }
    if (lsm_process_recorder_header_matches("timestamp,pid\n")) {
        (void)unlink(path);
        return EXIT_FAILURE;
    }

    if (unlink(path) != 0) {
        fputs("Unable to remove process recorder fixture.\n", stderr);
        return EXIT_FAILURE;
    }

    puts("Async process worker smoke passed.");
    return EXIT_SUCCESS;
}
