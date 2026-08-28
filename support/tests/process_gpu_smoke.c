// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_gpu_smoke.c
 * @brief Synthetic DRM client deduplication and utilisation regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "process_gpu.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_CLIENT_LIMIT 128U

static void write_client(const char *directory, const char *name,
                         const char *contents)
{
    char path[512];
    const int written = snprintf(path, sizeof(path), "%s/%s", directory, name);
    assert(written > 0 && (size_t)written < sizeof(path));
    FILE *file = fopen(path, "w");
    assert(file);
    assert(fputs(contents, file) >= 0);
    assert(fclose(file) == 0);
}

static void remove_process_fixture(const char *root, int pid)
{
    char directory[512];
    (void)snprintf(directory, sizeof(directory), "%s/%d/fdinfo", root, pid);
    DIR *opened = opendir(directory);
    if (opened) {
        struct dirent *entry = NULL;
        while ((entry = readdir(opened))) {
            if (entry->d_name[0] == '.') continue;
            char path[1024];
            (void)snprintf(path, sizeof(path), "%s/%s",
                           directory, entry->d_name);
            (void)unlink(path);
        }
        closedir(opened);
    }
    (void)rmdir(directory);
    (void)snprintf(directory, sizeof(directory), "%s/%d", root, pid);
    (void)rmdir(directory);
}

int main(void)
{
    char root[] = "/tmp/lsm-process-gpu-XXXXXX";
    assert(mkdtemp(root));
    char process_directory[512];
    (void)snprintf(process_directory, sizeof(process_directory),
                   "%s/4242", root);
    assert(mkdir(process_directory, 0700) == 0);
    char fdinfo[1024];
    (void)snprintf(fdinfo, sizeof(fdinfo), "%s/fdinfo", process_directory);
    assert(mkdir(fdinfo, 0700) == 0);

    const char *first_client =
        "drm-driver: test\n"
        "drm-pdev: 0000:00:02.0\n"
        "drm-client-id: 7\n"
        "drm-engine-capacity-render: 2\n"
        "drm-engine-render: 1000000000 ns\n"
        "drm-engine-copy: 500000000 ns\n"
        "drm-memory-local: 2097152\n"
        "drm-resident-local: 1024 K\n"
        "drm-resident-malformed: 10 Kjunk\n";
    write_client(fdinfo, "3", first_client);
    write_client(fdinfo, "4", first_client);
    write_client(fdinfo, "5",
        "drm-driver: test\n"
        "drm-pdev: 0000:00:02.0\n"
        "drm-client-id: 8\n"
        "drm-engine-render: 500000000 ns\n"
        "drm-resident-shared: 2097152\n");

    LsmProcessGpuSnapshot first;
    assert(lsm_process_gpu_read(root, 4242, &first));
    assert(first.engine_count == 2U);
    assert(first.memory_available);
    assert(first.memory_bytes == 3ULL * 1024ULL * 1024ULL);

    const char *updated_client =
        "drm-driver: test\n"
        "drm-pdev: 0000:00:02.0\n"
        "drm-client-id: 7\n"
        "drm-engine-capacity-render: 2\n"
        "drm-engine-render: 1400000000 ns\n"
        "drm-engine-copy: 700000000 ns\n"
        "drm-memory-local: 2097152\n"
        "drm-resident-local: 1024 K\n"
        "drm-resident-malformed: 10 Kjunk\n";
    write_client(fdinfo, "3", updated_client);
    write_client(fdinfo, "4", updated_client);
    write_client(fdinfo, "5",
        "drm-driver: test\n"
        "drm-pdev: 0000:00:02.0\n"
        "drm-client-id: 8\n"
        "drm-engine-render: 700000000 ns\n"
        "drm-resident-shared: 2097152\n");

    LsmProcessGpuSnapshot second;
    assert(lsm_process_gpu_read(root, 4242, &second));
    double percent = 0.0;
    char engine[64];
    assert(lsm_process_gpu_calculate_engine(
        &second, &first, 1.0, &percent, engine, sizeof(engine)));
    assert(fabs(percent - 30.0) < 0.0001);
    assert(strcmp(engine, "render") == 0);

    LsmProcessGpuSnapshot temporary_drop = second;
    assert(temporary_drop.engine_count > 0U);
    temporary_drop.engines[0].time_ns = 1U;
    lsm_process_gpu_normalise(&temporary_drop, &second);
    assert(temporary_drop.engines[0].time_ns == second.engines[0].time_ns);

    char capped_process[512];
    (void)snprintf(capped_process, sizeof(capped_process), "%s/4343", root);
    assert(mkdir(capped_process, 0700) == 0);
    char capped_fdinfo[1024];
    (void)snprintf(capped_fdinfo, sizeof(capped_fdinfo),
                   "%s/fdinfo", capped_process);
    assert(mkdir(capped_fdinfo, 0700) == 0);
    for (unsigned index = 0U; index < 140U; index++) {
        char name[32];
        char contents[256];
        (void)snprintf(name, sizeof(name), "%u", index + 10U);
        (void)snprintf(contents, sizeof(contents),
                       "drm-driver: test\n"
                       "drm-pdev: 0000:00:03.0\n"
                       "drm-client-id: %u\n"
                       "drm-resident-local: 1024\n",
                       index + 1U);
        write_client(capped_fdinfo, name, contents);
    }

    LsmProcessGpuSnapshot capped;
    assert(lsm_process_gpu_read(root, 4343, &capped));
    assert(capped.memory_available);
    assert(capped.memory_bytes == TEST_CLIENT_LIMIT * 1024ULL);

    remove_process_fixture(root, 4343);
    remove_process_fixture(root, 4242);
    (void)rmdir(root);
    puts("Per-process DRM accounting and client deduplication passed.");
    return 0;
}
