// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file glibc_abi.c
 * @brief Dependency-free GLIBC symbol-version inspection for release tooling.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "glibc_abi.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_component(const unsigned char **cursor,
                            const unsigned char *end, unsigned *value_out)
{
    if (!cursor || !*cursor || !value_out || *cursor >= end ||
        **cursor < (unsigned char)'0' || **cursor > (unsigned char)'9')
        return false;

    unsigned value = 0U;
    do {
        const unsigned digit = (unsigned)(**cursor - (unsigned char)'0');
        if (value > (UINT_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
        (*cursor)++;
    } while (*cursor < end && **cursor >= (unsigned char)'0' &&
             **cursor <= (unsigned char)'9');

    *value_out = value;
    return true;
}

int lsm_glibc_abi_compare(unsigned left_major, unsigned left_minor,
                          unsigned right_major, unsigned right_minor)
{
    if (left_major != right_major)
        return left_major < right_major ? -1 : 1;
    if (left_minor != right_minor)
        return left_minor < right_minor ? -1 : 1;
    return 0;
}

int lsm_glibc_abi_max_version(const char *path, unsigned *major_out,
                              unsigned *minor_out)
{
    static const unsigned char prefix[] = "GLIBC_";
    if (!path || !major_out || !minor_out) {
        errno = EINVAL;
        return -1;
    }

    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }
    const long length_long = ftell(file);
    if (length_long < 0 || fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    const size_t length = (size_t)length_long;
    if ((long)length != length_long || length == SIZE_MAX) {
        fclose(file);
        errno = EOVERFLOW;
        return -1;
    }

    unsigned char *contents = malloc(length + 1U);
    if (!contents) {
        fclose(file);
        return -1;
    }
    if (length > 0U && fread(contents, 1U, length, file) != length) {
        const int saved_errno = ferror(file) && errno ? errno : EIO;
        free(contents);
        fclose(file);
        errno = saved_errno;
        return -1;
    }
    if (fclose(file) != 0) {
        const int saved_errno = errno;
        free(contents);
        errno = saved_errno;
        return -1;
    }
    contents[length] = '\0';

    bool found = false;
    unsigned greatest_major = 0U;
    unsigned greatest_minor = 0U;
    const size_t prefix_length = sizeof(prefix) - 1U;
    for (size_t offset = 0U; offset + prefix_length < length; offset++) {
        if (offset > 0U && contents[offset - 1U] != '\0') continue;
        if (memcmp(contents + offset, prefix, prefix_length) != 0) continue;

        const unsigned char *cursor = contents + offset + prefix_length;
        const unsigned char *end = contents + length;
        unsigned major = 0U;
        unsigned minor = 0U;
        if (!parse_component(&cursor, end, &major) || cursor >= end ||
            *cursor++ != (unsigned char)'.' ||
            !parse_component(&cursor, end, &minor) || cursor >= end ||
            *cursor != '\0')
            continue;

        if (!found || lsm_glibc_abi_compare(major, minor, greatest_major,
                                            greatest_minor) > 0) {
            greatest_major = major;
            greatest_minor = minor;
            found = true;
        }
    }

    free(contents);
    if (!found) return 0;
    *major_out = greatest_major;
    *minor_out = greatest_minor;
    return 1;
}
