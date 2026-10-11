// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_export.c
 * @brief Data-only CSV serialization for selected process rows.
 *
 * The serializer consumes completed plain-C process data and immutable selection
 * identity. Application storage, GTK widgets and dialog lifetime are adapted in
 * process_export_app.c so this layer remains independently testable.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "process_export.h"

#include "atomic_file.h"
#include "common.h"

#include <infiltratr/escape.h>
#include <infiltratr/format.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t size, const char *message)
{
    lsm_copy_string(error, size, message ? message : "Unknown error");
}

static bool process_selected(const LsmProcessExportView *view,
                             const LsmProcessInfo *process)
{
    if (!view || !process) return false;
    if (view->selected_group_count == 0U)
        return process->pid == view->selected_pid &&
            process->instance_id == view->selected_instance_id;
    if (!view->selected_group_pids || !view->selected_group_instance_ids)
        return false;
    for (size_t index = 0U; index < view->selected_group_count; index++)
        if (view->selected_group_pids[index] == process->pid &&
            view->selected_group_instance_ids[index] == process->instance_id)
            return true;
    return false;
}

static size_t selected_count(const LsmProcessExportView *view)
{
    if (!view || view->selected_pid <= 0) return 0U;
    size_t count = 0U;
    for (size_t index = 0U; index < view->process_count; index++)
        if (process_selected(view, &view->processes[index]))
            count++;
    return count;
}

static bool csv_field(FILE *file, const char *text)
{
    if (!file) return false;
    if (!text) text = "";

    size_t required = 0U;
    if (!infiltratr_escape_csv_field(text, true, NULL, 0U, &required))
        return false;

    char stack[256];
    char *encoded = required <= sizeof(stack) ? stack : malloc(required);
    if (!encoded) return false;

    const bool encoded_ok = infiltratr_escape_csv_field(
        text, true, encoded, required, NULL);
    const bool written_ok = encoded_ok && fputs(encoded, file) >= 0;
    if (encoded != stack) free(encoded);
    return written_ok;
}

static bool csv_row(FILE *file, const LsmProcessInfo *process)
{
    char cpu_percent[64];
    char read_rate[64];
    char write_rate[64];
    char gpu_percent[64];
    if (!infiltratr_format_fixed_ascii(
            process->cpu_percent, 3U, cpu_percent, sizeof(cpu_percent)) ||
        (process->io_rate_available &&
         (!infiltratr_format_fixed_ascii(
              process->read_bytes_per_sec, 3U, read_rate, sizeof(read_rate)) ||
          !infiltratr_format_fixed_ascii(
              process->write_bytes_per_sec, 3U, write_rate,
              sizeof(write_rate)))) ||
        (process->gpu_available &&
         !infiltratr_format_fixed_ascii(
             process->gpu_percent, 3U, gpu_percent, sizeof(gpu_percent))))
        return false;
    if (!process->io_rate_available) {
        read_rate[0] = '\0';
        write_rate[0] = '\0';
    }
    if (!csv_field(file, process->name)) return false;
    fprintf(file, ",%llu,%llu,",
            (unsigned long long)process->pid,
            (unsigned long long)process->ppid);
    if (!csv_field(file, process->user)) return false;
    fputc(',', file);
    if (!csv_field(file, process->state)) return false;
    fprintf(file, ",%s,%llu,%llu,%u,%s,%s,",
            cpu_percent,
            (unsigned long long)process->cpu_time_nanoseconds,
            (unsigned long long)process->rss_bytes, process->threads,
            read_rate, write_rate);
    if (process->gpu_available) fputs(gpu_percent, file);
    fputc(',', file);
    if (!csv_field(file, process->gpu_engine[0] ? process->gpu_engine : "N/A"))
        return false;
    fprintf(file, ",%llu,", (unsigned long long)process->gpu_memory_bytes);
    if (process->io_totals_available)
        fprintf(file, "%llu,%llu,",
                (unsigned long long)process->read_bytes,
                (unsigned long long)process->write_bytes);
    else
        fputs(",,", file);
    if (process->handle_count_available)
        fprintf(file, "%u,", process->handle_count);
    else
        fputc(',', file);
    fprintf(file, "%llu,%llu,",
            (unsigned long long)process->context_switches,
            (unsigned long long)process->page_faults);
    if (!csv_field(file, lsm_process_priority_name(process->priority)))
        return false;
    fputc(',', file);
    if (!csv_field(file, process->executable)) return false;
    fputc(',', file);
    if (!csv_field(file, process->command)) return false;
    fputc('\n', file);
    return ferror(file) == 0;
}

static bool write_process_export(FILE *file, const void *user_data)
{
    const LsmProcessExportView *view = user_data;
    fputs("Name,PID,Parent PID,User,Status,CPU (%),CPU time (ns),"
          "Memory (bytes),Threads,Read (bytes/s),Write (bytes/s),GPU (%),"
          "GPU engine,GPU memory (bytes),Read total (bytes),Write total (bytes),"
          "Handles,Context switches,Page faults,Priority,Executable,Command\n",
          file);
    for (size_t index = 0U; index < view->process_count; index++) {
        const LsmProcessInfo *process = &view->processes[index];
        if (process_selected(view, process) && !csv_row(file, process))
            return false;
    }
    return ferror(file) == 0;
}

bool lsm_process_export_write_view(const LsmProcessExportView *view,
                                   const char *path, char *error,
                                   size_t error_size)
{
    if (error && error_size > 0U) error[0] = '\0';
    if (!view || !path || !*path) {
        set_error(error, error_size, "No destination file was selected.");
        return false;
    }
    if (selected_count(view) == 0U) {
        set_error(error, error_size,
                  "Select a process or application group first.");
        return false;
    }
    const int failure = lsm_atomic_file_write(
        path, LSM_ATOMIC_FILE_USER_DOCUMENT, write_process_export, view);
    if (failure == 0) return true;
    set_error(error, error_size, strerror(failure));
    return false;
}
