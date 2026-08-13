// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file common_smoke.c
 * @brief Regression tests for shared native utility functions.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "common.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char text[8];
    lsm_copy_string(text, sizeof(text), "123456789");
    assert(strcmp(text, "1234567") == 0);

    char whitespace[] = "  value \n";
    lsm_trim(whitespace);
    assert(strcmp(whitespace, "value") == 0);

    assert(lsm_string_equal(NULL, NULL));
    assert(lsm_string_equal("monitor", "monitor"));
    assert(!lsm_string_equal("monitor", NULL));
    assert(lsm_string_starts_with("linux-system-monitor", "linux"));
    assert(lsm_string_ends_with("linux-system-monitor", "monitor"));
    uint64_t parsed = 0U;
    assert(lsm_parse_u64("0xff", 0U, &parsed));
    assert(parsed == 255U);
    assert(!lsm_parse_u64("-1", 10U, &parsed));
    assert(lsm_clamp_double(-1.0, 0.0, 100.0) == 0.0);
    assert(lsm_clamp_double(101.0, 0.0, 100.0) == 100.0);

    char path[32];
    assert(lsm_join_path(path, sizeof(path), "/sys/", "device"));
    assert(strcmp(path, "/sys/device") == 0);
    assert(!lsm_join_path(path, 4, "/sys/", "device"));

    char resolved[512];
    assert(lsm_realpath_copy(".", resolved, sizeof(resolved)));
    assert(resolved[0] == '/');
    char too_small[2];
    assert(!lsm_realpath_copy(".", too_small, sizeof(too_small)));
    assert(too_small[0] == '\0');


    char temporary[] = "/tmp/lsm-common-XXXXXX";
    const int descriptor = mkstemp(temporary);
    assert(descriptor >= 0);
    const char numeric[] = "  18446744073709551615\n";
    assert(write(descriptor, numeric, sizeof(numeric) - 1U) ==
           (ssize_t)(sizeof(numeric) - 1U));
    assert(close(descriptor) == 0);

    char file_text[64];
    assert(lsm_read_text_file(temporary, file_text, sizeof(file_text)));
    assert(strcmp(file_text, "  18446744073709551615") == 0);
    uint64_t maximum = 0U;
    assert(lsm_read_u64_file(temporary, &maximum));
    assert(maximum == UINT64_MAX);

    const int overflow_descriptor = open(temporary, O_WRONLY | O_TRUNC);
    assert(overflow_descriptor >= 0);
    const char overflow[] = "18446744073709551616";
    assert(write(overflow_descriptor, overflow, sizeof(overflow) - 1U) ==
           (ssize_t)(sizeof(overflow) - 1U));
    assert(close(overflow_descriptor) == 0);
    assert(!lsm_read_u64_file(temporary, &maximum));
    assert(unlink(temporary) == 0);

    char quantity[32];
    assert(strcmp(lsm_format_bytes(1023U, quantity, sizeof(quantity)),
                  "1023 B") == 0);
    assert(strcmp(lsm_format_bytes(1024U, quantity, sizeof(quantity)),
                  "1.0 KB") == 0);
    assert(strcmp(lsm_format_bytes(1536, quantity, sizeof(quantity)),
                  "1.5 KB") == 0);
    assert(strcmp(lsm_format_bytes(1ULL << 20U, quantity, sizeof(quantity)),
                  "1.0 MB") == 0);
    assert(strcmp(lsm_format_bytes(1ULL << 30U, quantity, sizeof(quantity)),
                  "1.0 GB") == 0);
    assert(strcmp(lsm_format_bytes(1ULL << 40U, quantity, sizeof(quantity)),
                  "1.0 TB") == 0);
    assert(strcmp(lsm_format_rate(1024.0, quantity, sizeof(quantity)),
                  "1.0 KB/s") == 0);
    assert(strcmp(lsm_format_rate(1536.0, quantity, sizeof(quantity)),
                  "1.5 KB/s") == 0);
    assert(strcmp(lsm_format_rate(NAN, quantity, sizeof(quantity)),
                  "0 B/s") == 0);
    assert(lsm_u64_add_saturating(UINT64_MAX, 1U) == UINT64_MAX);
    assert(lsm_u64_multiply_saturating(UINT64_MAX, 2U) == UINT64_MAX);
    assert(lsm_u64_multiply_saturating(1024U, 1024U) == (1ULL << 20U));
    assert(fabs(lsm_percent_u64(1U, 8U) - 12.5) < 0.000001);
    assert(lsm_percent_u64(9U, 8U) == 100.0);
    assert(lsm_percent_u64(1U, 0U) == 0.0);
    double rate = -1.0;
    assert(lsm_u64_counter_rate(12U, 10U, 512.0L, 2.0, &rate));
    assert(rate == 512.0);
    assert(!lsm_u64_counter_rate(9U, 10U, 1.0L, 1.0, &rate));
    assert(rate == 0.0);
    assert(!lsm_u64_counter_rate(12U, 10U, 1.0L, NAN, &rate));
    assert(rate == 0.0);
    assert(lsm_monotonic_seconds() > 0.0);

    puts("Common utility smoke test passed.");
    return 0;
}
