// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_actions.c
 * @brief Process filtering, recording and user-initiated control actions.
 *
 * Keeps mutating process operations and their dialogs separate from the
 * Details table model/rendering code. All actions operate on the retained
 * process snapshot and the native process backend.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "details_page.h"
#include "app_internal.h"
#include "atomic_file.h"
#include "process_backend.h"
#include "process_inspector.h"
#include "processes_ui.h"
#include "ui_helpers.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void show_process_backend_error(LsmApp *app, const char *title);

typedef enum {
    ACTION_DETAILS,
    ACTION_END,
    ACTION_END_TREE,
    ACTION_SUSPEND,
    ACTION_RESUME,
    ACTION_EFFICIENCY,
    ACTION_AFFINITY,
    ACTION_RECORD,
    ACTION_COPY_PID,
    ACTION_COPY_PATH,
    ACTION_COPY_COMMAND,
    ACTION_OPEN_DIRECTORY
} ProcessAction;

static const LsmProcessInfo *action_snapshot_process(
    const LsmApp *app, LsmProcessId pid, LsmProcessInstanceId instance_id)
{
    if (!app) return NULL;
    for (size_t index = 0U; index < app->process.process_snapshot_count; index++) {
        const LsmProcessInfo *process = &app->process.process_snapshot[index];
        if (process->pid == pid && process->instance_id == instance_id)
            return process;
    }
    return NULL;
}

void lsm_process_selection_set(LsmApp *app, guint64 pid)
{
    if (!app) return;
    app->process.selected_pid = pid > 0U ? (LsmProcessId)pid : 0U;
    app->process.selected_instance_id = 0U;
    for (size_t index = 0U;
         app->process.selected_pid > 0U &&
         index < app->process.process_snapshot_count; index++) {
        const LsmProcessInfo *process = &app->process.process_snapshot[index];
        if (process->pid == app->process.selected_pid) {
            app->process.selected_instance_id = process->instance_id;
            break;
        }
    }
}

void lsm_process_group_selection_clear(LsmApp *app)
{
    if (!app) return;
    free(app->process.selected_group_pids);
    free(app->process.selected_group_instance_ids);
    app->process.selected_group_pids = NULL;
    app->process.selected_group_instance_ids = NULL;
    app->process.selected_group_count = 0U;
    app->process.selected_group_name[0] = '\0';
}

/* Per-process CSV recording is a UI concern and never changes sampling. */
static int close_record_file(LsmApp *app)
{
    int failure = 0;
    if (app->process.record_file) {
        if (fclose(app->process.record_file) != 0)
            failure = errno ? errno : EIO;
        app->process.record_file = NULL;
    }
    app->process.recording_pid = 0;
    app->process.recording_instance_id = 0U;
    return failure;
}

static void mark_recording_stopped(LsmApp *app)
{
    if (!app || !app->details.process_record_menu_item) return;
    gtk_menu_item_set_label(GTK_MENU_ITEM(app->details.process_record_menu_item),
                            "Record selected process");
    gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM(app->details.process_record_menu_item), FALSE);
}

void lsm_process_record_stop(LsmApp *app)
{
    if (!app) return;
    const int failure = close_record_file(app);
    mark_recording_stopped(app);
    if (failure != 0 && app->shell.window && !app->runtime.shutting_down)
        lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                          "Unable to finish process recording", "%s",
                          g_strerror(failure));
}

static gchar *selected_process_name(LsmApp *app)
{
    const LsmProcessInfo *process = action_snapshot_process(
        app, app->process.selected_pid, app->process.selected_instance_id);
    return process ? g_strdup(process->name) : NULL;
}

void lsm_process_record_set(LsmApp *app, gboolean active)
{
    if (!app) return;
    if (!active) {
        lsm_process_record_stop(app);
        return;
    }
    if (app->process.selected_pid <= 1 ||
        app->process.selected_instance_id == 0U) {
        if (app->details.process_record_menu_item)
            gtk_check_menu_item_set_active(
                GTK_CHECK_MENU_ITEM(app->details.process_record_menu_item), FALSE);
        return;
    }

    char directory[LSM_PATH_LEN];
    snprintf(directory, sizeof(directory), "%s/%s", g_get_home_dir(), LSM_LOG_DIRECTORY);
    if (g_mkdir_with_parents(directory, 0700) != 0) {
        lsm_ui_show_error(GTK_WINDOW(app->shell.window), "Unable to create the log directory", "%s", g_strerror(errno));
        if (app->details.process_record_menu_item)
            gtk_check_menu_item_set_active(
                GTK_CHECK_MENU_ITEM(app->details.process_record_menu_item), FALSE);
        return;
    }

    gchar *name = selected_process_name(app);
    if (!name) name = g_strdup("process");
    for (char *p = name; *p; p++)
        if (!g_ascii_isalnum(*p) && *p != '-' && *p != '_') *p = '_';
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", &local);
    char filename[256];
    snprintf(filename, sizeof(filename), "%.160s-%llu-%s.csv", name,
             (unsigned long long)app->process.selected_pid, timestamp);
    char *full_path = g_build_filename(directory, filename, NULL);
    g_strlcpy(app->process.record_path, full_path, sizeof(app->process.record_path));
    g_free(full_path);
    g_free(name);

    app->process.record_file = fopen(app->process.record_path, "w");
    if (!app->process.record_file) {
        lsm_ui_show_error(GTK_WINDOW(app->shell.window), "Unable to start recording", "%s", g_strerror(errno));
        if (app->details.process_record_menu_item)
            gtk_check_menu_item_set_active(
                GTK_CHECK_MENU_ITEM(app->details.process_record_menu_item), FALSE);
        return;
    }
    errno = 0;
    const int header_result = fprintf(app->process.record_file,
        "timestamp,pid,cpu_percent,memory_percent,rss_bytes,read_bytes,write_bytes,threads\n");
    const int flush_result = header_result >= 0
        ? fflush(app->process.record_file) : EOF;
    if (header_result < 0 || flush_result != 0) {
        const int failure = errno ? errno : EIO;
        (void)close_record_file(app);
        mark_recording_stopped(app);
        lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                          "Unable to start recording", "%s",
                          g_strerror(failure));
        return;
    }
    app->process.recording_pid = app->process.selected_pid;
    app->process.recording_instance_id = app->process.selected_instance_id;
    char label[96];
    snprintf(label, sizeof(label), "Stop recording PID %llu",
             (unsigned long long)app->process.recording_pid);
    if (app->details.process_record_menu_item)
        gtk_menu_item_set_label(GTK_MENU_ITEM(app->details.process_record_menu_item),
                                label);
}

gboolean lsm_process_record_append(LsmApp *app,
                                   const LsmProcessInfo *process)
{
    if (!app || !process || !app->process.record_file) return FALSE;

    time_t now = time(NULL);
    char timestamp[64];
    struct tm local;
    localtime_r(&now, &local);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z", &local);

    errno = 0;
    const int write_result = fprintf(app->process.record_file,
        "%s,%llu,%.3f,%.3f,%llu,%llu,%llu,%u\n",
        timestamp, (unsigned long long)process->pid,
        process->cpu_percent, process->memory_percent,
        (unsigned long long)process->rss_bytes,
        (unsigned long long)process->read_bytes,
        (unsigned long long)process->write_bytes,
        process->threads);
    const int flush_result = write_result >= 0
        ? fflush(app->process.record_file) : EOF;
    if (write_result >= 0 && flush_result == 0) return TRUE;

    const int failure = errno ? errno : EIO;
    (void)close_record_file(app);
    mark_recording_stopped(app);
    if (app->shell.window && !app->runtime.shutting_down)
        lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                          "Process recording stopped",
                          "The recording file could not be written: %s",
                          g_strerror(failure));
    return FALSE;
}

void lsm_process_filters_load(LsmApp *app)
{
    if (!app->process.filters) app->process.filters = g_ptr_array_new_with_free_func(g_free);
    else g_ptr_array_set_size(app->process.filters, 0);

    gchar *content = NULL;
    gsize length = 0;
    if (!g_file_get_contents(app->paths.filter_path, &content, &length, NULL)) return;
    gchar **lines = g_strsplit(content, "\n", -1);
    for (gchar **line = lines; *line; line++) {
        gchar *trimmed = g_strstrip(*line);
        if (*trimmed && *trimmed != '#') g_ptr_array_add(app->process.filters, g_strdup(trimmed));
    }
    g_strfreev(lines);
    g_free(content);
}

void lsm_process_filters_dialog(LsmApp *app)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Process filters", GTK_WINDOW(app->shell.window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 540, 420);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    GtkWidget *description = gtk_label_new(
        "Enter one exclusion term per line. A process is hidden when its name, owner, or command contains a term.\n"
        "The search box remains an inclusive, temporary filter.");
    gtk_label_set_line_wrap(GTK_LABEL(description), TRUE);
    gtk_widget_set_halign(description, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(content), description, FALSE, FALSE, 0);

    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroller, TRUE);
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_container_add(GTK_CONTAINER(scroller), text_view);
    gtk_box_pack_start(GTK_BOX(content), scroller, TRUE, TRUE, 8);

    GString *current = g_string_new(NULL);
    for (guint i = 0; i < app->process.filters->len; i++)
        g_string_append_printf(current, "%s\n", (char *)g_ptr_array_index(app->process.filters, i));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    gtk_text_buffer_set_text(buffer, current->str, -1);
    g_string_free(current, TRUE);

    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GtkTextIter start, end;
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        gchar *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
        int failure = 0;
        if (g_mkdir_with_parents(app->paths.config_dir, 0700) != 0)
            failure = errno;
        else
            failure = lsm_atomic_file_write_bytes(
                app->paths.filter_path, LSM_ATOMIC_FILE_PRIVATE,
                text, strlen(text));
        g_free(text);
        if (failure != 0) {
            lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                              "Unable to save process filters", "%s",
                              g_strerror(failure));
        } else {
            lsm_process_filters_load(app);
            app->processes.processes_model_dirty = TRUE;
            app->details.details_model_dirty = TRUE;
            lsm_details_present_snapshot(app);
            lsm_processes_present_snapshot(app);
        }
    }
    gtk_widget_destroy(dialog);
}

static void copy_text(const char *text)
{
    if (!text) return;
    GtkClipboard *clipboard = gtk_clipboard_get(gdk_atom_intern_static_string("CLIPBOARD"));
    gtk_clipboard_set_text(clipboard, text, -1);
}

static void show_affinity_dialog(LsmApp *app)
{
    const LsmProcessId pid = app->process.selected_pid;
    const LsmProcessInstanceId instance_id = app->process.selected_instance_id;
    bool enabled[LSM_MAX_CPUS] = {0};
    size_t count = lsm_process_affinity_get(
        pid, instance_id, enabled, LSM_MAX_CPUS);
    if (!count) {
        show_process_backend_error(app, "Unable to read CPU affinity");
        return;
    }

    GtkWidget *dialog = gtk_dialog_new_with_buttons("Set CPU affinity", GTK_WINDOW(app->shell.window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, "Apply", GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 620, 480);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    GtkWidget *description = gtk_label_new(
        "Select the logical processors on which this process may run.");
    gtk_widget_set_halign(description, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(content), description, FALSE, FALSE, 0);
    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroller, TRUE);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_container_add(GTK_CONTAINER(scroller), grid);
    gtk_box_pack_start(GTK_BOX(content), scroller, TRUE, TRUE, 8);

    GtkWidget **checks = g_new0(GtkWidget *, count);
    for (size_t i = 0; i < count; i++) {
        char label[32];
        snprintf(label, sizeof(label), "CPU %zu", i);
        checks[i] = gtk_check_button_new_with_label(label);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checks[i]), enabled[i]);
        gtk_grid_attach(GTK_GRID(grid), checks[i], (int)(i % 6), (int)(i / 6), 1, 1);
    }

    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        bool any = false;
        for (size_t i = 0; i < count; i++) {
            enabled[i] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checks[i]));
            any = any || enabled[i];
        }
        if (!any) lsm_ui_show_error(GTK_WINDOW(app->shell.window), "Unable to set CPU affinity", "At least one CPU must be selected.");
        else if (!lsm_process_affinity_set(pid, instance_id, enabled, count))
            show_process_backend_error(app, "Unable to set CPU affinity");
    }
    g_free(checks);
    gtk_widget_destroy(dialog);
}

static gboolean confirm_end(LsmApp *app, gboolean tree)
{
    const gboolean group =
        !tree && app->process.selected_group_count > 1U &&
        app->process.selected_group_name[0] != '\0';
    GtkWidget *dialog = NULL;
    if (tree) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(app->shell.window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
            "End process tree %llu?",
            (unsigned long long)app->process.selected_pid);
    } else if (group) {
        dialog = gtk_message_dialog_new(GTK_WINDOW(app->shell.window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
            "End task %s?", app->process.selected_group_name);
    } else {
        dialog = gtk_message_dialog_new(GTK_WINDOW(app->shell.window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE,
            "End process %llu?",
            (unsigned long long)app->process.selected_pid);
    }
    if (tree) {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
            "All current descendants will be asked to terminate before the selected process.");
    } else if (group) {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
            "All %zu processes currently belonging to this application group "
            "will be asked to terminate.", app->process.selected_group_count);
    } else {
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
            LSM_PROGRAM_NAME " will ask the process to terminate cleanly.");
    }
    gtk_dialog_add_buttons(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL,
                           tree ? "End process tree" :
                           group ? "End task" : "End process",
                           GTK_RESPONSE_ACCEPT, NULL);
    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    return response == GTK_RESPONSE_ACCEPT;
}

static void open_executable_directory(LsmApp *app)
{
    const LsmProcessInfo *snapshot = action_snapshot_process(
        app, app->process.selected_pid, app->process.selected_instance_id);
    if (!snapshot) return;
    LsmProcessInfo process = *snapshot;
    if (!lsm_process_enrich(
            process.pid, &process, LSM_PROCESS_SCAN_EXECUTABLE))
        return;
    if (!process.executable[0]) {
        lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                          "Executable path is unavailable",
                          "The process may have exited, expose no executable image, "
                          "or restrict access to that information.");
        return;
    }
    char *directory = g_path_get_dirname(process.executable);
    GError *error = NULL;
    char *uri = g_filename_to_uri(directory, NULL, &error);
    if (!uri || !gtk_show_uri_on_window(GTK_WINDOW(app->shell.window), uri, GDK_CURRENT_TIME, &error)) {
        lsm_ui_show_error(GTK_WINDOW(app->shell.window), "Unable to open the executable directory", "%s",
                   error ? error->message : "Unknown error");
    }
    if (error) g_error_free(error);
    g_free(uri);
    g_free(directory);
}

static void show_process_backend_error(LsmApp *app, const char *title)
{
    char error[160];
    lsm_process_error_message(error, sizeof(error));
    lsm_ui_show_error(GTK_WINDOW(app->shell.window), title, "%s", error);
}

static void process_action_activate(GtkMenuItem *item, gpointer user_data)
{
    LsmApp *app = user_data;
    const LsmProcessId pid = app->process.selected_pid;
    const LsmProcessInstanceId instance_id = app->process.selected_instance_id;
    ProcessAction action = (ProcessAction)GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(item), "lsm-action"));
    const LsmProcessInfo *process = action_snapshot_process(
        app, pid, instance_id);
    if (!process) return;

    switch (action) {
        case ACTION_DETAILS:
            lsm_process_inspector_show(app, pid, instance_id);
            break;
        case ACTION_END: lsm_processes_end_selected(app); break;
        case ACTION_END_TREE:
            if (pid > 1 && confirm_end(app, TRUE) &&
                !lsm_process_control_tree(pid, instance_id,
                                          LSM_PROCESS_CONTROL_TERMINATE))
                show_process_backend_error(app, "Unable to end the process tree");
            break;
        case ACTION_SUSPEND:
            if (!lsm_process_control(pid, instance_id,
                                     LSM_PROCESS_CONTROL_SUSPEND))
                show_process_backend_error(app, "Unable to suspend the process");
            break;
        case ACTION_RESUME:
            if (!lsm_process_control(pid, instance_id,
                                     LSM_PROCESS_CONTROL_RESUME))
                show_process_backend_error(app, "Unable to resume the process");
            break;
        case ACTION_EFFICIENCY: {
            const gboolean enable = !process->efficiency_mode;
            if (!lsm_process_set_efficiency(pid, instance_id, enable))
                show_process_backend_error(app, enable ? "Unable to enable Efficiency mode"
                                                : "Unable to disable Efficiency mode");
            else
                lsm_processes_update(app);
            break;
        }
        case ACTION_AFFINITY: show_affinity_dialog(app); break;
        case ACTION_RECORD:
            if (app->details.process_record_menu_item)
                gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(app->details.process_record_menu_item),
                    app->process.record_file == NULL);
            else
                lsm_process_record_set(app, app->process.record_file == NULL);
            break;
        case ACTION_COPY_PID: {
            char pid_text[32];
            snprintf(pid_text, sizeof(pid_text), "%llu",
                     (unsigned long long)process->pid);
            copy_text(pid_text);
            break;
        }
        case ACTION_COPY_PATH: {
            LsmProcessInfo enriched = *process;
            if (lsm_process_enrich(
                    process->pid, &enriched, LSM_PROCESS_SCAN_EXECUTABLE))
                copy_text(enriched.executable);
            break;
        }
        case ACTION_COPY_COMMAND: copy_text(process->command); break;
        case ACTION_OPEN_DIRECTORY: open_executable_directory(app); break;
    }
}

static void priority_activate(GtkMenuItem *item, gpointer user_data)
{
    LsmApp *app = user_data;
    const LsmProcessPriority priority = (LsmProcessPriority)GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(item), "lsm-priority"));
    if (!lsm_process_set_priority(app->process.selected_pid,
                                  app->process.selected_instance_id,
                                  priority)) {
        char error[160];
        lsm_process_error_message(error, sizeof(error));
        lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                          "Unable to change process priority", "%s", error);
    }
}

static GtkWidget *action_menu_item(const char *label, ProcessAction action, LsmApp *app)
{
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    g_object_set_data(G_OBJECT(item), "lsm-action", GINT_TO_POINTER(action));
    g_signal_connect(item, "activate", G_CALLBACK(process_action_activate), app);
    return item;
}

static void destroy_menu(GtkWidget *menu, gpointer user_data)
{
    (void)user_data;
    gtk_widget_destroy(menu);
}

static void show_columns_from_menu(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    lsm_details_show_columns(user_data);
}

/* Context-menu construction shares the same action dispatcher as buttons. */
GtkWidget *lsm_process_actions_menu(LsmApp *app, gboolean include_columns)
{
    const LsmProcessInfo *process = action_snapshot_process(
        app, app->process.selected_pid, app->process.selected_instance_id);
    const gboolean grouped = app->process.selected_group_count > 1U;
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *details =
        action_menu_item("Process details", ACTION_DETAILS, app);
    GtkWidget *record =
        action_menu_item(app->process.record_file ? "Stop recording" : "Record process",
                         ACTION_RECORD, app);
    gtk_widget_set_sensitive(details, !grouped);
    gtk_widget_set_sensitive(record, !grouped || app->process.record_file != NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), details);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), record);
    if (include_columns) {
        GtkWidget *columns = gtk_menu_item_new_with_label("Choose columns…");
        g_signal_connect(columns, "activate",
                         G_CALLBACK(show_columns_from_menu), app);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), columns);
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    GtkWidget *end = action_menu_item(
        app->process.selected_group_count > 1U ? "End task" : "End process",
        ACTION_END, app);
    GtkWidget *end_tree = action_menu_item("End process tree", ACTION_END_TREE, app);
    GtkWidget *suspend = action_menu_item("Suspend", ACTION_SUSPEND, app);
    GtkWidget *resume = action_menu_item("Resume", ACTION_RESUME, app);
    gtk_widget_set_sensitive(end, app->process.selected_pid > 1);
    gtk_widget_set_sensitive(end_tree, app->process.selected_pid > 1 &&
                                      app->process.selected_group_count == 0U);
    gtk_widget_set_sensitive(suspend, !grouped && app->process.selected_pid > 1 &&
        (!process || strcmp(process->state, "Stopped") != 0));
    gtk_widget_set_sensitive(resume, !grouped && app->process.selected_pid > 1 && process &&
        strcmp(process->state, "Stopped") == 0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), end);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), end_tree);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), suspend);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), resume);
    GtkWidget *efficiency = gtk_check_menu_item_new_with_label("Efficiency mode");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(efficiency),
                                   process && process->efficiency_mode);
    g_object_set_data(G_OBJECT(efficiency), "lsm-action",
                      GINT_TO_POINTER(ACTION_EFFICIENCY));
    g_signal_connect(efficiency, "activate", G_CALLBACK(process_action_activate), app);
    gtk_widget_set_sensitive(efficiency, !grouped && app->process.selected_pid > 1);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), efficiency);

    GtkWidget *priority_root = gtk_menu_item_new_with_label("Set priority");
    GtkWidget *priority_menu = gtk_menu_new();
    struct { const char *label; LsmProcessPriority priority; } priorities[] = {
        {"High", LSM_PROCESS_PRIORITY_HIGH},
        {"Above normal", LSM_PROCESS_PRIORITY_ABOVE_NORMAL},
        {"Normal", LSM_PROCESS_PRIORITY_NORMAL},
        {"Below normal", LSM_PROCESS_PRIORITY_BELOW_NORMAL},
        {"Low", LSM_PROCESS_PRIORITY_LOW}
    };
    for (size_t i = 0; i < G_N_ELEMENTS(priorities); i++) {
        GtkWidget *item = gtk_menu_item_new_with_label(priorities[i].label);
        g_object_set_data(G_OBJECT(item), "lsm-priority",
                          GINT_TO_POINTER((int)priorities[i].priority));
        g_signal_connect(item, "activate", G_CALLBACK(priority_activate), app);
        gtk_menu_shell_append(GTK_MENU_SHELL(priority_menu), item);
    }
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(priority_root), priority_menu);
    gtk_widget_set_sensitive(priority_root, !grouped && app->process.selected_pid > 1);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), priority_root);
    GtkWidget *affinity = action_menu_item("Set CPU affinity…", ACTION_AFFINITY, app);
    gtk_widget_set_sensitive(affinity, !grouped && app->process.selected_pid > 1);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), affinity);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    GtkWidget *copy_root = gtk_menu_item_new_with_label("Copy");
    GtkWidget *copy_menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(copy_menu), action_menu_item("PID", ACTION_COPY_PID, app));
    gtk_menu_shell_append(GTK_MENU_SHELL(copy_menu), action_menu_item("Executable path", ACTION_COPY_PATH, app));
    gtk_menu_shell_append(GTK_MENU_SHELL(copy_menu), action_menu_item("Command line", ACTION_COPY_COMMAND, app));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(copy_root), copy_menu);
    gtk_widget_set_sensitive(copy_root, !grouped);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_root);
    GtkWidget *open_directory =
        action_menu_item("Open executable directory",
                         ACTION_OPEN_DIRECTORY, app);
    gtk_widget_set_sensitive(open_directory, !grouped);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), open_directory);
    g_signal_connect(menu, "selection-done", G_CALLBACK(destroy_menu), NULL);
    return menu;
}

void lsm_processes_end_selected(LsmApp *app)
{
    if (!app || app->process.selected_pid <= 1 ||
        app->process.selected_instance_id == 0U)
        return;

    const LsmProcessId selected_pid = app->process.selected_pid;
    const LsmProcessInstanceId selected_instance_id =
        app->process.selected_instance_id;
    const size_t group_count = app->process.selected_group_count;
    LsmProcessId *group_pids = NULL;
    LsmProcessInstanceId *group_instance_ids = NULL;
    if (group_count > 1U) {
        if (!app->process.selected_group_pids ||
            !app->process.selected_group_instance_ids ||
            group_count > SIZE_MAX / sizeof(*group_pids) ||
            group_count > SIZE_MAX / sizeof(*group_instance_ids))
            return;
        group_pids = malloc(group_count * sizeof(*group_pids));
        group_instance_ids = malloc(
            group_count * sizeof(*group_instance_ids));
        if (!group_pids || !group_instance_ids) {
            free(group_pids);
            free(group_instance_ids);
            return;
        }
        memcpy(group_pids, app->process.selected_group_pids,
               group_count * sizeof(*group_pids));
        memcpy(group_instance_ids, app->process.selected_group_instance_ids,
               group_count * sizeof(*group_instance_ids));
    }

    if (!confirm_end(app, FALSE)) {
        free(group_pids);
        free(group_instance_ids);
        return;
    }
    if (group_count > 1U) {
        size_t failures = 0U;
        char last_error[160] = "Unknown process backend error";
        for (size_t index = 0U; index < group_count; index++) {
            const LsmProcessId pid = group_pids[index];
            if (pid <= 1U) continue;
            if (!lsm_process_control(
                    pid, group_instance_ids[index],
                    LSM_PROCESS_CONTROL_TERMINATE) && errno != ESRCH) {
                failures++;
                lsm_process_error_message(last_error, sizeof(last_error));
            }
        }
        free(group_pids);
        free(group_instance_ids);
        if (failures > 0U)
            lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                "Unable to end every process in the task",
                "%zu process%s could not be ended: %s", failures,
                failures == 1U ? "" : "es", last_error);
        return;
    }
    if (!lsm_process_control(selected_pid, selected_instance_id,
                             LSM_PROCESS_CONTROL_TERMINATE))
        show_process_backend_error(app, "Unable to end the process");
}

void lsm_processes_show_selected_details(LsmApp *app)
{
    if (app && app->process.selected_pid > 0)
        lsm_process_inspector_show(app, app->process.selected_pid,
                                   app->process.selected_instance_id);
}
