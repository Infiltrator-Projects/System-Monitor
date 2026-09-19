// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_shell.c
 * @brief Global window state, navigation, keyboard policy and shell styling.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_shell.h"
#include "app_internal.h"
#include "app_menu.h"
#include "app_runtime.h"

#include "details_page.h"
#include "filesystems.h"
#include "history.h"
#include "performance.h"
#include "preferences.h"
#include "process_export.h"
#include "processes_ui.h"
#include "services.h"
#include "startup.h"
#include "users.h"

#include <infiltratr/design.h>

#include <string.h>

void lsm_app_shell_apply_compact_summary(LsmApp *app)
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

static gboolean lsm_system_prefers_dark(void)
{
    GtkSettings *settings = gtk_settings_get_default();
    if (!settings) return FALSE;

    gboolean prefer_dark = FALSE;
    gchar *theme_name = NULL;
    g_object_get(settings,
                 "gtk-application-prefer-dark-theme", &prefer_dark,
                 "gtk-theme-name", &theme_name,
                 NULL);

    gboolean dark = prefer_dark;
    if (theme_name) {
        gchar *lower = g_ascii_strdown(theme_name, -1);
        dark = dark || (lower && strstr(lower, "dark") != NULL);
        g_free(lower);
        g_free(theme_name);
    }
    return dark;
}

static void on_system_theme_changed(GtkSettings *settings,
                                    GParamSpec *pspec,
                                    gpointer user_data)
{
    (void)settings;
    (void)pspec;
    LsmApp *app = user_data;
    if (app && app->runtime.theme_mode == INFILTRATR_THEME_SYSTEM)
        lsm_app_shell_apply_theme(app);
}

void lsm_app_shell_apply_theme(LsmApp *app)
{
    if (!app) return;
    GdkScreen *screen = gdk_screen_get_default();
    if (!screen) return;

    if (!app->shell.theme_provider) {
        app->shell.theme_provider = gtk_css_provider_new();
        gtk_style_context_add_provider_for_screen(
            screen, GTK_STYLE_PROVIDER(app->shell.theme_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 50U);

        GtkSettings *settings = gtk_settings_get_default();
        if (settings) {
            g_signal_connect(settings, "notify::gtk-theme-name",
                             G_CALLBACK(on_system_theme_changed), app);
            g_signal_connect(settings,
                             "notify::gtk-application-prefer-dark-theme",
                             G_CALLBACK(on_system_theme_changed), app);
        }
    }

    const gboolean system_dark = lsm_system_prefers_dark();
    const InfiltratrThemePalette *palette =
        infiltratr_theme_resolve(app->runtime.theme_mode, system_dark);
    const InfiltratrTypography *typography = infiltratr_typography();
    if (!palette || !typography || !typography->ui_family ||
        !typography->brand_family)
        return;

    GString *css = g_string_new(NULL);
    if (!css) return;

    g_string_append_printf(
        css,
        "* { font-family: \"%s\"; font-weight: %u; }"
        "headerbar .title, .titlebar .title {"
        " font-family: \"%s\", \"%s\"; font-weight: %u;"
        "}"
        "button, treeview header button, notebook tab { font-weight: %u; }"
        "@define-color lsm_background #%06X;"
        "@define-color lsm_panel #%06X;"
        "@define-color lsm_card #%06X;"
        "@define-color lsm_surface #%06X;"
        "@define-color lsm_input #%06X;"
        "@define-color lsm_border #%06X;"
        "@define-color lsm_text #%06X;"
        "@define-color lsm_title #%06X;"
        "@define-color lsm_muted #%06X;"
        "@define-color lsm_subtle #%06X;"
        "@define-color lsm_button_background #%06X;"
        "@define-color lsm_button_foreground #%06X;"
        "@define-color lsm_neutral #%06X;"
        "@define-color lsm_selection #%06X;"
        "@define-color lsm_selection_text #%06X;"
        "@define-color lsm_card_hover #%06X;"
        "@define-color lsm_surface_hover #%06X;"
        "@define-color lsm_operation #%06X;"
        "@define-color lsm_operation_hover #%06X;"
        "@define-color lsm_titlebar #%06X;"
        "@define-color lsm_connection #%06X;"
        "@define-color lsm_connection_border #%06X;"
        "@define-color lsm_heading #%06X;"
        "@define-color lsm_summary #%06X;"
        "@define-color lsm_kicker #%06X;"
        "@define-color lsm_detail_label #%06X;"
        "@define-color lsm_selected_summary #%06X;"
        "@define-color lsm_status_border #%06X;"
        "@define-color lsm_accent_foreground #%06X;"
        "@define-color lsm_accent_hover #%06X;"
        "window, dialog, .background {"
        " background-color: @lsm_background; color: @lsm_text;"
        "}"
        "headerbar, .titlebar {"
        " background-image: none; background-color: @lsm_titlebar;"
        " color: @lsm_title; border-bottom: 1px solid @lsm_border;"
        "}"
        "#lsm-summary-bar {"
        " background-image: none; background-color: @lsm_connection;"
        " color: @lsm_text; border: 1px solid @lsm_connection_border;"
        "}"
        "frame {"
        " background-image: none; background-color: @lsm_card;"
        " color: @lsm_text; border-color: @lsm_border;"
        "}"
        "menubar {"
        " background-color: @lsm_panel; color: @lsm_text;"
        " border-bottom: 1px solid @lsm_border;"
        "}"
        "menubar > menuitem { color: @lsm_summary; }"
        "menubar > menuitem:hover { color: @lsm_title; background-color: @lsm_card_hover; }"
        "menu {"
        " background-color: @lsm_panel; color: @lsm_text;"
        " border: 1px solid @lsm_border;"
        "}"
        "menuitem:hover { background-color: @lsm_surface_hover; }"
        "button, combobox button {"
        " background-image: none; background-color: @lsm_button_background;"
        " color: @lsm_button_foreground; border: 1px solid @lsm_border;"
        " box-shadow: none;"
        "}"
        "button label, button image, combobox button label, combobox button image {"
        " color: @lsm_button_foreground;"
        "}"
        "button:hover, combobox button:hover {"
        " background-color: @lsm_neutral; border-color: @lsm_neutral;"
        "}"
        "button:hover label, button:hover image { color: @lsm_button_foreground; }"
        "button:active, button:checked {"
        " background-color: @lsm_selection; color: @lsm_selection_text;"
        " border-color: @lsm_neutral;"
        "}"
        "button:disabled {"
        " background-color: @lsm_input; color: @lsm_subtle;"
        " border-color: @lsm_border;"
        "}"
        "entry, spinbutton {"
        " background-image: none; background-color: @lsm_input; color: @lsm_text;"
        " border: 1px solid @lsm_connection_border; box-shadow: none;"
        "}",
        typography->ui_family,
        (unsigned int)typography->ui_regular_weight,
        typography->brand_family,
        typography->ui_family,
        (unsigned int)typography->brand_weight,
        (unsigned int)typography->ui_bold_weight,
        (unsigned int)palette->background_rgb,
        (unsigned int)palette->panel_rgb,
        (unsigned int)palette->card_rgb,
        (unsigned int)palette->surface_rgb,
        (unsigned int)palette->input_rgb,
        (unsigned int)palette->border_rgb,
        (unsigned int)palette->text_rgb,
        (unsigned int)palette->title_rgb,
        (unsigned int)palette->muted_rgb,
        (unsigned int)palette->subtle_rgb,
        (unsigned int)palette->button_background_rgb,
        (unsigned int)palette->button_foreground_rgb,
        (unsigned int)palette->neutral_accent_rgb,
        (unsigned int)palette->selection_background_rgb,
        (unsigned int)palette->selection_foreground_rgb,
        (unsigned int)palette->card_hover_rgb,
        (unsigned int)palette->surface_hover_rgb,
        (unsigned int)palette->operation_rgb,
        (unsigned int)palette->operation_hover_rgb,
        (unsigned int)palette->titlebar_rgb,
        (unsigned int)palette->connection_rgb,
        (unsigned int)palette->connection_border_rgb,
        (unsigned int)palette->heading_rgb,
        (unsigned int)palette->summary_rgb,
        (unsigned int)palette->kicker_rgb,
        (unsigned int)palette->detail_label_rgb,
        (unsigned int)palette->selected_summary_rgb,
        (unsigned int)palette->status_border_rgb,
        (unsigned int)palette->accent_foreground_rgb,
        (unsigned int)palette->accent_hover_rgb);

    g_string_append(
        css,
        "notebook > header {"
        " background-color: @lsm_panel; color: @lsm_summary;"
        " border-color: @lsm_border;"
        "}"
        "notebook > header > tabs > tab {"
        " background-color: transparent; color: @lsm_summary;"
        " border: 0; border-bottom: 2px solid transparent;"
        " padding: 5px 10px;"
        "}"
        "notebook > header > tabs > tab label { color: @lsm_summary; }"
        "notebook > header > tabs > tab:hover {"
        " background-color: @lsm_card_hover;"
        "}"
        "notebook > header > tabs > tab:hover label { color: @lsm_title; }"
        "notebook > header > tabs > tab:checked {"
        " background-color: transparent; border-bottom-color: @lsm_neutral;"
        "}"
        "notebook > header > tabs > tab:checked label { color: @lsm_heading; }"
        "treeview, textview, textview text {"
        " background-color: @lsm_input; color: @lsm_text;"
        " border-color: @lsm_border;"
        "}"
        "viewport, scrolledwindow {"
        " background-color: @lsm_background; color: @lsm_text;"
        " border-color: @lsm_border;"
        "}"
        "entry selection, textview text selection, treeview.view:selected {"
        " background-color: @lsm_selection; color: @lsm_selection_text;"
        "}"
        "scrollbar trough { background-color: @lsm_surface; }"
        "scrollbar slider { background-color: @lsm_border; }"
        "scrollbar slider:hover { background-color: @lsm_summary; }"
        "tooltip {"
        " background-color: @lsm_card; color: @lsm_title;"
        " border: 1px solid @lsm_border;"
        "}");

    g_string_append(
        css,
        "#lsm-performance-sidebar, #lsm-performance-sidebar viewport {"
        " background-color: @lsm_panel; border-color: @lsm_border;"
        "}"
        "#lsm-performance-content, #lsm-performance-content viewport {"
        " background-color: @lsm_background; border-color: @lsm_border;"
        "}"
        "#lsm-performance-paned > separator {"
        " background-color: @lsm_border; min-width: 1px;"
        "}"
        "#lsm-side-button {"
        " background-image: none; background-color: transparent;"
        " color: @lsm_text; border: 1px solid transparent; box-shadow: none;"
        " border-radius: 6px; margin: 3px 7px; padding: 4px 6px;"
        "}"
        "#lsm-side-button:hover {"
        " background-color: @lsm_card_hover; border-color: transparent;"
        "}"
        "#lsm-side-button:checked {"
        " background-color: @lsm_selection; color: @lsm_selection_text;"
        " border-color: @lsm_border; border-left: 3px solid @lsm_neutral;"
        "}"
        ".lsm-side-title { color: @lsm_text; }"
        ".lsm-side-identifier { color: @lsm_kicker; }"
        ".lsm-side-value { color: @lsm_summary; }"
        "#lsm-side-button:checked .lsm-side-title { color: @lsm_heading; }"
        "#lsm-side-button:checked .lsm-side-identifier,"
        "#lsm-side-button:checked .lsm-side-value { color: @lsm_selected_summary; }"
        ".lsm-metric-caption { color: @lsm_detail_label; }"
        ".lsm-metric-value { color: @lsm_heading; }");

    gtk_css_provider_load_from_data(
        app->shell.theme_provider, css->str, (gssize)css->len, NULL);
    g_string_free(css, TRUE);
    if (app->shell.window) gtk_widget_queue_draw(app->shell.window);
}

/* Window and tab lifecycle. Expensive pages refresh on demand as well as by
 * their bounded background cadence. */

static gboolean on_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    (void)widget; (void)event;
    LsmApp *app = user_data;
    lsm_app_shell_save_page_scroll(app, app->runtime.active_tab);
    lsm_details_save_layout(app);
    lsm_preferences_save(app);
    g_application_quit(G_APPLICATION(app->application));
    return TRUE;
}

void lsm_app_shell_save_page_scroll(LsmApp *app, gint page)
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
    lsm_app_shell_save_page_scroll(app, app->runtime.active_tab);
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
            (void)lsm_app_refresh_processes_if_due(app, FALSE);
            lsm_processes_present_snapshot(app);
            break;
        case LSM_TAB_DETAILS:
            (void)lsm_app_refresh_processes_if_due(app, FALSE);
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
        lsm_app_menu_refresh(NULL, app);
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
        lsm_app_menu_save_snapshot(NULL, app);
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


void lsm_app_shell_connect_window(LsmApp *app)
{
    if (!app || !app->shell.window) return;
    g_signal_connect(app->shell.window, "delete-event",
                     G_CALLBACK(on_delete_event), app);
    g_signal_connect(app->shell.window, "configure-event",
                     G_CALLBACK(on_window_configure), app);
    g_signal_connect(app->shell.window, "window-state-event",
                     G_CALLBACK(on_window_state), app);
    g_signal_connect(app->shell.window, "key-press-event",
                     G_CALLBACK(on_key_press), app);
}

void lsm_app_shell_connect_notebook(LsmApp *app)
{
    if (!app || !app->shell.notebook) return;
    g_signal_connect(app->shell.notebook, "switch-page",
                     G_CALLBACK(on_tab_switched), app);
}

void lsm_app_shell_cancel_pending(LsmApp *app)
{
    if (!app) return;
    if (app->runtime.window_restore_reflow_source) {
        g_source_remove(app->runtime.window_restore_reflow_source);
        app->runtime.window_restore_reflow_source = 0U;
    }
    if (app->shell.theme_provider) {
        GdkScreen *screen = gdk_screen_get_default();
        if (screen)
            gtk_style_context_remove_provider_for_screen(
                screen, GTK_STYLE_PROVIDER(app->shell.theme_provider));
        g_object_unref(app->shell.theme_provider);
        app->shell.theme_provider = NULL;
    }
}
