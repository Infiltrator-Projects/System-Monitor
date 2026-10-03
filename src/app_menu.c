// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_menu.c
 * @brief Specialist tools/help overflow menu and user-invoked actions.
 *
 * Primary presentation settings live in the graphical Preferences dialog and
 * ordinary window lifecycle lives in the InfiltratorOS-style header. This
 * menu therefore contains only specialist tools and help actions exposed from
 * the header overflow control rather than a persistent desktop-style menubar.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_menu.h"
#include "app_internal.h"
#include "app_runtime.h"
#include "common.h"

#include "details_page.h"
#include "help.h"
#include "process_export.h"
#include "process_file_users.h"
#include "project_info.h"
#include "system_snapshot.h"
#include "task_launcher.h"
#include "ui_helpers.h"

#include <infiltratr/design.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Menu callbacks contain presentation policy only; feature modules own data. */
static void on_run_new_task(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    lsm_task_launcher_show(user_data);
}

void lsm_app_menu_save_snapshot(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    LsmApp *app = user_data;
    GtkWidget *chooser = gtk_file_chooser_dialog_new(
        "Save system snapshot", GTK_WINDOW(app->shell.window),
        GTK_FILE_CHOOSER_ACTION_SAVE, "Cancel", GTK_RESPONSE_CANCEL,
        "Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser),
                                      "system-monitor-snapshot.txt");
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(chooser),
                                                    TRUE);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Plain-text diagnostic snapshot");
    gtk_file_filter_add_pattern(filter, "*.txt");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        char error[256];
        if (path && !lsm_system_snapshot_write(app, path, error,
                                               sizeof(error)))
            lsm_ui_show_error(GTK_WINDOW(app->shell.window), "Snapshot failed",
                              "%s", error);
        g_free(path);
    }
    gtk_widget_destroy(chooser);
}

static void on_copy_selected(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    lsm_process_export_copy_selected(user_data);
}

static void on_export_selected(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    lsm_process_export_selected_dialog(user_data);
}

void lsm_app_menu_refresh(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    lsm_app_refresh_all(user_data);
}

static void on_help(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    lsm_help_show(user_data);
}

static void on_find_file_users(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    lsm_process_file_users_show(user_data);
}

static void on_filters(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    lsm_process_filters_dialog(user_data);
}

static void on_process_columns(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    lsm_details_show_columns(user_data);
}

static void on_record_process(GtkCheckMenuItem *item, gpointer user_data)
{
    LsmApp *app = user_data;
    lsm_process_record_set(app, gtk_check_menu_item_get_active(item));
}

static void on_open_logs(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    LsmApp *app = user_data;
    char path[LSM_PATH_LEN];
    char home[LSM_PATH_LEN];
    if (!lsm_home_directory(home, sizeof(home)) ||
        !lsm_join_path(path, sizeof(path), home, LSM_LOG_DIRECTORY)) {
        lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                          "Unable to open the log directory",
                          "The log directory path is too long.");
        return;
    }
    (void)lsm_mkdir_parents(path, 0700U);
    char *uri = g_filename_to_uri(path, NULL, NULL);
    if (uri) {
        GError *error = NULL;
        if (!gtk_show_uri_on_window(GTK_WINDOW(app->shell.window), uri,
                                    GDK_CURRENT_TIME, &error) && error) {
            GtkWidget *dialog = gtk_message_dialog_new(
                GTK_WINDOW(app->shell.window), GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                "Unable to open the log directory");
            gtk_message_dialog_format_secondary_text(
                GTK_MESSAGE_DIALOG(dialog), "%s", error->message);
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            g_error_free(error);
        }
        g_free(uri);
    }
    g_free(path);
}

static void graph_window_destroy(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    lsm_graph_free(user_data);
}

/** Immutable file request passed to the process-log parser worker. */
typedef struct {
    char *filename; /**< Owned path selected by the user. */
} ProcessLogPlotRequest;

/** Bounded process-log samples returned to the GTK main context. */
typedef struct {
    char *filename; /**< Owned source path used as the graph caption. */
    char *error_message; /**< Owned parse/open error, or NULL on success. */
    double cpu[LSM_HISTORY_LENGTH]; /**< Circular CPU sample storage. */
    double memory[LSM_HISTORY_LENGTH]; /**< Circular memory sample storage. */
    size_t count; /**< Number of retained samples, bounded by history length. */
    size_t next; /**< Circular index that will receive the next sample. */
} ProcessLogPlotResult;

static void process_log_plot_request_free(gpointer data)
{
    ProcessLogPlotRequest *request = data;
    if (!request) return;
    g_free(request->filename);
    g_free(request);
}

static void process_log_plot_result_free(gpointer data)
{
    ProcessLogPlotResult *result = data;
    if (!result) return;
    g_free(result->filename);
    g_free(result->error_message);
    g_free(result);
}

static void process_log_plot_worker(GTask *task, gpointer source_object,
                                    gpointer task_data,
                                    GCancellable *cancellable)
{
    (void)source_object;
    (void)cancellable;
    const ProcessLogPlotRequest *request = task_data;
    ProcessLogPlotResult *result = g_new0(ProcessLogPlotResult, 1U);
    result->filename = g_strdup(request->filename);

    FILE *file = fopen(request->filename, "r");
    if (!file) {
        result->error_message = g_strdup(g_strerror(errno ? errno : EIO));
        g_task_return_pointer(task, result, process_log_plot_result_free);
        return;
    }

    char line[2048];
    bool header = true;
    while (fgets(line, sizeof(line), file)) {
        if (header) {
            header = false;
            continue;
        }
        double cpu = 0.0;
        double memory = 0.0;
        if (sscanf(line, "%*95[^,],%*d,%lf,%lf", &cpu, &memory) != 2)
            continue;
        result->cpu[result->next] = cpu;
        result->memory[result->next] = memory;
        result->next = (result->next + 1U) % LSM_HISTORY_LENGTH;
        if (result->count < LSM_HISTORY_LENGTH) result->count++;
    }
    if (ferror(file))
        result->error_message = g_strdup(g_strerror(errno ? errno : EIO));
    (void)fclose(file);
    g_task_return_pointer(task, result, process_log_plot_result_free);
}

static void process_log_plot_complete(GObject *source_object,
                                      GAsyncResult *async_result,
                                      gpointer user_data)
{
    (void)user_data;
    ProcessLogPlotResult *result =
        g_task_propagate_pointer(G_TASK(async_result), NULL);
    LsmApp *app = source_object
        ? g_object_get_data(source_object, "lsm-app") : NULL;
    if (!result || !app || app->runtime.shutting_down) {
        process_log_plot_result_free(result);
        return;
    }
    if (result->error_message) {
        lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                          "Unable to plot process log", "%s",
                          result->error_message);
        process_log_plot_result_free(result);
        return;
    }

    LsmGraph *graph = lsm_graph_new(TRUE, TRUE, 100.0, -1, 390);
    const size_t first = result->count == LSM_HISTORY_LENGTH
        ? result->next : 0U;
    for (size_t offset = 0U; offset < result->count; offset++) {
        const size_t index = (first + offset) % LSM_HISTORY_LENGTH;
        lsm_graph_push(graph, result->cpu[index], result->memory[index], TRUE);
    }

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), LSM_PROGRAM_NAME " Process Log");
    lsm_ui_set_workarea_default_size(GTK_WINDOW(window), 820, 500);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(app->shell.window));
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    GtkWidget *label = gtk_label_new(result->filename);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    GtkWidget *legend = gtk_label_new(NULL);
    char legend_markup[128];
    snprintf(legend_markup, sizeof(legend_markup),
             "<b>CPU %%</b> and <b>Memory %%</b> — most recent %d samples",
             LSM_HISTORY_LENGTH);
    gtk_label_set_markup(GTK_LABEL(legend), legend_markup);
    gtk_widget_set_halign(legend, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), legend, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), graph->area, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    g_signal_connect(window, "destroy", G_CALLBACK(graph_window_destroy), graph);
    gtk_widget_show_all(window);
    process_log_plot_result_free(result);
}

/* Log parsing is worker-owned; GTK only constructs the completed graph. */
static void on_plot_log(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    LsmApp *app = user_data;
    GtkWidget *chooser = gtk_file_chooser_dialog_new(
        "Plot process log", GTK_WINDOW(app->shell.window),
        GTK_FILE_CHOOSER_ACTION_OPEN, "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT, NULL);
    char log_dir[LSM_PATH_LEN];
    char home[LSM_PATH_LEN];
    if (lsm_home_directory(home, sizeof(home)) &&
        lsm_join_path(log_dir, sizeof(log_dir), home, LSM_LOG_DIRECTORY))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), log_dir);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "System Monitor CSV logs");
    gtk_file_filter_add_pattern(filter, "*.csv");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(chooser);
        return;
    }
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    gtk_widget_destroy(chooser);
    if (!filename) return;

    ProcessLogPlotRequest *request = g_new0(ProcessLogPlotRequest, 1U);
    request->filename = filename;
    GTask *task = g_task_new(G_OBJECT(app->shell.window), NULL,
                             process_log_plot_complete, NULL);
    g_task_set_task_data(task, request, process_log_plot_request_free);
    g_task_run_in_thread(task, process_log_plot_worker);
    g_object_unref(task);
}

static void on_about(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    LsmApp *app = user_data;
    const InfiltratrProjectInfo *info = lsm_project_info();
    const char *profile = info->build_profile;
    if (lsm_string_equal(profile, "aggressive") ||
        lsm_string_equal(profile, "portable"))
        profile = "native";
    char comments[512];
    (void)snprintf(comments, sizeof(comments), "%s\n\nBuild: %s",
                   info->comments, infiltratr_build_profile_label(profile));
    const char *authors[] = {
        "Shannon Smith — Author and project maintainer",
        NULL
    };
    gtk_show_about_dialog(
        GTK_WINDOW(app->shell.window),
        "program-name", info->program_name,
        "version", info->version,
        "comments", comments,
        "authors", authors,
        "website", info->website,
        "website-label", "Website",
        "copyright", info->copyright_text,
        "license-type", GTK_LICENSE_CUSTOM,
        "license",
        "System Monitor is free software licensed under the GNU General "
        "Public License version 3 or, at your option, any later version "
        "(GPL-3.0-or-later).\n\n"
        "See LICENSE in the source package for the complete licence text.",
        "wrap-license", TRUE,
        "logo-icon-name", info->icon_name,
        NULL);
}

static GtkWidget *menu_item(const char *label, GCallback callback, gpointer data)
{
    GtkWidget *item = gtk_menu_item_new_with_mnemonic(label);
    if (callback) g_signal_connect(item, "activate", callback, data);
    return item;
}

GtkWidget *lsm_app_menu_build(LsmApp *app)
{
    GtkWidget *menu = gtk_menu_new();

    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("_Run new task…", G_CALLBACK(on_run_new_task), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("_Refresh now", G_CALLBACK(lsm_app_menu_refresh), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("_Save system snapshot…",
                  G_CALLBACK(lsm_app_menu_save_snapshot), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("Process _filters…", G_CALLBACK(on_filters), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("Process _columns…", G_CALLBACK(on_process_columns), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("_Find process using file…",
                  G_CALLBACK(on_find_file_users), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("_Copy selected process rows",
                  G_CALLBACK(on_copy_selected), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("_Export selected process rows…",
                  G_CALLBACK(on_export_selected), app));
    app->details.process_record_menu_item =
        gtk_check_menu_item_new_with_label("Record selected process");
    gtk_widget_set_sensitive(app->details.process_record_menu_item, FALSE);
    g_signal_connect(app->details.process_record_menu_item, "toggled",
                     G_CALLBACK(on_record_process), app);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                          app->details.process_record_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                          gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("_Plot process log…", G_CALLBACK(on_plot_log), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("Open process log _folder", G_CALLBACK(on_open_logs), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                          gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("_System Monitor Help", G_CALLBACK(on_help), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
        menu_item("_About " LSM_PROGRAM_NAME, G_CALLBACK(on_about), app));

    return menu;
}

static void overflow_menu_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    GtkWidget *menu = user_data;
    if (!menu) return;
    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}

static void overflow_button_destroy(GtkWidget *widget, gpointer user_data)
{
    (void)widget;
    if (user_data) gtk_widget_destroy(GTK_WIDGET(user_data));
}

void lsm_app_menu_attach_to_header(LsmApp *app)
{
    if (!app || !app->shell.window) return;
    GtkWidget *header = g_object_get_data(
        G_OBJECT(app->shell.window), "lsm-shell-header");
    if (!header) return;

    GtkWidget *button = gtk_button_new_from_icon_name(
        "open-menu-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(button), "lsm-window-control");
    gtk_widget_set_tooltip_text(button, "Tools and Help");
    GtkWidget *menu = lsm_app_menu_build(app);
    g_signal_connect(button, "clicked", G_CALLBACK(overflow_menu_clicked), menu);
    g_signal_connect(button, "destroy", G_CALLBACK(overflow_button_destroy), menu);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), button);
    g_object_set_data(G_OBJECT(app->shell.window), "lsm-overflow-button", button);
}
