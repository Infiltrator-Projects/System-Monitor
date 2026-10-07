// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_theme.c
 * @brief System Monitor GTK theme projection and host-theme observation.
 *
 * Theme construction is isolated from shell navigation and window lifecycle so
 * visual policy can evolve without making the shell a cross-feature change
 * hotspot. Semantic palette, typography and geometry values remain supplied by
 * Infiltratr Common.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_shell.h"

#include "app_internal.h"
#include "common.h"

#include <infiltratr/design.h>

#include <stdio.h>

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
        dark = dark || lsm_ascii_contains_ci(theme_name, "dark");
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
    const gboolean night_theme =
        app->runtime.theme_mode == INFILTRATR_THEME_NIGHT ||
        (app->runtime.theme_mode == INFILTRATR_THEME_SYSTEM && system_dark);
    const InfiltratrThemePalette *palette =
        infiltratr_theme_resolve(app->runtime.theme_mode, system_dark);
    const InfiltratrTypography *typography = infiltratr_typography();
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    if (!palette || !typography || !metrics || !typography->ui_family ||
        !typography->brand_family)
        return;

    GString *css = g_string_new(NULL);
    if (!css) return;

    g_string_append_printf(
        css,
        "* { font-family: \"%s\"; font-weight: %u; }"
        "headerbar .title, .titlebar .title, #lsm-header-brand-title,"
        " .lsm-performance-title {"
        " font-family: \"%s\"; font-weight: %u;"
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
        "@define-color lsm_warning #%06X;"
        "@define-color lsm_warning_muted #%06X;"
        "@define-color lsm_warning_border #%06X;"
        "@define-color lsm_fault #%06X;"
        "@define-color lsm_success #%06X;"
        "@define-color lsm_accent_foreground #%06X;"
        "@define-color lsm_accent_hover #%06X;"
        "window, dialog, .background {"
        " background-color: @lsm_background; color: @lsm_text;"
        "}"
        "headerbar, .titlebar {"
        " min-height: 44px; background-image: none; background-color: @lsm_titlebar;"
        " color: @lsm_title; border-bottom: 1px solid @lsm_border;"
        "}"
        "#lsm-summary-bar {"
        " background-image: none; background-color: @lsm_connection;"
        " color: @lsm_text; border: 1px solid @lsm_connection_border;"
        "}"
        "frame {"
        " background-image: linear-gradient(to bottom right, @lsm_card, @lsm_surface);"
        " color: @lsm_text; border: 1px solid @lsm_border;"
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
        "button > label, button > image,"
        "combobox button > label, combobox button > image {"
        " color: @lsm_button_foreground;"
        "}"
        "button:hover, combobox button:hover {"
        " background-color: @lsm_neutral; border-color: @lsm_neutral;"
        "}"
        "button:hover > label, button:hover > image { color: @lsm_button_foreground; }"
        "button:active, button:checked {"
        " background-color: @lsm_selection; color: @lsm_selection_text;"
        " border-color: @lsm_neutral;"
        "}"
        "button:active > label, button:active > image,"
        "button:checked > label, button:checked > image { color: @lsm_selection_text; }"
        "button:disabled {"
        " background-color: @lsm_input; color: @lsm_subtle;"
        " border-color: @lsm_border;"
        "}"
        "button:disabled > label, button:disabled > image { color: @lsm_subtle; }"
        "entry, spinbutton {"
        " background-image: none; background-color: @lsm_input; color: @lsm_text;"
        " border: 1px solid @lsm_connection_border; box-shadow: none;"
        "}",
        typography->ui_family,
        (unsigned int)typography->ui_regular_weight,
        typography->brand_family,
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
        (unsigned int)palette->warning_rgb,
        (unsigned int)palette->warning_muted_rgb,
        (unsigned int)palette->warning_border_rgb,
        (unsigned int)palette->fault_rgb,
        (unsigned int)palette->success_rgb,
        (unsigned int)palette->accent_foreground_rgb,
        (unsigned int)palette->accent_hover_rgb);

    g_string_append(css,
        ".lsm-preferences { font-size: 14px; }"
        ".lsm-preferences-title { font-size: 26px; color: @lsm_title; }"
        ".lsm-preferences-note { color: @lsm_muted; }"
        ".lsm-preferences notebook {"
        " background-color: @lsm_card; border: 1px solid @lsm_border;"
        " border-radius: 12px; }"
        ".lsm-preferences notebook viewport, .lsm-preferences notebook stack {"
        " background-color: @lsm_card; border-radius: 0 0 12px 12px; }"
        ".lsm-preferences notebook header {"
        " background-color: @lsm_panel; border-bottom: 1px solid @lsm_border; }"
        ".lsm-preferences notebook tab { padding: 10px 18px; }"
        ".lsm-preferences notebook tab:checked {"
        " color: @lsm_title; border-bottom: 2px solid @lsm_neutral; }"
        ".lsm-preferences combobox button {"
        " background-color: @lsm_input; color: @lsm_text; padding: 6px 12px; }"
        ".lsm-preferences combobox button label,"
        " .lsm-preferences combobox button image { color: @lsm_text; }"
        ".lsm-preferences combobox button:hover {"
        " background-color: @lsm_surface_hover; border-color: @lsm_neutral; }"
        ".lsm-preference-toggle {"
        " padding: 12px 8px; border-bottom: 1px solid @lsm_border; }"
        ".lsm-preference-toggle check { min-width: 18px; min-height: 18px;"
        " margin-right: 10px; border-radius: 5px;"
        " background-color: @lsm_input; border: 1px solid @lsm_border; }"
        ".lsm-preference-toggle check:checked {"
        " background-color: @lsm_neutral; border-color: @lsm_neutral;"
        " color: @lsm_accent_foreground; }"
        ".lsm-preferences .dialog-action-area { padding: 4px 20px 16px; }"
        ".lsm-preferences .dialog-action-area button {"
        " min-width: 84px; min-height: 32px; padding: 4px 12px;"
        " background-color: @lsm_input; color: @lsm_text; }"
        ".lsm-preferences .dialog-action-area button label { color: @lsm_text; }"
        ".lsm-preferences button.suggested-action {"
        " background-color: @lsm_neutral; border-color: @lsm_neutral;"
        " color: @lsm_accent_foreground; }"
        ".lsm-preferences button.suggested-action label {"
        " color: @lsm_accent_foreground; }");

    /* Keep branded window chrome in a separate literal so the strict
     * ISO C documentation build remains below the 4095-byte literal floor. */
    g_string_append(
        css,
        "#lsm-shell-header {"
        " min-height: 58px; padding: 6px 10px;"
        " background-image: linear-gradient(to right, #06131f, #08263a);"
        " background-color: #06131f; border-bottom: 1px solid @lsm_border;"
        "}"
        "#lsm-header-brand { padding: 2px 4px; }"
        "#lsm-header-brand-icon {"
        " background-color: @lsm_card; border: 1px solid @lsm_border;"
        " border-radius: 12px; padding: 7px;"
        " box-shadow: 0 0 18px alpha(@lsm_neutral, 0.18);"
        "}"
        "#lsm-header-brand-icon image { color: @lsm_neutral; }"
        "#lsm-header-brand-title { color: @lsm_title; font-size: 20px; }"
        "#lsm-header-brand-subtitle { color: @lsm_muted; font-size: 11px; }"
        "#lsm-header-end { margin-left: 10px; }"
        ".lsm-window-control {"
        " min-width: 30px; min-height: 30px; padding: 4px;"
        " background-image: none; background-color: transparent;"
        " border: 1px solid transparent; border-radius: 8px; box-shadow: none;"
        "}"
        "#lsm-shell-header .lsm-window-control,"
        "#lsm-shell-header .lsm-window-control > image { color: @lsm_title; }"
        ".lsm-window-control:hover {"
        " background-color: @lsm_surface_hover; border-color: @lsm_border;"
        "}"
        "#lsm-shell-header .lsm-window-control-close:hover,"
        "#lsm-shell-header .lsm-window-control-close:hover > image {"
        " background-color: @lsm_fault; color: #ffffff;"
        "}");

    g_string_append(
        css,
        "notebook > header {"
        " background-color: @lsm_panel; color: @lsm_summary;"
        " border-color: @lsm_border;"
        "}"
        "notebook > header > tabs > tab {"
        " background-color: transparent; color: @lsm_summary;"
        " border: 0; border-bottom: 2px solid transparent;"
        " padding: 6px 10px;"
        "}"
        "notebook > header > tabs > tab label { color: @lsm_summary; }"
        "notebook > header > tabs > tab:hover {"
        " background-color: @lsm_card_hover;"
        "}"
        "notebook > header > tabs > tab:hover label { color: @lsm_title; }"
        "notebook > header > tabs > tab:checked {"
        " background-color: transparent; border-bottom-color: @lsm_neutral;"
        "}"
        "notebook > header > tabs > tab:checked label { color: @lsm_neutral; }"
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
        "#lsm-main-navigation, #lsm-main-navigation viewport {"
        " background-image: linear-gradient(to bottom, @lsm_panel, @lsm_background);"
        " background-color: @lsm_panel;"
        " border-color: @lsm_connection_border;"
        "}"
        "#lsm-main-navigation { border-right: 1px solid @lsm_connection_border; }"
        "#lsm-main-nav-button {"
        " background-image: none; background-color: transparent;"
        " color: @lsm_summary; border: 1px solid transparent;"
        " box-shadow: none; margin: 2px 0; padding: 8px 10px;"
        "}"
        "#lsm-main-nav-button:hover {"
        " background-color: @lsm_card_hover;"
        " border-color: alpha(@lsm_neutral, 0.20);"
        "}"
        "#lsm-main-nav-button:checked {"
        " background-image: linear-gradient(to right,"
        " alpha(@lsm_neutral, 0.68), alpha(@lsm_accent_hover, 0.42));"
        " color: @lsm_title;"
        " border-color: alpha(@lsm_neutral, 0.92);"
        " box-shadow: inset 3px 0 @lsm_neutral;"
        "}"
        "#lsm-main-nav-button .lsm-main-nav-label {"
        " color: @lsm_summary; font-size: 15px; font-weight: 600;"
        "}"
        "#lsm-main-nav-button:hover .lsm-main-nav-label,"
        "#lsm-main-nav-button:checked .lsm-main-nav-label { color: @lsm_title; }"
        "#lsm-main-nav-button .lsm-main-nav-icon { color: @lsm_neutral; }"
        "#lsm-main-nav-button.lsm-nav-memory .lsm-main-nav-icon { color: #9b65ff; }"
        "#lsm-main-nav-button.lsm-nav-disk .lsm-main-nav-icon { color: #8fd94e; }"
        "#lsm-main-nav-button.lsm-nav-network .lsm-main-nav-icon { color: #30d9ef; }"
        "#lsm-main-nav-button.lsm-nav-gpu .lsm-main-nav-icon { color: #de68f2; }"
        "#lsm-main-nav-button.lsm-nav-battery .lsm-main-nav-icon { color: #6ae66a; }"
        "#lsm-main-nav-button.lsm-nav-processes .lsm-main-nav-icon,"
        "#lsm-main-nav-button.lsm-nav-history .lsm-main-nav-icon,"
        "#lsm-main-nav-button.lsm-nav-startup .lsm-main-nav-icon,"
        "#lsm-main-nav-button.lsm-nav-users .lsm-main-nav-icon,"
        "#lsm-main-nav-button.lsm-nav-details .lsm-main-nav-icon,"
        "#lsm-main-nav-button.lsm-nav-services .lsm-main-nav-icon,"
        "#lsm-main-nav-button.lsm-nav-filesystems .lsm-main-nav-icon {"
        " color: @lsm_selected_summary;"
        "}"
        ".lsm-main-nav-separator {"
        " background-color: alpha(@lsm_connection_border, 0.78);"
        " min-height: 1px;"
        "}");

    /* Keep each concatenated CSS literal below the ISO C translation limit.
     * Clang's documentation build deliberately enforces that portability
     * bound with -Werror. */
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
        " background-color: @lsm_card_hover;"
        " border-color: alpha(@lsm_text, 0.10);"
        "}"
        "#lsm-side-button:checked {"
        " background-color: alpha(@lsm_neutral, 0.075);"
        " color: @lsm_selection_text;"
        " border-color: alpha(@lsm_neutral, 0.48);"
        "}"
        "#lsm-side-button .lsm-side-title { color: @lsm_text; }"
        "#lsm-side-button .lsm-side-identifier { color: @lsm_kicker; }"
        "#lsm-side-button .lsm-side-value { color: @lsm_summary; }"
        "#lsm-side-button:checked .lsm-side-title { color: @lsm_neutral; }"
        "#lsm-side-button:checked .lsm-side-identifier,"
        "#lsm-side-button:checked .lsm-side-value { color: @lsm_selected_summary; }"
        ".lsm-summary-caption { color: @lsm_summary; font-size: 11px; }"
        ".lsm-summary-value { color: @lsm_heading; font-weight: 700; }"
        ".lsm-performance-title { color: @lsm_heading; }"
        ".lsm-performance-summary { color: @lsm_summary; }"
        ".lsm-performance-card {"
        " background-image: linear-gradient(to bottom right, @lsm_card, @lsm_surface);"
        " border: 1px solid @lsm_border;"
        "}"
        ".lsm-performance-card separator { background-color: @lsm_border; }"
        ".lsm-performance-card:hover { border-color: @lsm_connection_border; }");

    g_string_append(
        css,
        "#lsm-overview-hero {"
        " background-image: linear-gradient(115deg,"
        " alpha(#006cff, 0.32), alpha(#172b55, 0.88) 46%,"
        " alpha(#8b2de2, 0.20));"
        " border: 1px solid alpha(#26c7ff, 0.42);"
        " box-shadow: 0 0 12px alpha(#00adef, 0.10);"
        "}"
        "#lsm-overview-brand-mark {"
        " min-width: 52px; min-height: 52px;"
        " background-image: linear-gradient(135deg, alpha(#00e5ff, 0.22),"
        " alpha(#6d3cff, 0.12));"
        " border: 1px solid alpha(#19d9ff, 0.55);"
        " border-radius: 999px; padding: 8px;"
        "}"
        "#lsm-overview-brand-mark image { color: #39d8ff; }"
        "#lsm-overview-live {"
        " color: @lsm_success; background-color: alpha(@lsm_success, 0.08);"
        " border: 1px solid alpha(@lsm_success, 0.34);"
        " border-radius: 999px; padding: 7px 11px; font-weight: 700;"
        "}"
        "#lsm-overview-live.lsm-status-warning {"
        " color: @lsm_warning; border-color: alpha(@lsm_warning, 0.42);"
        " background-color: alpha(@lsm_warning, 0.09);"
        "}"
        "#lsm-overview-live.lsm-status-fault {"
        " color: @lsm_fault; border-color: alpha(@lsm_fault, 0.46);"
        " background-color: alpha(@lsm_fault, 0.09);"
        "}"
        "#lsm-overview-uptime {"
        " color: @lsm_title; background-color: alpha(@lsm_card, 0.42);"
        " border: 1px solid alpha(@lsm_connection_border, 0.54);"
        " border-radius: 10px; padding: 7px 12px; font-weight: 700;"
        "}"
        "#lsm-overview-root { background-color: @lsm_window; }"
        ".lsm-overview-card {"
        " background-image: linear-gradient(145deg, alpha(@lsm_neutral, 0.055),"
        " alpha(@lsm_neutral, 0.018)),"
        " linear-gradient(to bottom right, @lsm_card, @lsm_surface);"
        " box-shadow: 0 2px 10px alpha(#000000, 0.18);"
        " border: 1px solid alpha(@lsm_connection_border, 0.72);"
        "}"
        ".lsm-overview-card:hover { box-shadow: 0 3px 14px alpha(@lsm_neutral, 0.12); }"
        ".lsm-overview-card-title { font-size: 15px; font-weight: 700; }"
        ".lsm-overview-card-meta { color: @lsm_selected_summary; font-size: 12px; }"
        ".lsm-overview-chevron { color: @lsm_summary; opacity: 0.72; }"
        ".lsm-overview-value { color: @lsm_title; font-size: 24px; font-weight: 700; }"
        ".lsm-overview-cpu-pressure .lsm-overview-value,"
        ".lsm-overview-memory-pressure .lsm-overview-value,"
        ".lsm-overview-io-pressure .lsm-overview-value { font-size: 21px; }"
        ".lsm-overview-gauge-value { color: @lsm_title; font-size: 30px; font-weight: 800; }"
        ".lsm-overview-gauge-caption { color: @lsm_summary; font-size: 11px; }"
        ".lsm-overview-stat-row { border-top: 1px solid alpha(@lsm_border, 0.38); padding-top: 6px; }"
        ".lsm-overview-stat { padding: 0 8px; }"
        ".lsm-overview-stat-caption { color: @lsm_detail_label; font-size: 11px; }"
        ".lsm-overview-stat-value { color: @lsm_heading; font-size: 14px; font-weight: 700; }"
        ".lsm-overview-icon { min-width: 28px; }"
        ".lsm-overview-graph-icon {"
        " padding: 4px; opacity: 0.30;"
        " background-color: alpha(@lsm_card, 0.16);"
        " border-radius: 999px;"
        "}"
        ".lsm-overview-cpu .lsm-overview-icon { color: #28d7ff; }"
        ".lsm-overview-memory .lsm-overview-icon { color: #a56dff; }"
        ".lsm-overview-disk .lsm-overview-icon { color: #7ee84d; }"
        ".lsm-overview-network .lsm-overview-icon { color: #39dff0; }"
        ".lsm-overview-gpu .lsm-overview-icon { color: #e46dff; }"
        ".lsm-overview-temperature .lsm-overview-icon { color: #ffad45; }");

    g_string_append(
        css,
        "#lsm-overview-process-card {"
        " background-image: linear-gradient(to right, @lsm_card, @lsm_surface);"
        "}"
        ".lsm-overview-process-row {"
        " color: @lsm_heading;"
        " background-image: linear-gradient(to right, alpha(#007acb, 0.08), alpha(@lsm_neutral, 0.025));"
        " border: 1px solid alpha(@lsm_connection_border, 0.24);"
        " border-radius: 5px; padding: 3px 8px;"
        "}"
        ".lsm-overview-view-all {"
        " color: @lsm_title; background-image: linear-gradient(to bottom, alpha(#1b73c8, 0.42), alpha(#0d2b53, 0.48));"
        " border: 1px solid alpha(#58b9ff, 0.56); border-radius: 7px; padding: 4px 10px;"
        "}"
        ".lsm-overview-process-name { color: @lsm_title; font-weight: 700; }"
        ".lsm-overview-process-cpu { color: #4bd8ff; font-weight: 700; }"
        ".lsm-overview-process-memory { color: @lsm_summary; }"
        ".lsm-overview-process-bar trough { min-height: 6px; background-color: alpha(@lsm_neutral, 0.12); border-radius: 999px; }"
        ".lsm-overview-process-bar progress { min-height: 6px; background-image: linear-gradient(to right, #00d9ff, #7f42ff); border-radius: 999px; }"
        ".lsm-metric-caption { color: @lsm_detail_label; }"
        ".lsm-metric-value { color: @lsm_heading; }"
        ".lsm-state-warning { color: @lsm_warning; }"
        ".lsm-state-fault { color: @lsm_fault; }"
        ".lsm-state-success { color: @lsm_success; }"
        ".lsm-status-chip {"
        " padding: 5px 9px; border: 1px solid transparent;"
        " border-radius: 999px;"
        "}"
        ".lsm-status-warning {"
        " background-color: alpha(@lsm_warning, 0.08);"
        " border-color: alpha(@lsm_warning, 0.34); color: @lsm_warning_muted;"
        "}"
        ".lsm-status-fault {"
        " background-color: alpha(@lsm_fault, 0.08);"
        " border-color: alpha(@lsm_fault, 0.38); color: @lsm_fault;"
        "}");

    /* Day mode keeps semantic data colours while using explicit high-contrast
     * neutral menu surfaces. The contract is selector/role based rather than
     * tied to one source-line spelling so equivalent CSS refactors remain safe. */
    if (!night_theme) {
        g_string_append(
            css,
            "menubar > menuitem, menubar > menuitem label { color: #20252b; }"
            "menubar > menuitem:hover {"
            " background-image: none; background-color: #eef3f6; color: #111418;"
            "}"
            "menubar > menuitem:hover label { color: #111418; }"
            "menu {"
            " background-image: none; background-color: #ffffff;"
            " color: #111418; border-color: #b8c1c9;"
            "}"
            "menu menuitem { color: #111418; }"
            "menu menuitem label { color: #111418; }"
            "menu menuitem:hover {"
            " background-image: none; background-color: #dceaf2; color: #111418;"
            "}"
            "menu menuitem:hover label { color: #111418; }"
            "menu menuitem:disabled { color: #59636c; }"
            "menu menuitem:disabled label { color: #59636c; }"
            "menu menuitem check, menu menuitem radio { color: #20252b; }"
            "menu menuitem:disabled check, menu menuitem:disabled radio { color: #737d86; }"
            "menu separator { background-color: #c7cdd3; min-height: 1px; }"
            "popover.menu {"
            " background-image: none; background-color: #ffffff;"
            " color: #111418; border-color: #b8c1c9;"
            "}"
            "popover.menu modelbutton, popover.menu modelbutton label { color: #111418; }"
            "popover.menu modelbutton:hover { background-color: #dceaf2; color: #111418; }"
            "popover.menu modelbutton:hover label { color: #111418; }"
            "popover.menu modelbutton:disabled,"
            "popover.menu modelbutton:disabled label { color: #59636c; }"
            ".lsm-overview-card {"
            " background-image: none; background-color: @lsm_card;"
            " box-shadow: 0 1px 5px alpha(#000000, 0.07);"
            " border-color: alpha(@lsm_border, 0.78);"
            "}"
            ".lsm-overview-card:hover {"
            " box-shadow: 0 2px 9px alpha(#000000, 0.10);"
            "}"
            ".lsm-overview-stat-caption { color: @lsm_summary; }"
            "#lsm-overview-process-card {"
            " background-image: none; background-color: @lsm_card;"
            "}"
            ".lsm-overview-process-row {"
            " background-image: linear-gradient(to right,"
            " alpha(@lsm_neutral, 0.035), alpha(@lsm_card, 0.01));"
            " border-color: alpha(@lsm_border, 0.60);"
            "}");
    }

    g_string_append_printf(
        css,
        "#lsm-summary-bar {"
        " border-radius: %upx; margin: %upx %upx;"
        "}"
        "frame { border-radius: %upx; }"
        ".lsm-performance-card {"
        " border-radius: %upx; padding: %upx;"
        "}"
        "#lsm-overview-hero {"
        " border-radius: %upx; padding: %upx;"
        "}"
        "button, combobox button, entry, spinbutton { border-radius: %upx; }"
        "notebook > header > tabs > tab { border-radius: %upx %upx 0 0; }"
        "#lsm-side-button { border-radius: %upx; }"
        "#lsm-main-nav-button { border-radius: %upx; }",
        (unsigned int)metrics->card_radius,
        (unsigned int)metrics->compact_spacing,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->card_radius,
        (unsigned int)metrics->card_radius,
        (unsigned int)metrics->control_spacing,
        (unsigned int)metrics->card_radius,
        (unsigned int)(metrics->control_spacing * 2U),
        (unsigned int)metrics->control_radius,
        (unsigned int)metrics->small_radius,
        (unsigned int)metrics->small_radius,
        (unsigned int)metrics->small_radius,
        (unsigned int)metrics->control_radius);

    GError *css_error = NULL;
    if (!gtk_css_provider_load_from_data(
            app->shell.theme_provider, css->str, (gssize)css->len,
            &css_error)) {
        fprintf(stderr, "Unable to apply System Monitor theme CSS: %s\n",
                css_error && css_error->message
                    ? css_error->message : "unknown CSS parser error");
        g_clear_error(&css_error);
    }
    g_string_free(css, TRUE);
    if (app->shell.window) gtk_widget_queue_draw(app->shell.window);
}
