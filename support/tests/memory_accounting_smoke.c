// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file memory_accounting_smoke.c
 * @brief Exact binary memory-accounting and refresh-semantics regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "memory_accounting.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void replace_file(const char *path, const char *contents)
{
    const int descriptor = open(path, O_WRONLY | O_TRUNC);
    assert(descriptor >= 0);
    const size_t length = strlen(contents);
    assert(write(descriptor, contents, length) == (ssize_t)length);
    assert(close(descriptor) == 0);
}

int main(void)
{
    char path[] = "/tmp/lsm-memory-accounting-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    assert(close(descriptor) == 0);

    replace_file(
        path,
        "MemAvailable: 29360128 kB\n"
        "Cached: 4096 kB\n"
        "SReclaimable: 2048 kB\n"
        "SUnreclaim: 3072 kB\n"
        "Shmem: 1024 kB\n"
        "Committed_AS: 7340032 kB\n"
        "CommitLimit: 33554432 kB\n"
        "PageTables: 512 kB\n"
        "HardwareCorrupted: 32 kB\n");

    LsmMemoryInfo memory = {0};
    assert(lsm_memory_accounting_read(path, &memory, true));
    assert(memory.available_bytes == (28ULL << 30U));
    assert(memory.cached_bytes == (5ULL << 20U));
    assert(memory.committed_bytes == (7ULL << 30U));
    assert(memory.commit_limit_bytes == (32ULL << 30U));
    assert(memory.kernel_reclaimable_bytes == (2ULL << 20U));
    assert(memory.kernel_nonreclaimable_bytes == (3ULL << 20U));
    assert(memory.page_tables_bytes == (512ULL << 10U));
    assert(memory.hardware_corrupted_bytes == (32ULL << 10U));

    replace_file(path,
                 "MemAvailable: 0 kB\n"
                 "Committed_AS: 8388608 kB\n"
                 "CommitLimit: 33554432 kB\n");
    assert(lsm_memory_accounting_read(path, &memory, false));
    assert(memory.available_bytes == 0U);
    assert(memory.cached_bytes == (5ULL << 20U));
    assert(memory.committed_bytes == (8ULL << 30U));
    assert(memory.kernel_nonreclaimable_bytes == (3ULL << 20U));

    assert(unlink(path) == 0);
    puts("Memory accounting smoke test passed.");
    return 0;
}
