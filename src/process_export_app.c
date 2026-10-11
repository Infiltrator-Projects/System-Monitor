// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_export_app.c
 * @brief LsmApp and GTK adapter for data-only process export.
 *
 * Application-owned snapshot/selection state and dialog/clipboard lifetime stay
 * here so process_export.c remains a plain-data serializer with an independent
 * regression fixture.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_export.h"

#include "app_presentation_context.h"
#include "app_process_context.h"
#include "common.h"
#include "ui_helpers.h"

#include <stdio.h>
#include <string.h>

static bool export_view_from_app(const LsmApp *app, LsmProcessExportView *view)
{
    if (!view) return false;
    memset(view, 0, sizeof(*view));

    LsmProcessSelectionView selection;
    if (!lsm_app_process_selection_view(app, &selection)) return false;
    view->processes = selection.processes;
    view->process_count = selection.process_count;
    view->selected_pid = selection.selected_pid;
    view->selected_instance_id = selection.selected_instance_id;
    view->selected_group_pids = selection.selected_group_pids;
    view->selected_group_instance_ids = selection.selected_group_instance_ids;
    view->selected_group_count = selection.selected_group_count;
    return true;
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

static bool spreadsheet_needs_formula_escape(const char *text)
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
    if (spreadsheet_needs_formula_escape(text))
        g_string_append(destination, "'");
    for (const unsigned char *cursor = (const unsigned char *)text;
         *cursor; cursor++) {
        if (*cursor >= 32U && *cursor != 127U && *cursor != '\t')
            g_string_append_printf(destination, "%c", (char)*cursor);
        else
            g_string_append(destination, " ");
    }
}

bool lsm_process_export_selected_csv(const LsmApp *app, const char *path,
                                     char *error, size_t error_size)
{
    LsmProcessExportView view;
    if (!export_view_from_app(app, &view))
        return lsm_process_export_write_view(NULL, path, error, error_size);
    return lsm_process_export_write_view(&view, path, error, error_size);
}

void lsm_process_export_copy_selected(const LsmApp *app)
{
    LsmProcessExportView view;
    if (!export_view_from_app(app, &view) || selected_count(&view) == 0U)
        return;

    GString *text = g_string_new(
        "Name\tPID\tUser\tCPU\tMemory\tGPU\tGPU engine\tCommand\n");
    if (!text) return;
    for (size_t index = 0U; index < view.process_count; index++) {
        const LsmProcessInfo *process = &view.processes[index];
        if (!process_selected(&view, process)) continue;
        char memory[64];
        char gpu[32];
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
    LsmProcessExportView view;
    GtkWidget *window = lsm_app_presentation_window(app);
    GtkWindow *parent = window ? GTK_WINDOW(window) : NULL;
    if (!export_view_from_app(app, &view) || selected_count(&view) == 0U) {
        if (app)
            lsm_ui_show_error(
                parent, "Nothing selected",
                "Select a process or application group in Processes or Details first.");
        return;
    }

    GtkWidget *chooser = gtk_file_chooser_dialog_new(
        "Export selected process rows", parent, GTK_FILE_CHOOSER_ACTION_SAVE,
        "Cancel", GTK_RESPONSE_CANCEL, "Export", GTK_RESPONSE_ACCEPT, NULL);
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
        if (path && !lsm_process_export_write_view(
                        &view, path, error, sizeof(error)))
            lsm_ui_show_error(parent, "Export failed", "%s", error);
        g_free(path);
    }
    gtk_widget_destroy(chooser);
}
