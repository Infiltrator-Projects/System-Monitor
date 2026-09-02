// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app.c
 * @brief Application composition root and ordered lifecycle.
 *
 * Global shell policy and timer cadence live in dedicated modules; this file
 * owns construction order, subsystem lifetime and teardown order only.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app.h"
#include "app_internal.h"
#include "app_menu.h"
#include "app_runtime.h"
#include "app_shell.h"

#include "application_catalog.h"
#include "details_page.h"
#include "filesystems.h"
#include "history.h"
#include "monitor.h"
#include "performance.h"
#include "preferences.h"
#include "process_backend.h"
#include "process_scanner.h"
#include "process_inspector.h"
#include "processes_ui.h"
#include "services.h"
#include "startup.h"
#include "summary_bar.h"
#include "users.h"

#include <stdio.h>
#include <stdlib.h>

LsmApp *lsm_app_create(void)
{
    return calloc(1U, sizeof(LsmApp));
}

void lsm_app_free(LsmApp *app)
{
    free(app);
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
    app->process_scanner = lsm_process_scanner_create();
    if (!app->process_scanner) {
        fputs("Unable to start process scanner\n", stderr);
        lsm_monitor_destroy(&app->monitor);
        return;
    }
    app->process.application_catalog = lsm_application_catalog_create();
    if (!app->process.application_catalog) {
        fputs("Unable to allocate application catalogue\n", stderr);
        lsm_process_scanner_destroy(app->process_scanner);
        app->process_scanner = NULL;
        lsm_monitor_destroy(&app->monitor);
        return;
    }
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
    lsm_app_shell_apply_css();

    app->shell.window = gtk_application_window_new(application);
    gtk_window_set_title(GTK_WINDOW(app->shell.window), LSM_PROGRAM_NAME);
    gtk_window_set_default_size(GTK_WINDOW(app->shell.window),
                                app->runtime.window_width, app->runtime.window_height);
    gtk_window_set_position(GTK_WINDOW(app->shell.window), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(GTK_WINDOW(app->shell.window), LSM_EXECUTABLE_NAME);
    lsm_app_shell_connect_window(app);
    if (app->runtime.window_maximized) gtk_window_maximize(GTK_WINDOW(app->shell.window));

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(app->shell.window), main_box);
    gtk_box_pack_start(GTK_BOX(main_box), lsm_app_menu_build(app), FALSE, FALSE, 0);
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
    lsm_app_shell_connect_notebook(app);
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
    lsm_app_shell_apply_compact_summary(app);
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
    (void)lsm_app_refresh_processes_if_due(app, TRUE);
    lsm_performance_refresh(app);
    lsm_filesystems_refresh(app);
    lsm_app_runtime_start(app);
}

/* Shutdown is idempotent because GTK and GApplication can both request it. */
void lsm_app_shutdown(LsmApp *app)
{
    if (!app || app->runtime.shutting_down) return;
    app->runtime.shutting_down = TRUE;
    if (app->shell.window) {
        lsm_app_shell_save_page_scroll(app, app->runtime.active_tab);
        lsm_details_save_layout(app);
        lsm_preferences_save(app);
    }
    lsm_app_runtime_stop(app);
    if (app->startup.startup_search_timer) g_source_remove(app->startup.startup_search_timer);
    if (app->services.services_search_timer) g_source_remove(app->services.services_search_timer);
    lsm_app_shell_cancel_pending(app);
    lsm_process_record_stop(app);
    lsm_services_destroy(app);
    lsm_users_destroy(app);
    lsm_startup_destroy(app);
    lsm_processes_destroy(app);
    lsm_details_destroy(app);
    lsm_process_scanner_destroy(app->process_scanner);
    app->process_scanner = NULL;
    lsm_process_list_free(app->process.process_snapshot);
    app->process.process_snapshot = NULL;
    app->process.process_snapshot_count = 0;
    lsm_application_catalog_destroy(app->process.application_catalog);
    app->process.application_catalog = NULL;
    lsm_process_group_selection_clear(app);
    lsm_history_destroy(app);
    lsm_filesystems_destroy(app);
    lsm_performance_destroy(app);
    if (app->process.filters) g_ptr_array_free(app->process.filters, TRUE);
    lsm_monitor_destroy(&app->monitor);
}
