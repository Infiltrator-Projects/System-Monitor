// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file duration_format_smoke.c
 * @brief Elapsed and estimated duration presentation regression.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "duration_format.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect_clock(uint64_t seconds, const char *expected)
{
    char text[64];
    lsm_duration_format_clock(seconds, text, sizeof(text));
    assert(strcmp(text, expected) == 0);
}

static void expect_remaining(uint64_t seconds, const char *expected)
{
    char text[64];
    lsm_duration_format_remaining(seconds, text, sizeof(text));
    assert(strcmp(text, expected) == 0);
}

int main(void)
{
    expect_clock(0U, "00:00:00");
    expect_clock(3661U, "01:01:01");
    expect_clock(90061U, "1d 01:01:01");
    expect_remaining(0U, "N/A");
    expect_remaining(59U, "0m");
    expect_remaining(3661U, "1h 01m");
    expect_remaining(176460U, "2d 01h 01m");
    puts("Elapsed and remaining duration formatting passed.");
    return 0;
}
