// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_menu.c
 * @brief Global menu construction and user-invoked application actions.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_menu.h"
#include "app_internal.h"
#include "app_runtime.h"
#include "app_shell.h"
#include "common.h"

#include "details_page.h"
#include "help.h"
#include "process_export.h"
#include "process_file_users.h"
#include "project_info.h"
#include "preferences.h"
#include "system_snapshot.h"
#include "task_launcher.h"
#include "ui_helpers.h"

#include <infiltratr/design.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Menu callbacks contain presentation policy only; feature modules own data. */
static void on_quit(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    LsmApp *app = user_data;
    g_application_quit(G_APPLICATION(app->application));
}

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

static void on_speed_selected(GtkCheckMenuItem *item, gpointer user_data)
{
    if (!gtk_check_menu_item_get_active(item)) return;
    LsmApp *app = g_object_get_data(G_OBJECT(item), "lsm-app");
    app->runtime.update_interval_ms = GPOINTER_TO_UINT(user_data);
    lsm_preferences_save(app);
    lsm_app_preferences_changed(app);
}

static void on_pause_toggled(GtkCheckMenuItem *item, gpointer user_data)
{
    LsmApp *app = user_data;
    app->runtime.paused = gtk_check_menu_item_get_active(item);
    if (app->shell.pause_indicator)
        gtk_widget_set_visible(app->shell.pause_indicator,
                               app->runtime.paused && !app->runtime.compact_summary);
}

static void on_always_on_top_toggled(GtkCheckMenuItem *item,
                                     gpointer user_data)
{
    LsmApp *app = user_data;
    app->runtime.always_on_top = gtk_check_menu_item_get_active(item);
    if (app->shell.window)
        gtk_window_set_keep_above(GTK_WINDOW(app->shell.window),
                                  app->runtime.always_on_top);
    lsm_preferences_save(app);
}

static void on_compact_summary_toggled(GtkCheckMenuItem *item,
                                       gpointer user_data)
{
    LsmApp *app = user_data;
    app->runtime.compact_summary = gtk_check_menu_item_get_active(item);
    lsm_app_shell_apply_compact_summary(app);
    lsm_preferences_save(app);
}

static void on_direction_selected(GtkCheckMenuItem *item, gpointer user_data)
{
    if (!gtk_check_menu_item_get_active(item)) return;
    LsmApp *app = g_object_get_data(G_OBJECT(item), "lsm-app");
    app->runtime.newer_on_right = GPOINTER_TO_INT(user_data) != 0;
    lsm_preferences_save(app);
}

static void on_theme_selected(GtkCheckMenuItem *item, gpointer user_data)
{
    if (!gtk_check_menu_item_get_active(item)) return;
    LsmApp *app = g_object_get_data(G_OBJECT(item), "lsm-app");
    if (!app) return;
    const int mode = GPOINTER_TO_INT(user_data);
    if (mode < INFILTRATR_THEME_SYSTEM || mode > INFILTRATR_THEME_NIGHT)
        return;
    app->runtime.theme_mode = (InfiltratrThemeMode)mode;
    lsm_app_shell_apply_theme(app);
    lsm_preferences_save(app);
}

static void on_preferences(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    lsm_preferences_show(user_data);
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
        if (!gtk_show_uri_on_window(GTK_WINDOW(app->shell.window), uri, GDK_CURRENT_TIME, &error) && error) {
            GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->shell.window), GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "Unable to open the log directory");
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", error->message);
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
        /* The graph contract uses only CPU and memory from the recorder CSV. */
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
    gtk_window_set_default_size(GTK_WINDOW(window), 820, 500);
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
    GtkWidget *chooser = gtk_file_chooser_dialog_new("Plot process log",
        GTK_WINDOW(app->shell.window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);
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
    gtk_show_about_dialog(GTK_WINDOW(app->shell.window),
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

/* Menu construction keeps all global application actions in one place. */
GtkWidget *lsm_app_menu_build(LsmApp *app)
{
    GtkWidget *bar = gtk_menu_bar_new();

    GtkWidget *file_root = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *file_menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu),
        menu_item("_Run new task…", G_CALLBACK(on_run_new_task), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu),
                          gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu),
        menu_item("_Save system snapshot…", G_CALLBACK(lsm_app_menu_save_snapshot), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu),
        menu_item("_Export selected process rows…",
                  G_CALLBACK(on_export_selected), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu),
                          gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu),
        menu_item("_Quit", G_CALLBACK(on_quit), app));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_root), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), file_root);

    GtkWidget *options_root = gtk_menu_item_new_with_mnemonic("_Options");
    GtkWidget *options_menu = gtk_menu_new();
    app->shell.pause_menu_item =
        gtk_check_menu_item_new_with_mnemonic("_Pause updates");
    g_signal_connect(app->shell.pause_menu_item, "toggled",
                     G_CALLBACK(on_pause_toggled), app);
    gtk_menu_shell_append(GTK_MENU_SHELL(options_menu),
                          app->shell.pause_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(options_menu),
                          gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(options_menu),
        menu_item("_Preferences…", G_CALLBACK(on_preferences), app));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(options_root), options_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), options_root);

    GtkWidget *view_root = gtk_menu_item_new_with_mnemonic("_View");
    GtkWidget *view_menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), menu_item("_Refresh now", G_CALLBACK(lsm_app_menu_refresh), app));
    app->shell.always_on_top_menu_item =
        gtk_check_menu_item_new_with_label("Always on top");
    g_signal_connect(app->shell.always_on_top_menu_item, "toggled",
                     G_CALLBACK(on_always_on_top_toggled), app);
    gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM(app->shell.always_on_top_menu_item), app->runtime.always_on_top);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu),
                          app->shell.always_on_top_menu_item);
    app->shell.compact_summary_menu_item =
        gtk_check_menu_item_new_with_label("Compact summary mode");
    g_signal_connect(app->shell.compact_summary_menu_item, "toggled",
                     G_CALLBACK(on_compact_summary_toggled), app);
    gtk_check_menu_item_set_active(
        GTK_CHECK_MENU_ITEM(app->shell.compact_summary_menu_item),
        app->runtime.compact_summary);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu),
                          app->shell.compact_summary_menu_item);

    GtkWidget *theme_root = gtk_menu_item_new_with_label("Theme");
    GtkWidget *theme_menu = gtk_menu_new();
    GSList *theme_group = NULL;
    for (int mode = INFILTRATR_THEME_SYSTEM;
         mode <= INFILTRATR_THEME_NIGHT; mode++) {
        const char *label = mode == INFILTRATR_THEME_SYSTEM
            ? "Follow system"
            : infiltratr_theme_mode_name((InfiltratrThemeMode)mode);
        GtkWidget *radio = gtk_radio_menu_item_new_with_label(
            theme_group, label);
        theme_group = gtk_radio_menu_item_get_group(
            GTK_RADIO_MENU_ITEM(radio));
        g_object_set_data(G_OBJECT(radio), "lsm-app", app);
        g_signal_connect(radio, "toggled",
                         G_CALLBACK(on_theme_selected), GINT_TO_POINTER(mode));
        gtk_menu_shell_append(GTK_MENU_SHELL(theme_menu), radio);
        if (mode == (int)app->runtime.theme_mode)
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(radio), TRUE);
    }
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(theme_root), theme_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), theme_root);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu),
                          gtk_separator_menu_item_new());

    GtkWidget *speed_root =
        gtk_menu_item_new_with_label("Performance refresh speed");
    GtkWidget *speed_menu = gtk_menu_new();
    GSList *speed_group = NULL;
    struct { const char *name; guint milliseconds; } speeds[] = {
        {"Fast (0.5 seconds)", 500}, {"Normal (1 second)", 1000},
        {"Low (2 seconds)", 2000}, {"Very low (5 seconds)", 5000}
    };
    for (size_t i = 0; i < G_N_ELEMENTS(speeds); i++) {
        GtkWidget *radio = gtk_radio_menu_item_new_with_label(speed_group, speeds[i].name);
        speed_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(radio));
        g_object_set_data(G_OBJECT(radio), "lsm-app", app);
        g_signal_connect(radio, "toggled", G_CALLBACK(on_speed_selected), GUINT_TO_POINTER(speeds[i].milliseconds));
        gtk_menu_shell_append(GTK_MENU_SHELL(speed_menu), radio);
        if (speeds[i].milliseconds == app->runtime.update_interval_ms)
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(radio), TRUE);
    }
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(speed_root), speed_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), speed_root);

    GtkWidget *direction_root = gtk_menu_item_new_with_label("Graph direction");
    GtkWidget *direction_menu = gtk_menu_new();
    GtkWidget *right = gtk_radio_menu_item_new_with_label(NULL, "New values on the right");
    GtkWidget *left = gtk_radio_menu_item_new_with_label_from_widget(GTK_RADIO_MENU_ITEM(right), "New values on the left");
    g_object_set_data(G_OBJECT(right), "lsm-app", app);
    g_object_set_data(G_OBJECT(left), "lsm-app", app);
    g_signal_connect(right, "toggled", G_CALLBACK(on_direction_selected), GINT_TO_POINTER(1));
    g_signal_connect(left, "toggled", G_CALLBACK(on_direction_selected), GINT_TO_POINTER(0));
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
        app->runtime.newer_on_right ? right : left), TRUE);
    gtk_menu_shell_append(GTK_MENU_SHELL(direction_menu), right);
    gtk_menu_shell_append(GTK_MENU_SHELL(direction_menu), left);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(direction_root), direction_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), direction_root);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu),
        menu_item("Process _columns…", G_CALLBACK(on_process_columns), app));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_root), view_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), view_root);

    GtkWidget *tools_root = gtk_menu_item_new_with_mnemonic("_Tools");
    GtkWidget *tools_menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu),
                          menu_item("Process _filters…", G_CALLBACK(on_filters), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu),
                          menu_item("_Find process using file…", G_CALLBACK(on_find_file_users), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu),
                          menu_item("_Copy selected process rows",
                                    G_CALLBACK(on_copy_selected), app));
    app->details.process_record_menu_item =
        gtk_check_menu_item_new_with_label("Record selected process");
    gtk_widget_set_sensitive(app->details.process_record_menu_item, FALSE);
    g_signal_connect(app->details.process_record_menu_item, "toggled",
                     G_CALLBACK(on_record_process), app);
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu),
                          app->details.process_record_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu),
                          gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu),
                          menu_item("_Plot process log…", G_CALLBACK(on_plot_log), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu),
                          menu_item("Open process log _folder", G_CALLBACK(on_open_logs), app));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools_root), tools_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), tools_root);

    GtkWidget *help_root = gtk_menu_item_new_with_mnemonic("_Help");
    GtkWidget *help_menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu),
                          menu_item("_System Monitor Help", G_CALLBACK(on_help), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), menu_item("_About " LSM_PROGRAM_NAME, G_CALLBACK(on_about), app));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_root), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), help_root);

    return bar;
}
