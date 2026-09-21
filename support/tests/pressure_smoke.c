// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file pressure_smoke.c
 * @brief Linux Pressure Stall Information parser regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "pressure.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool close_enough(double left, double right)
{
    return fabs(left - right) < 0.000001;
}

static int fail(const char *message)
{
    fprintf(stderr, "pressure smoke: %s\n", message);
    return 1;
}

int main(void)
{
    LsmPressureInfo pressure = {0};
    if (!lsm_pressure_parse(
            "some avg10=1.25 avg60=2.50 avg300=3.75 total=123456\n",
            &pressure))
        return fail("some-only record did not parse");
    if (!pressure.available || pressure.full_available ||
        !close_enough(pressure.some_avg10, 1.25) ||
        !close_enough(pressure.some_avg60, 2.50) ||
        !close_enough(pressure.some_avg300, 3.75) ||
        pressure.some_total_us != 123456U)
        return fail("some-only values were not preserved");

    if (!lsm_pressure_parse(
            "some avg10=0.10 avg60=0.20 avg300=0.30 total=42 future=7\n"
            "full avg10=4.10 avg60=4.20 avg300=4.30 total=84\n",
            &pressure))
        return fail("some/full record did not parse");
    if (!pressure.full_available ||
        !close_enough(pressure.full_avg10, 4.10) ||
        pressure.full_total_us != 84U)
        return fail("full-pressure values were not preserved");

    pressure.available = true;
    if (lsm_pressure_parse(
            "some avg10=-1 avg60=2 avg300=3 total=4\n", &pressure) ||
        pressure.available)
        return fail("invalid pressure was accepted");

    char path[] = "/tmp/lsm-pressure-XXXXXX";
    const int descriptor = mkstemp(path);
    if (descriptor < 0) return fail("unable to create fixture");
    static const char fixture[] =
        "some avg10=7.00 avg60=8.00 avg300=9.00 total=1000\n"
        "full avg10=1.00 avg60=2.00 avg300=3.00 total=2000\n";
    const ssize_t expected = (ssize_t)(sizeof(fixture) - 1U);
    if (write(descriptor, fixture, sizeof(fixture) - 1U) != expected) {
        (void)close(descriptor);
        (void)unlink(path);
        return fail("unable to write fixture");
    }
    if (close(descriptor) != 0) {
        (void)unlink(path);
        return fail("unable to close fixture");
    }
    const bool read_ok = lsm_pressure_read(path, &pressure);
    const bool values_ok =
        read_ok &&
        close_enough(pressure.some_avg10, 7.0) &&
        close_enough(pressure.full_avg300, 3.0);
    const int unlink_result = unlink(path);
    if (!values_ok || unlink_result != 0)
        return fail("file-backed pressure read failed");

    puts("Pressure Stall Information parser passed.");
    return 0;
}
