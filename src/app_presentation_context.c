// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_presentation_context.c
 * @brief Private LsmApp accessors for presentation coordination policy.
 *
 * This file is the deliberate bridge between top-level presentation policy and
 * the complete private application layout. Keyboard and page-registry modules
 * consume narrow views instead of reaching through unrelated subsystem state.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_presentation_context.h"

#include "app_internal.h"

#include <string.h>

bool lsm_app_keyboard_control_view(const LsmApp *app,
                                   LsmKeyboardControlView *view)
{
    if (!app || !view) return false;

    view->window = app->shell.window;
    view->notebook = app->shell.notebook;
    view->processes_tree = app->processes.processes_tree;
    view->details_tree = app->details.details_tree;
    view->performance_stack = app->performance.performance_stack;
    view->pause_menu_item = app->shell.pause_menu_item;
    view->paused = app->runtime.paused;
    return true;
}

bool lsm_app_page_registry_control_view(const LsmApp *app,
                                        LsmPageRegistryControlView *view)
{
    if (!app || !view) return false;

    view->active_tab = app->runtime.active_tab >= 0 &&
                       app->runtime.active_tab < LSM_TAB_COUNT
        ? (LsmTabIndex)app->runtime.active_tab : LSM_TAB_COUNT;
    view->filesystem_update_interval_ms =
        app->runtime.filesystem_update_interval_ms;
    view->shutting_down = app->runtime.shutting_down;
    view->shell_shown = app->runtime.shell_shown;
    return true;
}

bool lsm_app_page_registry_page_built(const LsmApp *app, LsmTabIndex page)
{
    return app && page >= 0 && page < LSM_TAB_COUNT &&
           app->runtime.page_built[page];
}

GtkWidget *lsm_app_page_registry_search_target(const LsmApp *app,
                                               LsmTabIndex page)
{
    if (!app) return NULL;

    switch (page) {
        case LSM_TAB_PROCESSES:
            return app->processes.processes_search;
        case LSM_TAB_APP_HISTORY:
            return app->history.history_search;
        case LSM_TAB_STARTUP:
            return app->startup.startup_search;
        case LSM_TAB_DETAILS:
            return app->details.details_search;
        case LSM_TAB_SERVICES:
            return app->services.services_search;
        case LSM_TAB_FILESYSTEMS:
            return app->filesystem.filesystem_search;
        case LSM_TAB_PERFORMANCE:
        case LSM_TAB_USERS:
        case LSM_TAB_OVERVIEW:
        case LSM_TAB_COUNT:
            return NULL;
    }
    return NULL;
}

GtkWidget *lsm_app_page_registry_notebook(const LsmApp *app)
{
    return app ? app->shell.notebook : NULL;
}

GtkWidget *lsm_app_page_registry_window(const LsmApp *app)
{
    return app ? app->shell.window : NULL;
}

void lsm_app_page_registry_set_active(LsmApp *app, LsmTabIndex page)
{
    if (!app || page < 0 || page >= LSM_TAB_COUNT) return;
    app->runtime.active_tab = (gint)page;
    app->runtime.last_tab = (gint)page;
}

void lsm_app_page_registry_ensure_built(LsmApp *app, LsmTabIndex page)
{
    if (!app || page < 0 || page >= LSM_TAB_COUNT) return;
    lsm_app_ensure_page_built(app, page);
}

void lsm_app_page_registry_restore_scroll(LsmApp *app, LsmTabIndex page)
{
    if (!app || page < 0 || page >= LSM_TAB_COUNT ||
        !app->runtime.page_scrollers[page])
        return;

    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(app->runtime.page_scrollers[page]));
    if (adjustment)
        gtk_adjustment_set_value(adjustment, app->runtime.page_scroll[page]);
}

void lsm_app_page_registry_sync_overview_chrome(LsmApp *app)
{
    if (!app || !app->shell.window) return;

    const gboolean integrated =
        !app->runtime.compact_summary &&
        app->runtime.active_tab == LSM_TAB_OVERVIEW;
    GtkWidget *menu_bar = g_object_get_data(
        G_OBJECT(app->shell.window), "lsm-main-menu-bar");
    if (menu_bar)
        gtk_widget_set_visible(menu_bar, !integrated);
    if (app->shell.summary_bar)
        gtk_widget_set_visible(
            app->shell.summary_bar, app->runtime.compact_summary);
}
