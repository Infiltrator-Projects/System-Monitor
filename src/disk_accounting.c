// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file disk_accounting.c
 * @brief Overflow-safe physical-disk rate, activity and latency accounting.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "disk_accounting.h"

#include "common.h"

#include <limits.h>
#include <math.h>

void lsm_disk_accounting_update(LsmDiskInfo *disk,
                                LsmDiskAccountingState *state,
                                const LsmDiskCounters *counters,
                                double elapsed_seconds)
{
    if (!disk || !state || !counters) return;
    disk->read_bytes_total = lsm_u64_multiply_saturating(
        counters->read_sectors, 512U);
    disk->write_bytes_total = lsm_u64_multiply_saturating(
        counters->write_sectors, 512U);
    disk->in_progress_operations =
        counters->in_progress_operations > UINT_MAX
            ? UINT_MAX : (unsigned)counters->in_progress_operations;
    if (state->initialized) {
        (void)lsm_u64_counter_rate(
            counters->read_sectors, state->previous_read_sectors,
            512.0L, elapsed_seconds, &disk->read_bytes_per_sec);
        (void)lsm_u64_counter_rate(
            counters->write_sectors, state->previous_write_sectors,
            512.0L, elapsed_seconds, &disk->write_bytes_per_sec);

        double io_milliseconds_per_second = 0.0;
        if (lsm_u64_counter_rate(
                counters->io_ms, state->previous_io_ms, 1.0L,
                elapsed_seconds, &io_milliseconds_per_second))
            disk->active_percent = lsm_clamp_double(
                io_milliseconds_per_second / 10.0, 0.0, 100.0);
        else
            disk->active_percent = 0.0;

        const bool read_valid =
            counters->read_operations >= state->previous_read_operations &&
            counters->read_ms >= state->previous_read_ms;
        const bool write_valid =
            counters->write_operations >= state->previous_write_operations &&
            counters->write_ms >= state->previous_write_ms;
        const uint64_t read_operations = read_valid
            ? counters->read_operations - state->previous_read_operations : 0U;
        const uint64_t write_operations = write_valid
            ? counters->write_operations - state->previous_write_operations : 0U;
        const uint64_t read_time = read_valid
            ? counters->read_ms - state->previous_read_ms : 0U;
        const uint64_t write_time = write_valid
            ? counters->write_ms - state->previous_write_ms : 0U;
        disk->read_response_ms = read_operations > 0U
            ? (double)read_time / (double)read_operations : 0.0;
        disk->write_response_ms = write_operations > 0U
            ? (double)write_time / (double)write_operations : 0.0;
        const uint64_t operations = lsm_u64_add_saturating(
            read_operations, write_operations);
        const uint64_t operation_time = lsm_u64_add_saturating(
            read_time, write_time);
        disk->average_response_ms = operations > 0U
            ? (double)operation_time / (double)operations : 0.0;
        double weighted_milliseconds_per_second = 0.0;
        if (lsm_u64_counter_rate(
                counters->weighted_io_ms, state->previous_weighted_io_ms,
                1.0L, elapsed_seconds,
                &weighted_milliseconds_per_second))
            disk->queue_length = fmax(
                0.0, weighted_milliseconds_per_second / 1000.0);
        else
            disk->queue_length = 0.0;
    }

    state->previous_read_operations = counters->read_operations;
    state->previous_read_sectors = counters->read_sectors;
    state->previous_read_ms = counters->read_ms;
    state->previous_write_operations = counters->write_operations;
    state->previous_write_sectors = counters->write_sectors;
    state->previous_write_ms = counters->write_ms;
    state->previous_io_ms = counters->io_ms;
    state->previous_weighted_io_ms = counters->weighted_io_ms;
    state->initialized = true;
}
