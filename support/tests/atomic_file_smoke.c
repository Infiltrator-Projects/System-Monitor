// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file atomic_file_smoke.c
 * @brief Durable replacement, permissions and failure-cleanup regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "atomic_file.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool fail_after_partial_write(FILE *stream, const void *user_data)
{
    (void)user_data;
    assert(fputs("incomplete", stream) >= 0);
    errno = ENOSPC;
    return false;
}

static void expect_contents(const char *path, const char *expected)
{
    FILE *file = fopen(path, "rb");
    assert(file);
    char contents[128];
    const size_t length = fread(contents, 1U, sizeof(contents) - 1U, file);
    assert(fclose(file) == 0);
    contents[length] = '\0';
    assert(strcmp(contents, expected) == 0);
}

static void expect_no_temporary_files(const char *directory)
{
    DIR *stream = opendir(directory);
    assert(stream);
    struct dirent *entry;
    while ((entry = readdir(stream)))
        assert(strncmp(entry->d_name, ".infiltratr-write-", 18U) != 0);
    assert(closedir(stream) == 0);
}

int main(void)
{
    char directory[] = "/tmp/lsm-atomic-file-XXXXXX";
    assert(mkdtemp(directory));
    char path[256];
    assert(snprintf(path, sizeof(path), "%s/state.conf", directory) > 0);

    static const char first[] = "first complete value\n";
    assert(lsm_atomic_file_write_bytes(path, LSM_ATOMIC_FILE_PRIVATE,
                                       first, sizeof(first) - 1U) == 0);
    expect_contents(path, first);
    struct stat status;
    assert(stat(path, &status) == 0);
    assert((status.st_mode & 0777) == 0600);

    assert(chmod(path, 0640) == 0);
    static const char second[] = "replacement\n";
    assert(lsm_atomic_file_write_bytes(path,
                                       LSM_ATOMIC_FILE_USER_DOCUMENT,
                                       second, sizeof(second) - 1U) == 0);
    expect_contents(path, second);
    assert(stat(path, &status) == 0);
    assert((status.st_mode & 0777) == 0640);

    assert(lsm_atomic_file_write(path, LSM_ATOMIC_FILE_PRIVATE,
                                 fail_after_partial_write, NULL) == ENOSPC);
    expect_contents(path, second);
    expect_no_temporary_files(directory);

    assert(lsm_atomic_file_write_bytes(NULL, LSM_ATOMIC_FILE_PRIVATE,
                                       first, sizeof(first) - 1U) == EINVAL);
    assert(lsm_atomic_file_write_bytes(path, LSM_ATOMIC_FILE_PRIVATE,
                                       NULL, 1U) == EINVAL);
    assert(unlink(path) == 0);
    assert(rmdir(directory) == 0);
    puts("Durable atomic replacement, permissions and cleanup passed.");
    return 0;
}
