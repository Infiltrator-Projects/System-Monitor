// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_shell.c
 * @brief Global window chrome, navigation and compact-summary presentation.
 *
 * The shell owns visible chrome and resource navigation only. Theme projection
 * lives in app_theme.c, page activation in app_page_registry.c, keyboard policy
 * in app_keyboard.c and window-manager mechanics in app_shell_window.c.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_shell.h"

#include "app_internal.h"
#include "common.h"
#include "performance.h"
#include "preferences.h"

#include <string.h>

static void minimize_window(GtkButton *button, gpointer user_data)
{
    (void)button;
    gtk_window_iconify(GTK_WINDOW(user_data));
}

static void toggle_maximize_window(GtkButton *button, gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);

    (void)button;
    if (gtk_window_is_maximized(window))
        gtk_window_unmaximize(window);
    else
        gtk_window_maximize(window);
}

static void close_window(GtkButton *button, gpointer user_data)
{
    (void)button;
    gtk_window_close(GTK_WINDOW(user_data));
}

static void show_settings(GtkButton *button, gpointer user_data)
{
    (void)button;
    lsm_preferences_show(user_data);
}

static GtkWidget *make_window_control(const char *icon_name,
                                      const char *tooltip,
                                      const char *css_class)
{
    GtkWidget *button =
        gtk_button_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    GtkStyleContext *context = gtk_widget_get_style_context(button);

    gtk_style_context_add_class(context, "lsm-window-control");
    if (css_class)
        gtk_style_context_add_class(context, css_class);
    gtk_widget_set_tooltip_text(button, tooltip);
    return button;
}

GtkWidget *lsm_app_shell_build_header(LsmApp *app)
{
    if (!app || !app->shell.window)
        return gtk_header_bar_new();

    GtkWindow *window = GTK_WINDOW(app->shell.window);
    GtkWidget *header = gtk_header_bar_new();
    GtkWidget *brand = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon_wrap = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *icon = gtk_image_new_from_icon_name(
        LSM_EXECUTABLE_NAME, GTK_ICON_SIZE_BUTTON);
    GtkWidget *copy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *title = gtk_label_new(LSM_PROGRAM_NAME);
    GtkWidget *subtitle = gtk_label_new("Infiltrator OS");
    GtkWidget *header_end = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *settings = make_window_control(
        "preferences-system-symbolic", "Settings", NULL);
    GtkWidget *minimize = make_window_control(
        "window-minimize-symbolic", "Minimize", NULL);
    GtkWidget *maximize = make_window_control(
        "window-maximize-symbolic", "Maximize / Restore", NULL);
    GtkWidget *close = make_window_control(
        "window-close-symbolic", "Close", "lsm-window-control-close");
    GtkWidget *empty_title = gtk_label_new("");

    gtk_widget_set_name(header, "lsm-shell-header");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), FALSE);
    gtk_header_bar_set_custom_title(GTK_HEADER_BAR(header), empty_title);

    gtk_widget_set_name(brand, "lsm-header-brand");
    gtk_widget_set_name(icon_wrap, "lsm-header-brand-icon");
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 28);
    gtk_box_pack_start(GTK_BOX(icon_wrap), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brand), icon_wrap, FALSE, FALSE, 0);

    gtk_widget_set_name(title, "lsm-header-brand-title");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_name(subtitle, "lsm-header-brand-subtitle");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(copy), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(copy), subtitle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(brand), copy, FALSE, FALSE, 0);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), brand);

    gtk_widget_set_name(header_end, "lsm-header-end");
    gtk_widget_set_name(settings, "lsm-settings-button");
    g_signal_connect(
        settings, "clicked", G_CALLBACK(show_settings), app);
    gtk_box_pack_start(GTK_BOX(header_end), settings, FALSE, FALSE, 0);
    g_signal_connect(
        minimize, "clicked", G_CALLBACK(minimize_window), window);
    g_signal_connect(
        maximize, "clicked", G_CALLBACK(toggle_maximize_window), window);
    g_signal_connect(
        close, "clicked", G_CALLBACK(close_window), window);
    gtk_box_pack_start(GTK_BOX(header_end), minimize, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_end), maximize, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header_end), close, FALSE, FALSE, 0);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), header_end);

    g_object_set_data(G_OBJECT(window), "lsm-shell-header", header);
    g_object_set_data(G_OBJECT(window), "lsm-settings-button", settings);
    g_object_set_data(G_OBJECT(window), "lsm-minimize-button", minimize);
    g_object_set_data(G_OBJECT(window), "lsm-maximize-button", maximize);
    g_object_set_data(G_OBJECT(window), "lsm-close-button", close);
    return header;
}

static void sync_integrated_overview_chrome(LsmApp *app)
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

void lsm_app_shell_apply_compact_summary(LsmApp *app)
{
    if (!app || !app->shell.window) return;
    if (app->shell.notebook)
        gtk_widget_set_visible(app->shell.notebook, !app->runtime.compact_summary);
    if (app->shell.main_navigation)
        gtk_widget_set_visible(
            app->shell.main_navigation, !app->runtime.compact_summary);
    if (app->shell.pause_indicator)
        gtk_widget_set_visible(app->shell.pause_indicator,
                               app->runtime.paused && !app->runtime.compact_summary);
    sync_integrated_overview_chrome(app);
    if (app->runtime.compact_summary) {
        app->runtime.compact_restore_maximized =
            gtk_window_is_maximized(GTK_WINDOW(app->shell.window));
        gtk_window_unmaximize(GTK_WINDOW(app->shell.window));
        gtk_window_resize(GTK_WINDOW(app->shell.window), 760, 150);
    } else {
        const gboolean restore_maximized =
            app->runtime.compact_restore_maximized;
        app->runtime.compact_restore_maximized = FALSE;
        gtk_window_resize(GTK_WINDOW(app->shell.window), app->runtime.window_width,
                          app->runtime.window_height);
        if (restore_maximized)
            gtk_window_maximize(GTK_WINDOW(app->shell.window));
    }
}

static const char *navigation_resource_icon(LsmPageType type)
{
    switch (type) {
        case LSM_PAGE_CPU: return "applications-system-symbolic";
        case LSM_PAGE_MEMORY: return "view-grid-symbolic";
        case LSM_PAGE_DISK: return "drive-harddisk-symbolic";
        case LSM_PAGE_NETWORK: return "network-wireless-symbolic";
        case LSM_PAGE_GPU: return "video-display-symbolic";
        case LSM_PAGE_BATTERY: return "battery-good-symbolic";
        case LSM_PAGE_BLUETOOTH: return "bluetooth-symbolic";
        case LSM_PAGE_NPU: return "applications-engineering-symbolic";
        case LSM_PAGE_COUNT: return "applications-system-symbolic";
    }
    return "applications-system-symbolic";
}

static const char *navigation_resource_class(LsmPageType type)
{
    switch (type) {
        case LSM_PAGE_CPU: return "lsm-nav-cpu";
        case LSM_PAGE_MEMORY: return "lsm-nav-memory";
        case LSM_PAGE_DISK: return "lsm-nav-disk";
        case LSM_PAGE_NETWORK: return "lsm-nav-network";
        case LSM_PAGE_GPU: return "lsm-nav-gpu";
        case LSM_PAGE_BATTERY: return "lsm-nav-battery";
        case LSM_PAGE_BLUETOOTH: return "lsm-nav-bluetooth";
        case LSM_PAGE_NPU: return "lsm-nav-npu";
        case LSM_PAGE_COUNT: return "lsm-nav-neutral";
    }
    return "lsm-nav-neutral";
}

static gboolean navigation_resource_available(const LsmApp *app,
                                              LsmPageType type)
{
    if (!app) return FALSE;

    const LsmPageType group = lsm_performance_navigation_group(type);
    switch (group) {
        case LSM_PAGE_CPU:
        case LSM_PAGE_MEMORY:
            return TRUE;
        case LSM_PAGE_DISK:
            return app->monitor.disk_count > 0U;
        case LSM_PAGE_NETWORK:
            return app->monitor.net_count > 0U ||
                   app->monitor.bluetooth_device_count > 0U;
        case LSM_PAGE_GPU:
            return app->monitor.gpu_count > 0U ||
                   app->monitor.npu_count > 0U;
        case LSM_PAGE_BATTERY:
            return app->monitor.battery_count > 0U;
        case LSM_PAGE_BLUETOOTH:
        case LSM_PAGE_NPU:
        case LSM_PAGE_COUNT:
            return FALSE;
    }
    return FALSE;
}

static GtkWidget *navigation_button(LsmApp *app, const char *label,
                                    const char *icon_name,
                                    const char *style_class)
{
    const gboolean compact = app && app->runtime.compact_layout;
    GtkWidget *button = gtk_toggle_button_new();
    gtk_widget_set_name(button, "lsm-main-nav-button");
    gtk_widget_set_size_request(
        button,
        compact ? LSM_MAIN_NAV_BUTTON_COMPACT_WIDTH
                : LSM_MAIN_NAV_BUTTON_WIDTH,
        48);
    if (style_class)
        gtk_style_context_add_class(
            gtk_widget_get_style_context(button), style_class);

    GtkWidget *row = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, compact ? 0 : 12);
    GtkWidget *icon =
        gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_BUTTON);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(icon), "lsm-main-nav-icon");

    GtkWidget *text = gtk_label_new(label);
    gtk_widget_set_halign(text, GTK_ALIGN_START);
    gtk_widget_set_valign(text, GTK_ALIGN_CENTER);
    gtk_widget_set_no_show_all(text, TRUE);
    gtk_widget_set_visible(text, !compact);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(text), "lsm-main-nav-label");
    g_object_set_data(G_OBJECT(button), "lsm-nav-label-widget", text);
    if (compact) gtk_widget_set_tooltip_text(button, label);

    gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), text, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(button), row);
    return button;
}

static void navigation_tab_clicked(GtkButton *button, gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app || app->shell.navigation_syncing || !app->shell.notebook) return;
    const gint encoded = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "lsm-nav-tab"));
    const gint tab = encoded - 1;
    if (tab < 0 || tab >= LSM_TAB_COUNT) return;
    gtk_notebook_set_current_page(GTK_NOTEBOOK(app->shell.notebook), tab);
    lsm_app_shell_sync_navigation(app);
}

static void navigation_resource_clicked(GtkButton *button, gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app || app->shell.navigation_syncing) return;
    const gint encoded = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "lsm-nav-resource"));
    const gint type = encoded - 1;
    if (type < LSM_PAGE_CPU || type >= LSM_PAGE_COUNT) return;
    lsm_performance_show_resource(app, (LsmPageType)type, 0U);
    lsm_app_shell_sync_navigation(app);
}

static GtkWidget *navigation_tab_button(LsmApp *app, LsmTabIndex tab,
                                        const char *label,
                                        const char *icon_name,
                                        const char *style_class)
{
    GtkWidget *button = navigation_button(
        app, label, icon_name, style_class);
    g_object_set_data(
        G_OBJECT(button), "lsm-nav-tab", GINT_TO_POINTER((gint)tab + 1));
    g_signal_connect(
        button, "clicked", G_CALLBACK(navigation_tab_clicked), app);
    app->shell.navigation_tab_buttons[tab] = button;
    return button;
}

static GtkWidget *navigation_resource_button(LsmApp *app, LsmPageType type,
                                             const char *label)
{
    GtkWidget *button = navigation_button(
        app, label, navigation_resource_icon(type),
        navigation_resource_class(type));
    g_object_set_data(
        G_OBJECT(button), "lsm-nav-resource",
        GINT_TO_POINTER((gint)type + 1));
    g_signal_connect(
        button, "clicked", G_CALLBACK(navigation_resource_clicked), app);
    app->shell.navigation_resource_buttons[type] = button;
    return button;
}

GtkWidget *lsm_app_shell_build_navigation(LsmApp *app)
{
    if (!app) return gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_name(scroller, "lsm-main-navigation");
    gtk_widget_set_size_request(
        scroller,
        app->runtime.compact_layout
            ? LSM_MAIN_NAV_COMPACT_WIDTH : LSM_MAIN_NAV_WIDTH,
        -1);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *rail = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_container_set_border_width(GTK_CONTAINER(rail), 8);
    gtk_container_add(GTK_CONTAINER(scroller), rail);

    gtk_box_pack_start(
        GTK_BOX(rail),
        navigation_tab_button(
            app, LSM_TAB_OVERVIEW, "Overview",
            "go-home-symbolic", "lsm-nav-overview"),
        FALSE, FALSE, 0);

    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(separator), "lsm-main-nav-separator");
    gtk_box_pack_start(GTK_BOX(rail), separator, FALSE, FALSE, 5);

    const LsmPageType resources[] = {
        LSM_PAGE_CPU,
        LSM_PAGE_MEMORY,
        LSM_PAGE_DISK,
        LSM_PAGE_NETWORK,
        LSM_PAGE_GPU,
        LSM_PAGE_BATTERY
    };
    const char *const resource_labels[] = {
        "CPU", "Memory", "Disks", "Network", "GPU", "Battery"
    };
    for (guint index = 0U; index < G_N_ELEMENTS(resources); index++)
        gtk_box_pack_start(
            GTK_BOX(rail),
            navigation_resource_button(
                app, resources[index], resource_labels[index]),
            FALSE, FALSE, 0);

    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(separator), "lsm-main-nav-separator");
    gtk_box_pack_start(GTK_BOX(rail), separator, FALSE, FALSE, 5);

    const struct {
        LsmTabIndex tab;
        const char *label;
        const char *icon;
        const char *style_class;
    } pages[] = {
        {LSM_TAB_PROCESSES, "Processes", "view-list-symbolic", "lsm-nav-processes"},
        {LSM_TAB_APP_HISTORY, "App History", "document-open-recent-symbolic", "lsm-nav-history"},
        {LSM_TAB_STARTUP, "Startup Apps", "system-run-symbolic", "lsm-nav-startup"},
        {LSM_TAB_USERS, "Users", "system-users-symbolic", "lsm-nav-users"},
        {LSM_TAB_DETAILS, "Details", "dialog-information-symbolic", "lsm-nav-details"},
        {LSM_TAB_SERVICES, "Services", "emblem-system-symbolic", "lsm-nav-services"},
        {LSM_TAB_FILESYSTEMS, "File Systems", "folder-symbolic", "lsm-nav-filesystems"}
    };
    for (guint index = 0U; index < G_N_ELEMENTS(pages); index++)
        gtk_box_pack_start(
            GTK_BOX(rail),
            navigation_tab_button(
                app, pages[index].tab, pages[index].label,
                pages[index].icon, pages[index].style_class),
            FALSE, FALSE, 0);

    return scroller;
}

static LsmPageType navigation_visible_performance_type(const LsmApp *app)
{
    if (!app || !app->performance.performance_stack ||
        !app->performance.device_pages)
        return LSM_PAGE_COUNT;
    const char *visible = gtk_stack_get_visible_child_name(
        GTK_STACK(app->performance.performance_stack));
    if (!visible) return LSM_PAGE_COUNT;

    for (guint index = 0U; index < app->performance.device_pages->len; index++) {
        const LsmDevicePage *page =
            g_ptr_array_index(app->performance.device_pages, index);
        if (page && strcmp(page->stack_name, visible) == 0)
            return lsm_performance_navigation_group(page->type);
    }
    return LSM_PAGE_COUNT;
}

void lsm_app_shell_sync_navigation(LsmApp *app)
{
    if (!app || app->shell.navigation_syncing) return;
    app->shell.navigation_syncing = TRUE;

    const LsmTabIndex active =
        app->runtime.active_tab >= 0 &&
        app->runtime.active_tab < LSM_TAB_COUNT
            ? (LsmTabIndex)app->runtime.active_tab
            : LSM_TAB_COUNT;
    const LsmPageType resource =
        active == LSM_TAB_PERFORMANCE
            ? navigation_visible_performance_type(app)
            : LSM_PAGE_COUNT;

    for (gint tab = 0; tab < LSM_TAB_COUNT; tab++) {
        GtkWidget *button = app->shell.navigation_tab_buttons[tab];
        if (button)
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(button), active == (LsmTabIndex)tab);
    }
    for (gint type = 0; type < LSM_PAGE_COUNT; type++) {
        GtkWidget *button = app->shell.navigation_resource_buttons[type];
        if (button) {
            const LsmPageType page_type = (LsmPageType)type;
            const gboolean available =
                navigation_resource_available(app, page_type);
            gtk_widget_set_visible(button, available);
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(button),
                available && resource == page_type);
        }
    }
    app->shell.navigation_syncing = FALSE;
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
