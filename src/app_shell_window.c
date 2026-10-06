// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_shell_window.c
 * @brief Page-neutral close, resize and window-state handling.
 *
 * Top-level page activation and keyboard policy belong to app_page_registry.c.
 * This module owns only window-manager mechanics so shell connection does not
 * drag page-specific dependencies back into the live event path.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_shell_window.h"

#include "app_internal.h"
#include "performance.h"

static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event,
                                gpointer user_data)
{
    (void)event;
    LsmApp *app = user_data;
    if (!app) return FALSE;

    gtk_widget_set_visible(widget, FALSE);
    GdkDisplay *display = gtk_widget_get_display(widget);
    if (display) gdk_display_flush(display);
    g_application_quit(G_APPLICATION(app->application));
    return TRUE;
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
    app->runtime.window_restore_reflow_source =
        g_idle_add(reflow_after_window_restore, app);
}

static void apply_navigation_density(LsmApp *app)
{
    if (!app) return;
    const gboolean compact = app->runtime.compact_layout;
    if (app->shell.main_navigation)
        gtk_widget_set_size_request(
            app->shell.main_navigation,
            compact ? LSM_MAIN_NAV_COMPACT_WIDTH : LSM_MAIN_NAV_WIDTH, -1);

    for (gint tab = 0; tab < LSM_TAB_COUNT; tab++) {
        GtkWidget *button = app->shell.navigation_tab_buttons[tab];
        if (!button) continue;
        gtk_widget_set_size_request(
            button,
            compact ? LSM_MAIN_NAV_BUTTON_COMPACT_WIDTH
                    : LSM_MAIN_NAV_BUTTON_WIDTH,
            48);
        GtkWidget *label = g_object_get_data(
            G_OBJECT(button), "lsm-nav-label-widget");
        if (label) gtk_widget_set_visible(label, !compact);
    }
    for (gint type = 0; type < LSM_PAGE_COUNT; type++) {
        GtkWidget *button = app->shell.navigation_resource_buttons[type];
        if (!button) continue;
        gtk_widget_set_size_request(
            button,
            compact ? LSM_MAIN_NAV_BUTTON_COMPACT_WIDTH
                    : LSM_MAIN_NAV_BUTTON_WIDTH,
            48);
        GtkWidget *label = g_object_get_data(
            G_OBJECT(button), "lsm-nav-label-widget");
        if (label) gtk_widget_set_visible(label, !compact);
    }
}

static gboolean on_window_configure(GtkWidget *widget,
                                    GdkEventConfigure *event,
                                    gpointer user_data)
{
    LsmApp *app = user_data;
    const gboolean maximized = gtk_window_is_maximized(GTK_WINDOW(widget));
    if (!app->runtime.compact_summary && !maximized &&
        event->width > 0 && event->height > 0) {
        app->runtime.window_width = event->width;
        app->runtime.window_height = event->height;
        const gint compact_limit = app->runtime.compact_layout
            ? LSM_COMPACT_LAYOUT_THRESHOLD + LSM_COMPACT_LAYOUT_HYSTERESIS
            : LSM_COMPACT_LAYOUT_THRESHOLD;
        const gboolean compact = event->width < compact_limit;
        if (compact != app->runtime.compact_layout) {
            app->runtime.compact_layout = compact;
            apply_navigation_density(app);
            lsm_performance_reflow(app);
        }
    }
    return FALSE;
}

static gboolean on_window_state(GtkWidget *widget,
                                GdkEventWindowState *event,
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

void lsm_app_shell_window_connect(LsmApp *app)
{
    if (!app || !app->shell.window) return;
    g_signal_connect(app->shell.window, "delete-event",
                     G_CALLBACK(on_delete_event), app);
    g_signal_connect(app->shell.window, "configure-event",
                     G_CALLBACK(on_window_configure), app);
    g_signal_connect(app->shell.window, "window-state-event",
                     G_CALLBACK(on_window_state), app);
}
