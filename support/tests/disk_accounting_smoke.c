// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file disk_accounting_smoke.c
 * @brief Disk rate, latency and reset regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "disk_accounting.h"

#include <math.h>
#include <stdio.h>

static bool near(double value, double expected)
{
    return fabs(value - expected) < 0.0001;
}

int main(void)
{
    LsmDiskInfo disk = {0};
    LsmDiskAccountingState state = {0};
    const LsmDiskCounters first = {
        .read_operations = 100U,
        .read_sectors = 1000U,
        .read_ms = 500U,
        .write_operations = 50U,
        .write_sectors = 2000U,
        .write_ms = 400U,
        .in_progress_operations = 2U,
        .io_ms = 1000U,
        .weighted_io_ms = 1500U
    };
    lsm_disk_accounting_update(&disk, &state, &first, 2.0);
    if (!state.initialized || disk.read_bytes_per_sec != 0.0 ||
        disk.read_bytes_total != 512000U ||
        disk.write_bytes_total != 1024000U ||
        disk.in_progress_operations != 2U)
        return 1;

    const LsmDiskCounters second = {
        .read_operations = 110U,
        .read_sectors = 3048U,
        .read_ms = 550U,
        .write_operations = 55U,
        .write_sectors = 6096U,
        .write_ms = 440U,
        .in_progress_operations = 3U,
        .io_ms = 2000U,
        .weighted_io_ms = 2500U
    };
    lsm_disk_accounting_update(&disk, &state, &second, 2.0);
    if (!near(disk.read_bytes_per_sec, 524288.0) ||
        !near(disk.write_bytes_per_sec, 1048576.0) ||
        !near(disk.active_percent, 50.0) ||
        !near(disk.read_response_ms, 5.0) ||
        !near(disk.write_response_ms, 8.0) ||
        !near(disk.average_response_ms, 6.0) ||
        !near(disk.queue_length, 0.5) ||
        disk.in_progress_operations != 3U ||
        disk.read_bytes_total != 1560576U ||
        disk.write_bytes_total != 3121152U)
        return 2;

    const LsmDiskCounters reset = {0};
    lsm_disk_accounting_update(&disk, &state, &reset, 1.0);
    if (disk.read_bytes_per_sec != 0.0 || disk.write_bytes_per_sec != 0.0 ||
        disk.active_percent != 0.0 || disk.average_response_ms != 0.0 ||
        disk.queue_length != 0.0 || disk.in_progress_operations != 0U ||
        disk.read_bytes_total != 0U || disk.write_bytes_total != 0U)
        return 3;
    puts("Disk rates, totals, queue depth and counter-reset handling passed.");
    return 0;
}
