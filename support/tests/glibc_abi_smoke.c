// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file glibc_abi_smoke.c
 * @brief Regression tests for finished-binary GLIBC ABI version detection.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "../tools/glibc_abi.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void require(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "GLIBC ABI smoke failure: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void write_all(int descriptor, const unsigned char *data, size_t length)
{
    size_t offset = 0U;
    while (offset < length) {
        const ssize_t written = write(descriptor, data + offset, length - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
            perror("write");
            exit(EXIT_FAILURE);
        }
        offset += (size_t)written;
    }
}

static void check_fixture(const unsigned char *data, size_t length,
                          int expected_result, unsigned expected_major,
                          unsigned expected_minor)
{
    char path[] = "/tmp/lsm-glibc-abi-XXXXXX";
    const int descriptor = mkstemp(path);
    require(descriptor >= 0, "mkstemp failed");
    write_all(descriptor, data, length);
    require(close(descriptor) == 0, "fixture close failed");

    unsigned major = 99U;
    unsigned minor = 99U;
    const int result = lsm_glibc_abi_max_version(path, &major, &minor);
    require(unlink(path) == 0, "fixture unlink failed");
    require(result == expected_result, "unexpected parser result");
    if (expected_result == 1) {
        require(major == expected_major && minor == expected_minor,
                "incorrect greatest GLIBC version");
    }
}

int main(void)
{
    static const unsigned char supported[] = {
        0, 'G','L','I','B','C','_','2','.','2',0,
        'x',0,'G','L','I','B','C','_','2','.','3','4',0
    };
    static const unsigned char too_new[] = {
        0, 'G','L','I','B','C','_','2','.','3','4',0,
        'G','L','I','B','C','_','2','.','3','8',0
    };
    static const unsigned char ignored_text[] = {
        'N','O','T','G','L','I','B','C','_','9','.','9',0,
        'G','L','I','B','C','_','P','R','I','V','A','T','E',0
    };

    check_fixture(supported, sizeof(supported), 1, 2U, 34U);
    check_fixture(too_new, sizeof(too_new), 1, 2U, 38U);
    check_fixture(ignored_text, sizeof(ignored_text), 0, 0U, 0U);

    require(lsm_glibc_abi_compare(2U, 34U, 2U, 34U) == 0,
            "equal version comparison failed");
    require(lsm_glibc_abi_compare(2U, 33U, 2U, 34U) < 0,
            "older version comparison failed");
    require(lsm_glibc_abi_compare(3U, 0U, 2U, 99U) > 0,
            "major version comparison failed");

    puts("GLIBC ABI parser and release-baseline comparison passed.");
    return EXIT_SUCCESS;
}
