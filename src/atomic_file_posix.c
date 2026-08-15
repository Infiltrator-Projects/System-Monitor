// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file atomic_file_posix.c
 * @brief POSIX provider for durable application file replacement.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "atomic_file.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct {
    const void *data;
    size_t length;
} LsmAtomicBytes;

static char *parent_directory(const char *path)
{
    const char *separator = strrchr(path, '/');
    if (!separator) return strdup(".");
    if (separator == path) return strdup("/");

    const size_t length = (size_t)(separator - path);
    char *parent = malloc(length + 1U);
    if (!parent) return NULL;
    memcpy(parent, path, length);
    parent[length] = '\0';
    return parent;
}

static char *temporary_template(const char *parent)
{
    static const char suffix[] = ".lsm-write-XXXXXX";
    const size_t parent_length = strlen(parent);
    const bool needs_separator = parent_length == 0U ||
                                 parent[parent_length - 1U] != '/';
    if (parent_length > SIZE_MAX - sizeof(suffix) - 1U) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    const size_t size = parent_length + (needs_separator ? 1U : 0U) +
                        sizeof(suffix);
    char *temporary = malloc(size);
    if (!temporary) return NULL;
    const int written = snprintf(temporary, size, "%s%s%s", parent,
                                 needs_separator ? "/" : "", suffix);
    if (written < 0 || (size_t)written >= size) {
        free(temporary);
        errno = ENAMETOOLONG;
        return NULL;
    }
    return temporary;
}

static int completed_mode(const char *path, LsmAtomicFileMode mode,
                          mode_t *permissions)
{
    if (!permissions) return EINVAL;
    if (mode == LSM_ATOMIC_FILE_PRIVATE) {
        *permissions = S_IRUSR | S_IWUSR;
        return 0;
    }
    if (mode != LSM_ATOMIC_FILE_USER_DOCUMENT) return EINVAL;

    struct stat status;
    if (lstat(path, &status) == 0) {
        *permissions = S_ISREG(status.st_mode) ?
            status.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO) :
            S_IRUSR | S_IWUSR;
        return 0;
    }
    if (errno != ENOENT) return errno;
    *permissions = S_IRUSR | S_IWUSR;
    return 0;
}

static int mark_close_on_exec(int descriptor)
{
    const int flags = fcntl(descriptor, F_GETFD);
    if (flags < 0 || fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) != 0)
        return errno;
    return 0;
}

static int sync_directory(const char *path)
{
    const int descriptor = open(path, O_RDONLY);
    if (descriptor < 0) return errno;

    int failure = mark_close_on_exec(descriptor);
    struct stat status;
    if (failure == 0 && fstat(descriptor, &status) != 0) failure = errno;
    if (failure == 0 && !S_ISDIR(status.st_mode)) failure = ENOTDIR;
    if (failure == 0 && fsync(descriptor) != 0) failure = errno;
    if (close(descriptor) != 0 && failure == 0) failure = errno;
    return failure;
}

static int finish_stream(FILE *stream, bool content_complete)
{
    int failure = content_complete ? 0 : (errno ? errno : EIO);
    if (failure == 0 && ferror(stream)) failure = EIO;
    if (failure == 0 && fflush(stream) != 0) failure = errno;
    if (failure == 0 && fsync(fileno(stream)) != 0) failure = errno;
    if (fclose(stream) != 0 && failure == 0) failure = errno;
    return failure;
}

int lsm_atomic_file_write(const char *path, LsmAtomicFileMode mode,
                          LsmAtomicFileWriter writer, const void *user_data)
{
    if (!path || !*path || !writer) return EINVAL;

    mode_t permissions = 0;
    int failure = completed_mode(path, mode, &permissions);
    if (failure != 0) return failure;

    char *parent = parent_directory(path);
    if (!parent) return errno ? errno : ENOMEM;
    char *temporary = temporary_template(parent);
    if (!temporary) {
        failure = errno ? errno : ENOMEM;
        free(parent);
        return failure;
    }

    const int descriptor = mkstemp(temporary);
    if (descriptor < 0) {
        failure = errno;
        free(temporary);
        free(parent);
        return failure;
    }
    failure = mark_close_on_exec(descriptor);
    if (failure == 0 && fchmod(descriptor, permissions) != 0) failure = errno;

    FILE *stream = NULL;
    if (failure == 0) {
        stream = fdopen(descriptor, "wb");
        if (!stream) failure = errno;
    }
    if (!stream) {
        (void)close(descriptor);
    } else {
        errno = 0;
        const bool content_complete = writer(stream, user_data);
        failure = finish_stream(stream, content_complete);
    }

    if (failure == 0 && rename(temporary, path) != 0) failure = errno;
    if (failure == 0) failure = sync_directory(parent);
    if (failure != 0) (void)unlink(temporary);

    free(temporary);
    free(parent);
    return failure;
}

static bool write_bytes(FILE *stream, const void *user_data)
{
    const LsmAtomicBytes *bytes = user_data;
    return bytes->length == 0U ||
           fwrite(bytes->data, 1U, bytes->length, stream) == bytes->length;
}

int lsm_atomic_file_write_bytes(const char *path, LsmAtomicFileMode mode,
                                const void *data, size_t length)
{
    if (!data && length != 0U) return EINVAL;
    LsmAtomicBytes bytes = {.data = data, .length = length};
    return lsm_atomic_file_write(path, mode, write_bytes, &bytes);
}
