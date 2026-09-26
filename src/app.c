// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app.c
 * @brief Application composition root and ordered lifecycle.
 *
 * Global shell policy and timer cadence live in dedicated modules; this file
 * owns construction order, subsystem lifetime and teardown order only.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app.h"
#include "app_internal.h"
#include "app_menu.h"
#include "app_runtime.h"
#include "app_shell.h"

#include "application_catalog.h"
#include "common.h"
#include "details_page.h"
#include "filesystems.h"
#include "history.h"
#include "monitor.h"
#include "overview.h"
#include "performance.h"
#include "presentation_contract.h"
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

static void constrain_initial_window_geometry(LsmApp *app)
{
    if (!app) return;

    GdkScreen *screen = gdk_screen_get_default();
    if (!screen) return;

    gint monitor = gdk_screen_get_primary_monitor(screen);
    if (monitor < 0) monitor = 0;

    GdkRectangle workarea = {0, 0, 0, 0};
    gdk_screen_get_monitor_workarea(screen, monitor, &workarea);
    if (workarea.width <= 0 || workarea.height <= 0) return;

    /*
     * gtk_window_set_default_size() specifies the client allocation, while the
     * window manager still needs room for server-side decorations. Treat a
     * persisted size as a preference and reserve a small decoration margin so
     * a previously large window cannot start underneath the panel/title bar.
     */
    const gint width_margin = 32;
    const gint height_margin = 80;
    const gint maximum_width =
        workarea.width > width_margin ? workarea.width - width_margin
                                      : workarea.width;
    const gint maximum_height =
        workarea.height > height_margin ? workarea.height - height_margin
                                        : workarea.height;

    if (app->runtime.window_width > maximum_width)
        app->runtime.window_width = maximum_width;
    if (app->runtime.window_height > maximum_height)
        app->runtime.window_height = maximum_height;
}

static bool app_paths_initialise(LsmApp *app)
{
    if (!app) return false;
    char config_root[LSM_PATH_LEN];
    if (!lsm_xdg_config_home(config_root, sizeof(config_root)) ||
        !lsm_join_path(app->paths.config_dir, sizeof(app->paths.config_dir),
                       config_root, LSM_CONFIG_DIRECTORY))
        return false;

    return lsm_join_path(app->paths.filter_path, sizeof(app->paths.filter_path),
                         app->paths.config_dir, "filters.conf") &&
        lsm_join_path(app->paths.column_path, sizeof(app->paths.column_path),
                      app->paths.config_dir, "process-columns.conf") &&
        lsm_join_path(app->paths.preferences_path,
                      sizeof(app->paths.preferences_path),
                      app->paths.config_dir, "preferences.conf");
}

static void application_catalog_result_free(gpointer data)
{
    lsm_application_catalog_destroy(data);
}

static void application_catalog_worker(GTask *task, gpointer source_object,
                                       gpointer task_data,
                                       GCancellable *cancellable)
{
    (void)source_object;
    (void)task_data;
    (void)cancellable;
    LsmApplicationCatalog *catalog = lsm_application_catalog_create();
    g_task_return_pointer(task, catalog, application_catalog_result_free);
}

static void application_catalog_complete(GObject *source_object,
                                         GAsyncResult *async_result,
                                         gpointer user_data)
{
    (void)user_data;
    LsmApplicationCatalog *catalog =
        g_task_propagate_pointer(G_TASK(async_result), NULL);
    LsmApp *app = source_object
        ? g_object_get_data(source_object, "lsm-app") : NULL;
    if (!app || app->runtime.shutting_down) {
        lsm_application_catalog_destroy(catalog);
        return;
    }

    app->runtime.application_catalog_loading = FALSE;
    if (!catalog) return;
    lsm_application_catalog_destroy(app->process.application_catalog);
    app->process.application_catalog = catalog;
    app->processes.processes_structure_valid = FALSE;
    app->processes.processes_model_dirty = TRUE;
    if (app->runtime.active_tab == LSM_TAB_PROCESSES &&
        app->runtime.page_built[LSM_TAB_PROCESSES])
        lsm_processes_present_snapshot(app);
}

static void start_application_catalog_load(LsmApp *app)
{
    if (!app || !app->shell.window || app->runtime.shutting_down ||
        app->runtime.application_catalog_loading ||
        app->process.application_catalog)
        return;

    app->runtime.application_catalog_loading = TRUE;
    GTask *task = g_task_new(G_OBJECT(app->shell.window), NULL,
                             application_catalog_complete, NULL);
    g_task_run_in_thread(task, application_catalog_worker);
    g_object_unref(task);
}

void lsm_app_ensure_page_built(LsmApp *app, LsmTabIndex page)
{
    if (!app || page < 0 || page >= LSM_TAB_COUNT ||
        app->runtime.page_built[page] ||
        !app->runtime.page_containers[page])
        return;

    GtkWidget *container = app->runtime.page_containers[page];
    switch (page) {
        case LSM_TAB_PERFORMANCE:
            lsm_performance_build(app, container);
            break;
        case LSM_TAB_PROCESSES:
            lsm_processes_build(app, container);
            break;
        case LSM_TAB_APP_HISTORY:
            lsm_history_build(app, container);
            break;
        case LSM_TAB_STARTUP:
            lsm_startup_build(app, container);
            break;
        case LSM_TAB_USERS:
            lsm_users_build(app, container);
            break;
        case LSM_TAB_DETAILS:
            lsm_details_build(app, container);
            break;
        case LSM_TAB_SERVICES:
            lsm_services_build(app, container);
            break;
        case LSM_TAB_FILESYSTEMS:
            lsm_filesystems_build(app, container);
            break;
        case LSM_TAB_OVERVIEW:
            lsm_overview_build(app, container);
            break;
        case LSM_TAB_COUNT:
            return;
    }

    app->runtime.page_built[page] = TRUE;
    lsm_app_runtime_page_built(app, page);
    if (app->runtime.shell_shown)
        gtk_widget_show_all(container);
}

static gboolean restore_initial_tab_after_first_paint(gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app) return G_SOURCE_REMOVE;
    app->runtime.initial_tab_restore_source = 0U;
    if (app->runtime.shutting_down || !app->shell.notebook)
        return G_SOURCE_REMOVE;

    const gint target = app->runtime.initial_tab_after_paint;
    app->runtime.initial_tab_after_paint = LSM_TAB_PERFORMANCE;
    if (target >= 0 && target < LSM_TAB_COUNT &&
        target != LSM_TAB_PERFORMANCE)
        gtk_notebook_set_current_page(GTK_NOTEBOOK(app->shell.notebook), target);
    return G_SOURCE_REMOVE;
}

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
    app->runtime.theme_mode = INFILTRATR_THEME_SYSTEM;
    app->runtime.newer_on_right = TRUE;
    app->runtime.network_use_bits = FALSE;
    app->runtime.process_cpu_per_core = FALSE;
    app->runtime.show_all_filesystems = FALSE;
    app->runtime.always_on_top = FALSE;
    app->runtime.compact_summary = FALSE;
    app->runtime.compact_restore_maximized = FALSE;
    app->runtime.window_width = LSM_DEFAULT_WINDOW_WIDTH;
    app->runtime.window_height = LSM_DEFAULT_WINDOW_HEIGHT;
    /* Keep Performance as the fast first-paint surface, but make the graphical
     * Overview the default destination for a fresh profile. Persisted choices
     * still win when preferences are loaded. */
    app->runtime.last_tab = LSM_TAB_OVERVIEW;
    app->runtime.active_tab = LSM_TAB_PERFORMANCE;
    lsm_copy_string(app->runtime.selected_performance_page,
                    sizeof(app->runtime.selected_performance_page), "cpu");
    if (!app_paths_initialise(app)) {
        fputs("Unable to construct the configuration paths\n", stderr);
        return;
    }
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
#ifdef LSM_TEST_14_CORES
    app->monitor.cpu.logical_cores = 14;
    app->monitor.cpu.physical_cores = 12;
#endif

    lsm_preferences_load(app);
    constrain_initial_window_geometry(app);
    app->runtime.initial_tab_after_paint = app->runtime.last_tab;
    lsm_process_filters_load(app);
    lsm_app_shell_apply_theme(app);

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
    GtkWidget *menu_bar = lsm_app_menu_build(app);
    g_object_set_data(G_OBJECT(app->shell.window), "lsm-main-menu-bar", menu_bar);
    gtk_box_pack_start(GTK_BOX(main_box), menu_bar, FALSE, FALSE, 0);
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

    GtkWidget *workspace = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(workspace, TRUE);
    gtk_widget_set_vexpand(workspace, TRUE);
    gtk_box_pack_start(GTK_BOX(main_box), workspace, TRUE, TRUE, 0);

    app->shell.main_navigation = lsm_app_shell_build_navigation(app);
    gtk_box_pack_start(GTK_BOX(workspace), app->shell.main_navigation,
                       FALSE, FALSE, 0);

    app->shell.notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(app->shell.notebook), GTK_POS_TOP);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(app->shell.notebook), FALSE);
    lsm_app_shell_connect_notebook(app);
    gtk_box_pack_start(GTK_BOX(workspace), app->shell.notebook, TRUE, TRUE, 0);

    for (gint page = 0; page < LSM_TAB_COUNT; page++) {
        GtkWidget *container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        app->runtime.page_containers[page] = container;
        gtk_notebook_append_page(GTK_NOTEBOOK(app->shell.notebook), container,
                                 gtk_label_new(lsm_tab_label(page)));
    }

    /* The first frame needs only the shell and Performance page. Everything
     * else is created incrementally after the event loop has had a chance to
     * paint the window. */
    lsm_app_ensure_page_built(app, LSM_TAB_PERFORMANCE);
    app->runtime.active_tab = LSM_TAB_PERFORMANCE;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->shell.notebook),
                                  LSM_TAB_PERFORMANCE);
    gtk_widget_show_all(app->shell.window);
    app->runtime.shell_shown = TRUE;
    lsm_app_shell_sync_navigation(app);
    g_object_set_data(G_OBJECT(app->shell.window), "lsm-app", app);
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
#ifdef LSM_TEST_PERFORMANCE_RESOURCE
    lsm_performance_show_resource(
        app, (LsmPageType)LSM_TEST_PERFORMANCE_RESOURCE, 0U);
#endif
    /* Slow application metadata and non-visible notebook pages must never
     * hold the first paint hostage. The process scanner and native monitor
     * already work asynchronously; keep that rule at the composition layer. */
    start_application_catalog_load(app);
    if (app->runtime.initial_tab_after_paint != LSM_TAB_PERFORMANCE)
        app->runtime.initial_tab_restore_source =
            g_idle_add(restore_initial_tab_after_first_paint, app);
    lsm_history_start(app);
    lsm_performance_refresh(app);
    lsm_app_runtime_start(app);
}

/* Shutdown is idempotent because GTK and GApplication can both request it. */
void lsm_app_shutdown(LsmApp *app)
{
    if (!app || app->runtime.shutting_down) return;
    app->runtime.shutting_down = TRUE;
    if (app->runtime.initial_tab_restore_source) {
        g_source_remove(app->runtime.initial_tab_restore_source);
        app->runtime.initial_tab_restore_source = 0U;
    }
    if (app->shell.window) {
        g_object_set_data(G_OBJECT(app->shell.window), "lsm-app", NULL);
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
    lsm_overview_destroy(app);
    lsm_performance_destroy(app);
    if (app->process.filters) g_ptr_array_free(app->process.filters, TRUE);
    lsm_monitor_destroy(&app->monitor);
}
