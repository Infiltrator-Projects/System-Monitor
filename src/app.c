// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app.c
 * @brief Main GTK window, menus, timers and application lifecycle.
 *
 * LsmApp is owned exclusively by the GTK main thread. This module establishes
 * that ownership, composes the feature tabs and turns user-selected refresh
 * rates into GLib timeout sources. Feature modules retain their own models and
 * collectors; app.c coordinates lifecycle but does not duplicate their data.
 * Shutdown is ordered so timeout callbacks stop before backend and widget-owned
 * state is released.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app.h"
#include "application_catalog.h"
#include "history.h"
#include "filesystems.h"
#include "help.h"
#include "monitor.h"
#include "performance.h"
#include "process_backend.h"
#include "process_export.h"
#include "process_inspector.h"
#include "project_info.h"
#include "preferences.h"
#include "details_page.h"
#include "processes_ui.h"
#include "services.h"
#include "startup.h"
#include "summary_bar.h"
#include "system_snapshot.h"
#include "task_launcher.h"
#include "ui_helpers.h"
#include "users.h"

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

static void on_save_snapshot(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    LsmApp *app = user_data;
    GtkWidget *chooser = gtk_file_chooser_dialog_new(
        "Save system snapshot", GTK_WINDOW(app->shell.window),
        GTK_FILE_CHOOSER_ACTION_SAVE, "Cancel", GTK_RESPONSE_CANCEL,
        "Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser),
                                      "linux-system-monitor-snapshot.txt");
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

static void on_refresh(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    LsmApp *app = user_data;
    const gboolean was_paused = app->runtime.paused;
    app->runtime.paused = FALSE;
    lsm_processes_update(app);
    lsm_performance_refresh(app);
    lsm_history_refresh(app);
    lsm_filesystems_refresh(app);
    lsm_startup_refresh(app);
    lsm_services_refresh(app);
    lsm_users_refresh(app);
    app->runtime.paused = was_paused;
}

static guint process_refresh_interval(const LsmApp *app)
{
    return app->runtime.update_interval_ms < 1000U ? 1000U : app->runtime.update_interval_ms;
}

void lsm_app_preferences_changed(LsmApp *app)
{
    if (!app || !app->shell.window ||
        (!app->runtime.performance_timer && !app->runtime.process_timer)) return;
    if (app->runtime.performance_timer) g_source_remove(app->runtime.performance_timer);
    if (app->runtime.process_timer) g_source_remove(app->runtime.process_timer);
    app->runtime.performance_timer = g_timeout_add(app->runtime.update_interval_ms,
                                            lsm_performance_update, app);
    app->runtime.process_timer = g_timeout_add(process_refresh_interval(app),
                                       lsm_processes_update, app);
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

static void apply_compact_summary(LsmApp *app)
{
    if (!app || !app->shell.window) return;
    if (app->shell.notebook)
        gtk_widget_set_visible(app->shell.notebook, !app->runtime.compact_summary);
    if (app->shell.pause_indicator)
        gtk_widget_set_visible(app->shell.pause_indicator,
                               app->runtime.paused && !app->runtime.compact_summary);
    if (app->runtime.compact_summary) {
        if (app->runtime.window_maximized) app->runtime.compact_restore_maximized = TRUE;
        gtk_window_unmaximize(GTK_WINDOW(app->shell.window));
        gtk_window_resize(GTK_WINDOW(app->shell.window), 760, 150);
    } else {
        gtk_window_resize(GTK_WINDOW(app->shell.window), app->runtime.window_width,
                          app->runtime.window_height);
        if (app->runtime.compact_restore_maximized || app->runtime.window_maximized)
            gtk_window_maximize(GTK_WINDOW(app->shell.window));
    }
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
    apply_compact_summary(app);
    lsm_preferences_save(app);
}

static void on_direction_selected(GtkCheckMenuItem *item, gpointer user_data)
{
    if (!gtk_check_menu_item_get_active(item)) return;
    LsmApp *app = g_object_get_data(G_OBJECT(item), "lsm-app");
    app->runtime.newer_on_right = GPOINTER_TO_INT(user_data) != 0;
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
    char *path = g_build_filename(g_get_home_dir(), LSM_LOG_DIRECTORY, NULL);
    g_mkdir_with_parents(path, 0755);
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

/* Log plotting creates an independent window and retains no monitor ownership. */
static void on_plot_log(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    LsmApp *app = user_data;
    GtkWidget *chooser = gtk_file_chooser_dialog_new("Plot process log",
        GTK_WINDOW(app->shell.window), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL, "Open", GTK_RESPONSE_ACCEPT, NULL);
    char *log_dir = g_build_filename(g_get_home_dir(), LSM_LOG_DIRECTORY, NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), log_dir);
    g_free(log_dir);
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Linux System Monitor CSV logs");
    gtk_file_filter_add_pattern(filter, "*.csv");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);

    if (gtk_dialog_run(GTK_DIALOG(chooser)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(chooser);
        return;
    }
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    gtk_widget_destroy(chooser);
    if (!filename) return;

    FILE *file = fopen(filename, "r");
    if (!file) {
        g_free(filename);
        return;
    }
    LsmGraph *graph = lsm_graph_new(TRUE, TRUE, 100.0, -1, 390);
    char line[2048];
    bool header = true;
    while (fgets(line, sizeof(line), file)) {
        if (header) { header = false; continue; }
        double cpu = 0.0, memory = 0.0;
        /* Only CPU and memory are plotted; suppress the remaining CSV fields. */
        if (sscanf(line, "%*95[^,],%*d,%lf,%lf", &cpu, &memory) == 2)
            lsm_graph_push(graph, cpu, memory, TRUE);
    }
    fclose(file);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), LSM_PROGRAM_NAME " Process Log");
    gtk_window_set_default_size(GTK_WINDOW(window), 820, 500);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(app->shell.window));
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 12);
    GtkWidget *label = gtk_label_new(filename);
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
    g_free(filename);
}

static void on_about(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    LsmApp *app = user_data;
    const InfiltratrProjectInfo *info = lsm_project_info();
    const char *authors[] = {
        "Shannon Smith — author and project maintainer",
        NULL
    };
    gtk_show_about_dialog(GTK_WINDOW(app->shell.window),
        "program-name", info->program_name,
        "version", info->version,
        "comments", info->comments,
        "copyright", info->copyright_text,
        "license-type", GTK_LICENSE_CUSTOM,
        "license", "GNU General Public License version 3 or later "
                   "(GPL-3.0-or-later)",
        "wrap-license", TRUE,
        "authors", authors,
        "website", info->website,
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
static GtkWidget *build_menu(LsmApp *app)
{
    GtkWidget *bar = gtk_menu_bar_new();

    GtkWidget *file_root = gtk_menu_item_new_with_mnemonic("_File");
    GtkWidget *file_menu = gtk_menu_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu),
        menu_item("_Run new task…", G_CALLBACK(on_run_new_task), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu),
                          gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu),
        menu_item("_Save system snapshot…", G_CALLBACK(on_save_snapshot), app));
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
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), menu_item("_Refresh now", G_CALLBACK(on_refresh), app));
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
                          menu_item("_Linux System Monitor Help", G_CALLBACK(on_help), app));
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), gtk_separator_menu_item_new());
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), menu_item("_About " LSM_PROGRAM_NAME, G_CALLBACK(on_about), app));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_root), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(bar), help_root);

    return bar;
}

static void apply_css(void)
{
    static const char css[] =
        "#lsm-side-button:checked {"
        " background-color: alpha(@theme_selected_bg_color, 0.28);"
        " border-color: @theme_selected_bg_color;"
        "}";
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

/* Window and tab lifecycle. Expensive pages refresh on demand as well as by
 * their bounded background cadence. */
static void save_page_scroll(LsmApp *app, gint page);

static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    (void)widget; (void)event;
    LsmApp *app = user_data;
    save_page_scroll(app, app->runtime.active_tab);
    lsm_details_save_layout(app);
    lsm_preferences_save(app);
    g_application_quit(G_APPLICATION(app->application));
    return TRUE;
}

static void save_page_scroll(LsmApp *app, gint page)
{
    if (!app || page < 0 || page >= LSM_TAB_COUNT ||
        !app->runtime.page_scrollers[page]) return;
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(app->runtime.page_scrollers[page]));
    if (adjustment)
        app->runtime.page_scroll[page] = gtk_adjustment_get_value(adjustment);
}

static void restore_page_scroll(LsmApp *app, guint page)
{
    if (!app || page >= LSM_TAB_COUNT || !app->runtime.page_scrollers[page]) return;
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(app->runtime.page_scrollers[page]));
    if (adjustment)
        gtk_adjustment_set_value(adjustment, app->runtime.page_scroll[page]);
}

static void on_tab_switched(GtkNotebook *notebook, GtkWidget *page,
                            guint page_number, gpointer user_data)
{
    (void)notebook;
    (void)page;
    LsmApp *app = user_data;
    save_page_scroll(app, app->runtime.active_tab);
    if (page_number < LSM_TAB_COUNT) {
        app->runtime.active_tab = (gint)page_number;
        app->runtime.last_tab = (gint)page_number;
    }
    switch ((LsmTabIndex)page_number) {
        case LSM_TAB_APP_HISTORY:
            lsm_history_refresh(app);
            break;
        case LSM_TAB_FILESYSTEMS:
            lsm_filesystems_refresh(app);
            break;
        case LSM_TAB_STARTUP:
            lsm_startup_refresh(app);
            break;
        case LSM_TAB_SERVICES:
            lsm_services_refresh(app);
            break;
        case LSM_TAB_USERS:
            lsm_users_refresh(app);
            break;
        case LSM_TAB_PROCESSES:
            lsm_processes_present_snapshot(app);
            break;
        case LSM_TAB_DETAILS:
            lsm_details_present_snapshot(app);
            break;
        case LSM_TAB_PERFORMANCE:
        case LSM_TAB_COUNT:
            break;
    }
    restore_page_scroll(app, page_number);
}

static gboolean reflow_after_window_restore(gpointer user_data)
{
    LsmApp *app = user_data;
    app->runtime.window_restore_reflow_source = 0U;
    if (app->runtime.shutting_down) return G_SOURCE_REMOVE;

    if (!app->runtime.compact_summary && app->shell.window &&
        !gtk_window_is_maximized(GTK_WINDOW(app->shell.window))) {
        gint width = 0;
        gint height = 0;
        gtk_window_get_size(GTK_WINDOW(app->shell.window), &width, &height);
        if (width > 0 && height > 0) {
            app->runtime.window_width = width;
            app->runtime.window_height = height;
        }
    }
    lsm_performance_reflow(app);
    return G_SOURCE_REMOVE;
}

static void schedule_window_restore_reflow(LsmApp *app)
{
    if (!app || app->runtime.window_restore_reflow_source) return;
    /* Defer until the window manager's restore configure events have settled. */
    app->runtime.window_restore_reflow_source =
        g_idle_add(reflow_after_window_restore, app);
}

static gboolean on_window_configure(GtkWidget *widget, GdkEventConfigure *event,
                                    gpointer user_data)
{
    LsmApp *app = user_data;
    const gboolean maximized = gtk_window_is_maximized(GTK_WINDOW(widget));
    if (!app->runtime.compact_summary && !maximized &&
        event->width > 0 && event->height > 0) {
        app->runtime.window_width = event->width;
        app->runtime.window_height = event->height;
    }
    return FALSE;
}

static gboolean on_window_state(GtkWidget *widget, GdkEventWindowState *event,
                                gpointer user_data)
{
    (void)widget;
    LsmApp *app = user_data;
    if (app->runtime.compact_summary) return FALSE;

    const gboolean was_maximized = app->runtime.window_maximized;
    app->runtime.window_maximized =
        (event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0;
    if (was_maximized && !app->runtime.window_maximized)
        schedule_window_restore_reflow(app);
    return FALSE;
}

static GtkWidget *search_for_current_tab(const LsmApp *app)
{
    switch ((LsmTabIndex)gtk_notebook_get_current_page(
                GTK_NOTEBOOK(app->shell.notebook))) {
        case LSM_TAB_PROCESSES: return app->processes.processes_search;
        case LSM_TAB_DETAILS: return app->details.details_search;
        case LSM_TAB_APP_HISTORY: return app->history.history_search;
        case LSM_TAB_FILESYSTEMS: return app->filesystem.filesystem_search;
        case LSM_TAB_STARTUP: return app->startup.startup_search;
        case LSM_TAB_SERVICES: return app->services.services_search;
        case LSM_TAB_PERFORMANCE:
        case LSM_TAB_USERS:
        case LSM_TAB_COUNT:
            return NULL;
    }
    return NULL;
}

static gboolean focus_allows_pause(const LsmApp *app, GtkWidget *focus)
{
    return !focus || focus == app->shell.notebook ||
           focus == app->processes.processes_tree || focus == app->details.details_tree ||
           focus == app->performance.performance_stack;
}

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event,
                             gpointer user_data)
{
    (void)widget;
    LsmApp *app = user_data;
    const gboolean control = (event->state & GDK_CONTROL_MASK) != 0;
    const gboolean shift = (event->state & GDK_SHIFT_MASK) != 0;
    const gboolean alt = (event->state & GDK_MOD1_MASK) != 0;
    if (event->keyval == GDK_KEY_F5) {
        on_refresh(NULL, app);
        return TRUE;
    }
    if (control && (event->keyval == GDK_KEY_f ||
                    event->keyval == GDK_KEY_F)) {
        GtkWidget *search = search_for_current_tab(app);
        if (search) {
            gtk_widget_grab_focus(search);
            return TRUE;
        }
    }
    if (control && shift && (event->keyval == GDK_KEY_s ||
                             event->keyval == GDK_KEY_S)) {
        on_save_snapshot(NULL, app);
        return TRUE;
    }
    if (control && (event->keyval == GDK_KEY_c ||
                    event->keyval == GDK_KEY_C)) {
        const gint current = gtk_notebook_get_current_page(
            GTK_NOTEBOOK(app->shell.notebook));
        GtkWidget *copy_focus = gtk_window_get_focus(GTK_WINDOW(app->shell.window));
        if ((current == LSM_TAB_PROCESSES &&
             copy_focus == app->processes.processes_tree) ||
            (current == LSM_TAB_DETAILS &&
             copy_focus == app->details.details_tree)) {
            lsm_process_export_copy_selected(app);
            return TRUE;
        }
    }
    if (alt && event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_8) {
        const gint page_index = (gint)(event->keyval - GDK_KEY_1);
        gtk_notebook_set_current_page(GTK_NOTEBOOK(app->shell.notebook), page_index);
        return TRUE;
    }

    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(app->shell.window));
    if (event->keyval == GDK_KEY_space && focus_allows_pause(app, focus)) {
        gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(app->shell.pause_menu_item), !app->runtime.paused);
        return TRUE;
    }
    const gint current =
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->shell.notebook));
    if ((current == LSM_TAB_PROCESSES && focus == app->processes.processes_tree) ||
        (current == LSM_TAB_DETAILS && focus == app->details.details_tree)) {
        if (event->keyval == GDK_KEY_Return ||
            event->keyval == GDK_KEY_KP_Enter) {
            if (current == LSM_TAB_PROCESSES)
                lsm_processes_go_to_details(app);
            else
                lsm_processes_show_selected_details(app);
            return TRUE;
        }
        if (event->keyval == GDK_KEY_Delete) {
            lsm_processes_end_selected(app);
            return TRUE;
        }
    }
    return FALSE;
}

/* Application construction establishes ownership before starting timers. */
void lsm_app_activate(GtkApplication *application, gpointer user_data)
{
    LsmApp *app = user_data;
    if (app->shell.window) {
        gtk_window_present(GTK_WINDOW(app->shell.window));
        return;
    }
    app->application = application;
    app->runtime.update_interval_ms = LSM_DEFAULT_UPDATE_INTERVAL_MS;
    app->runtime.newer_on_right = TRUE;
    app->runtime.network_use_bits = FALSE;
    app->runtime.process_cpu_per_core = FALSE;
    app->runtime.show_all_filesystems = FALSE;
    app->runtime.always_on_top = FALSE;
    app->runtime.compact_summary = FALSE;
    app->runtime.compact_restore_maximized = FALSE;
    app->runtime.window_width = LSM_DEFAULT_WINDOW_WIDTH;
    app->runtime.window_height = LSM_DEFAULT_WINDOW_HEIGHT;
    app->runtime.last_tab = LSM_TAB_PERFORMANCE;
    app->runtime.active_tab = LSM_TAB_PERFORMANCE;
    g_strlcpy(app->runtime.selected_performance_page, "cpu",
              sizeof(app->runtime.selected_performance_page));
    if (!lsm_monitor_init(&app->monitor)) {
        fputs("Unable to initialise the monitoring backend\n", stderr);
        return;
    }
    app->process_backend = lsm_process_backend_create();
    if (!app->process_backend) {
        fputs("Unable to allocate process backend\n", stderr);
        lsm_monitor_destroy(&app->monitor);
        return;
    }
    app->process.application_catalog = lsm_application_catalog_create();
#ifdef LSM_TEST_14_CORES
    app->monitor.cpu.logical_cores = 14;
    app->monitor.cpu.physical_cores = 12;
#endif

    snprintf(app->paths.config_dir, sizeof(app->paths.config_dir), "%s/%s", g_get_user_config_dir(), LSM_CONFIG_DIRECTORY);
    char *filter_path = g_build_filename(app->paths.config_dir, "filters.conf", NULL);
    g_strlcpy(app->paths.filter_path, filter_path, sizeof(app->paths.filter_path));
    g_free(filter_path);
    char *column_path = g_build_filename(app->paths.config_dir, "process-columns.conf", NULL);
    g_strlcpy(app->paths.column_path, column_path, sizeof(app->paths.column_path));
    g_free(column_path);
    char *preferences_path = g_build_filename(app->paths.config_dir, "preferences.conf", NULL);
    g_strlcpy(app->paths.preferences_path, preferences_path, sizeof(app->paths.preferences_path));
    g_free(preferences_path);
    lsm_preferences_load(app);
    lsm_process_filters_load(app);
    apply_css();

    app->shell.window = gtk_application_window_new(application);
    gtk_window_set_title(GTK_WINDOW(app->shell.window), LSM_PROGRAM_NAME);
    gtk_window_set_default_size(GTK_WINDOW(app->shell.window),
                                app->runtime.window_width, app->runtime.window_height);
    gtk_window_set_position(GTK_WINDOW(app->shell.window), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(GTK_WINDOW(app->shell.window), LSM_EXECUTABLE_NAME);
    g_signal_connect(app->shell.window, "delete-event", G_CALLBACK(on_delete_event), app);
    g_signal_connect(app->shell.window, "configure-event",
                     G_CALLBACK(on_window_configure), app);
    g_signal_connect(app->shell.window, "window-state-event",
                     G_CALLBACK(on_window_state), app);
    g_signal_connect(app->shell.window, "key-press-event",
                     G_CALLBACK(on_key_press), app);
    if (app->runtime.window_maximized) gtk_window_maximize(GTK_WINDOW(app->shell.window));

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->shell.window), main_box);
    gtk_box_pack_start(GTK_BOX(main_box), build_menu(app), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), lsm_summary_bar_build(app),
                       FALSE, FALSE, 0);
    app->shell.pause_indicator = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(app->shell.pause_indicator),
        "<b>Updates paused</b> — press F5 to refresh once or Space to resume.");
    gtk_widget_set_halign(app->shell.pause_indicator, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(app->shell.pause_indicator, 5);
    gtk_widget_set_margin_bottom(app->shell.pause_indicator, 5);
    gtk_widget_set_visible(app->shell.pause_indicator, FALSE);
    gtk_box_pack_start(GTK_BOX(main_box), app->shell.pause_indicator,
                       FALSE, FALSE, 0);

    app->shell.notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(app->shell.notebook), GTK_POS_TOP);
    g_signal_connect(app->shell.notebook, "switch-page", G_CALLBACK(on_tab_switched), app);
    gtk_box_pack_start(GTK_BOX(main_box), app->shell.notebook, TRUE, TRUE, 0);

    GtkWidget *performance_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    lsm_performance_build(app, performance_page);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->shell.notebook), performance_page,
                             gtk_label_new("Performance"));

    GtkWidget *process_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    lsm_processes_build(app, process_page);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->shell.notebook), process_page,
                             gtk_label_new("Processes"));

    GtkWidget *history_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    lsm_history_build(app, history_page);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->shell.notebook), history_page, gtk_label_new("App History"));

    GtkWidget *startup_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    lsm_startup_build(app, startup_page);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->shell.notebook), startup_page, gtk_label_new("Startup Apps"));

    GtkWidget *users_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    lsm_users_build(app, users_page);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->shell.notebook), users_page, gtk_label_new("Users"));

    GtkWidget *details_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    lsm_details_build(app, details_page);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->shell.notebook), details_page,
                             gtk_label_new("Details"));

    GtkWidget *services_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    lsm_services_build(app, services_page);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->shell.notebook), services_page,
                             gtk_label_new("Services"));

    GtkWidget *filesystem_page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    lsm_filesystems_build(app, filesystem_page);
    gtk_notebook_append_page(GTK_NOTEBOOK(app->shell.notebook), filesystem_page,
                             gtk_label_new("File Systems"));
    app->runtime.active_tab = LSM_TAB_PERFORMANCE;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->shell.notebook), app->runtime.last_tab);
    gtk_widget_show_all(app->shell.window);
    gtk_window_set_keep_above(GTK_WINDOW(app->shell.window), app->runtime.always_on_top);
    apply_compact_summary(app);
#ifdef LSM_TEST_PERFORMANCE
    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->shell.notebook),
                                  LSM_TAB_PERFORMANCE);
#endif
#ifdef LSM_TEST_PAGE_INDEX
    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->shell.notebook), LSM_TEST_PAGE_INDEX);
#endif
#ifdef LSM_TEST_LOGICAL
    gtk_stack_set_visible_child_name(GTK_STACK(app->performance.cpu_graph_stack), "logical");
#endif
    lsm_processes_update(app);
    lsm_performance_refresh(app);
    lsm_filesystems_refresh(app);
    app->runtime.performance_timer = g_timeout_add(app->runtime.update_interval_ms, lsm_performance_update, app);
    app->runtime.process_timer = g_timeout_add(process_refresh_interval(app), lsm_processes_update, app);
    app->runtime.services_timer = g_timeout_add_seconds(
        LSM_SERVICE_UPDATE_INTERVAL_SECONDS, lsm_services_update, app);
    app->runtime.users_timer = g_timeout_add_seconds(
        LSM_USER_UPDATE_INTERVAL_SECONDS, lsm_users_update, app);
    app->runtime.filesystem_timer = g_timeout_add_seconds(
        LSM_FILESYSTEM_UPDATE_INTERVAL_SECONDS, lsm_filesystems_update, app);
}

/* Shutdown is idempotent because GTK and GApplication can both request it. */
void lsm_app_shutdown(LsmApp *app)
{
    if (!app || app->runtime.shutting_down) return;
    app->runtime.shutting_down = TRUE;
    if (app->shell.window) {
        save_page_scroll(app, app->runtime.active_tab);
        lsm_details_save_layout(app);
        lsm_preferences_save(app);
    }
    if (app->runtime.performance_timer) g_source_remove(app->runtime.performance_timer);
    if (app->runtime.process_timer) g_source_remove(app->runtime.process_timer);
    if (app->runtime.services_timer) g_source_remove(app->runtime.services_timer);
    if (app->startup.startup_search_timer) g_source_remove(app->startup.startup_search_timer);
    if (app->services.services_search_timer) g_source_remove(app->services.services_search_timer);
    if (app->runtime.users_timer) g_source_remove(app->runtime.users_timer);
    if (app->runtime.filesystem_timer) g_source_remove(app->runtime.filesystem_timer);
    if (app->runtime.window_restore_reflow_source) {
        g_source_remove(app->runtime.window_restore_reflow_source);
        app->runtime.window_restore_reflow_source = 0U;
    }
    lsm_process_record_stop(app);
    lsm_services_destroy(app);
    lsm_users_destroy(app);
    lsm_startup_destroy(app);
    lsm_processes_destroy(app);
    lsm_details_destroy(app);
    lsm_process_list_free(app->process.process_snapshot);
    app->process.process_snapshot = NULL;
    app->process.process_snapshot_count = 0;
    lsm_process_backend_destroy(app->process_backend);
    app->process_backend = NULL;
    lsm_application_catalog_destroy(app->process.application_catalog);
    app->process.application_catalog = NULL;
    lsm_process_group_selection_clear(app);
    lsm_history_destroy(app);
    lsm_filesystems_destroy(app);
    lsm_performance_destroy(app);
    if (app->process.filters) g_ptr_array_free(app->process.filters, TRUE);
    lsm_monitor_destroy(&app->monitor);
}
