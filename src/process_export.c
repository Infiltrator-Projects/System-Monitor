// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_export.c
 * @brief Selection-aware clipboard and CSV presentation for process rows.
 *
 * This module reads the already collected snapshot. It does not run commands,
 * rescan procfs, or change process state.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "process_export.h"

#include "app_internal.h"
#include "atomic_file.h"
#include "common.h"
#include "ui_helpers.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t size, const char *message)
{
    if (!error || size == 0U) return;
    (void)snprintf(error, size, "%s", message ? message : "Unknown error");
}

static bool process_selected(const LsmApp *app,
                             const LsmProcessInfo *process)
{
    if (!app || !process) return false;
    if (app->process.selected_group_count == 0U)
        return process->pid == app->process.selected_pid &&
            process->instance_id == app->process.selected_instance_id;
    if (!app->process.selected_group_pids ||
        !app->process.selected_group_instance_ids)
        return false;
    for (size_t index = 0U; index < app->process.selected_group_count; index++)
        if (app->process.selected_group_pids[index] == process->pid &&
            app->process.selected_group_instance_ids[index] ==
                process->instance_id)
            return true;
    return false;
}

static size_t selected_count(const LsmApp *app)
{
    if (!app || app->process.selected_pid <= 0) return 0U;
    size_t count = 0U;
    for (size_t index = 0U; index < app->process.process_snapshot_count; index++)
        if (process_selected(app, &app->process.process_snapshot[index]))
            count++;
    return count;
}

static bool csv_needs_formula_escape(const char *text)
{
    if (!text) return false;
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor) {
        if (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' ||
            *cursor == '\n' || *cursor < 32U || *cursor == 127U) {
            cursor++;
            continue;
        }
        return *cursor == '=' || *cursor == '+' || *cursor == '-' ||
               *cursor == '@';
    }
    return false;
}

static void append_spreadsheet_text(GString *destination, const char *text)
{
    if (!destination || !text) return;
    if (csv_needs_formula_escape(text)) g_string_append(destination, "'");
    for (const unsigned char *cursor = (const unsigned char *)text;
         *cursor; cursor++) {
        if (*cursor >= 32U && *cursor != 127U && *cursor != '\t')
            g_string_append_printf(destination, "%c", (char)*cursor);
        else
            g_string_append(destination, " ");
    }
}

static void csv_field(FILE *file, const char *text)
{
    fputc('"', file);
    if (csv_needs_formula_escape(text)) fputc('\'', file);
    if (text) {
        for (const unsigned char *cursor = (const unsigned char *)text;
             *cursor; cursor++) {
            if (*cursor == '"') fputc('"', file);
            if (*cursor >= 32U && *cursor != 127U) fputc(*cursor, file);
            else if (*cursor == '\t') fputc(' ', file);
        }
    }
    fputc('"', file);
}

static void csv_row(FILE *file, const LsmProcessInfo *process)
{
    csv_field(file, process->name);
    fprintf(file, ",%llu,%llu,",
            (unsigned long long)process->pid,
            (unsigned long long)process->ppid);
    csv_field(file, process->user);
    fputc(',', file);
    csv_field(file, process->state);
    fprintf(file, ",%.3f,%llu,%llu,%u,%.3f,%.3f,",
            process->cpu_percent,
            (unsigned long long)process->cpu_time_nanoseconds,
            (unsigned long long)process->rss_bytes, process->threads,
            process->read_bytes_per_sec, process->write_bytes_per_sec);
    if (process->gpu_available) fprintf(file, "%.3f", process->gpu_percent);
    fputc(',', file);
    csv_field(file, process->gpu_engine[0] ? process->gpu_engine : "N/A");
    fprintf(file, ",%llu,%llu,%llu,%u,%llu,%llu,",
            (unsigned long long)process->gpu_memory_bytes,
            (unsigned long long)process->read_bytes,
            (unsigned long long)process->write_bytes, process->handle_count,
            (unsigned long long)process->context_switches,
            (unsigned long long)process->page_faults);
    csv_field(file, lsm_process_priority_name(process->priority));
    fputc(',', file);
    csv_field(file, process->executable);
    fputc(',', file);
    csv_field(file, process->command);
    fputc('\n', file);
}

static bool write_process_export(FILE *file, const void *user_data)
{
    const LsmApp *app = user_data;
    fputs("Name,PID,Parent PID,User,Status,CPU (%),CPU time (ns),"
          "Memory (bytes),Threads,Read (bytes/s),Write (bytes/s),GPU (%),"
          "GPU engine,GPU memory (bytes),Read total (bytes),Write total (bytes),"
          "Handles,Context switches,Page faults,Priority,Executable,Command\n",
          file);
    for (size_t index = 0U; index < app->process.process_snapshot_count; index++) {
        const LsmProcessInfo *process = &app->process.process_snapshot[index];
        if (process_selected(app, process)) csv_row(file, process);
    }
    return ferror(file) == 0;
}

bool lsm_process_export_selected_csv(const LsmApp *app, const char *path,
                                     char *error, size_t error_size)
{
    if (error && error_size > 0U) error[0] = '\0';
    if (!app || !path || !*path) {
        set_error(error, error_size, "No destination file was selected.");
        return false;
    }
    if (selected_count(app) == 0U) {
        set_error(error, error_size,
                  "Select a process or application group first.");
        return false;
    }
    const int failure = lsm_atomic_file_write(
        path, LSM_ATOMIC_FILE_USER_DOCUMENT, write_process_export, app);
    if (failure == 0) return true;
    set_error(error, error_size, strerror(failure));
    return false;
}

void lsm_process_export_copy_selected(const LsmApp *app)
{
    if (!app || selected_count(app) == 0U) return;
    GString *text = g_string_new(
        "Name\tPID\tUser\tCPU\tMemory\tGPU\tGPU engine\tCommand\n");
    for (size_t index = 0U; index < app->process.process_snapshot_count; index++) {
        const LsmProcessInfo *process = &app->process.process_snapshot[index];
        if (!process_selected(app, process)) continue;
        char memory[64], gpu[32];
        lsm_format_bytes(process->rss_bytes, memory, sizeof(memory));
        if (process->gpu_available)
            (void)snprintf(gpu, sizeof(gpu), "%.1f%%", process->gpu_percent);
        else
            (void)snprintf(gpu, sizeof(gpu), "N/A");
        append_spreadsheet_text(text, process->name);
        g_string_append_printf(text, "\t%llu\t",
                               (unsigned long long)process->pid);
        append_spreadsheet_text(text, process->user);
        g_string_append_printf(text, "\t%.1f%%\t%s\t%s\t",
                               process->cpu_percent, memory, gpu);
        append_spreadsheet_text(
            text, process->gpu_engine[0] ? process->gpu_engine : "N/A");
        g_string_append(text, "\t");
        append_spreadsheet_text(text, process->command);
        g_string_append(text, "\n");
    }
    GtkClipboard *clipboard = gtk_clipboard_get(
        gdk_atom_intern_static_string("CLIPBOARD"));
    gtk_clipboard_set_text(clipboard, text->str, -1);
    g_string_free(text, TRUE);
}

void lsm_process_export_selected_dialog(LsmApp *app)
{
    if (!app || selected_count(app) == 0U) {
        if (app)
            lsm_ui_show_error(GTK_WINDOW(app->shell.window), "Nothing selected",
                "Select a process or application group in Processes or Details first.");
        return;
    }
    GtkWidget *chooser = gtk_file_chooser_dialog_new(
        "Export selected process rows", GTK_WINDOW(app->shell.window),
        GTK_FILE_CHOOSER_ACTION_SAVE, "Cancel", GTK_RESPONSE_CANCEL,
        "Export", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser),
                                      "selected-processes.csv");
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser),
                                                    TRUE);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "CSV table");
    gtk_file_filter_add_pattern(filter, "*.csv");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        char error[256];
        if (path && !lsm_process_export_selected_csv(app, path, error,
                                                      sizeof(error)))
            lsm_ui_show_error(GTK_WINDOW(app->shell.window), "Export failed",
                              "%s", error);
        g_free(path);
    }
    gtk_widget_destroy(chooser);
}
