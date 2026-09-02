// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file async_workers_smoke.c
 * @brief Exercise detached process scanning and ordered recording workers.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_recorder.h"
#include "process_scanner.h"
#include "process_backend.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define WORKER_POLL_ATTEMPTS 400U
#define WORKER_POLL_NS 25000000L

static void short_pause(void)
{
    const struct timespec pause = {
        .tv_sec = 0,
        .tv_nsec = WORKER_POLL_NS
    };
    (void)nanosleep(&pause, NULL);
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

    if (!lsm_process_scanner_request(scanner, LSM_PROCESS_SCAN_NONE)) {
        fputs("Unable to request base process scan.\n", stderr);
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
    (void)close(descriptor);

    int failure = 0;
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

    if (unlink(path) != 0) {
        fputs("Unable to remove process recorder fixture.\n", stderr);
        return EXIT_FAILURE;
    }

    puts("Async process worker smoke passed.");
    return EXIT_SUCCESS;
}
