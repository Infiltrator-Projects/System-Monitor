// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_page_registry.c
 * @brief Central GTK page construction, activation, refresh and cadence registry.
 *
 * Top-level page wiring belongs here so the shell and runtime do not maintain
 * parallel switches, timer policy or feature include sets. The descriptor
 * table is indexed by stable LsmTabIndex identity and uses designated
 * initialisers so adding fields cannot silently retarget existing policy.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define LSM_APP_PAGE_REGISTRY_IMPLEMENTATION
#include "app_page_registry.h"
#undef LSM_APP_PAGE_REGISTRY_IMPLEMENTATION

#include "app_internal.h"
#include "app_menu.h"
#include "app_runtime.h"
#include "app_shell.h"
#include "details_page.h"
#include "filesystems.h"
#include "history.h"
#include "overview.h"
#include "performance.h"
#include "process_export.h"
#include "process_workspace.h"
#include "processes_ui.h"
#include "services.h"
#include "startup.h"
#include "users.h"

typedef void (*LsmPageBuildFunction)(LsmApp *app, GtkWidget *container);
typedef void (*LsmPageRefreshFunction)(LsmApp *app);
typedef GtkWidget *(*LsmPageSearchFunction)(LsmApp *app);

typedef struct {
    LsmPageBuildFunction build;
    LsmPageRefreshFunction refresh;
    LsmPageRefreshFunction enter;
    LsmPageSearchFunction search;
    GSourceFunc periodic_update;
    guint periodic_interval;
    gboolean periodic_whole_seconds;
    gboolean filesystem_interval;
    gboolean process_foreground;
    gboolean process_on_enter;
    gboolean enter_only_after_initial_build;
} LsmPageDescriptor;

static GtkWidget *processes_search(LsmApp *app)
{
    return app ? app->processes.processes_search : NULL;
}

static GtkWidget *details_search(LsmApp *app)
{
    return app ? app->details.details_search : NULL;
}

static GtkWidget *history_search(LsmApp *app)
{
    return app ? app->history.history_search : NULL;
}

static GtkWidget *filesystem_search(LsmApp *app)
{
    return app ? app->filesystem.filesystem_search : NULL;
}

static GtkWidget *startup_search(LsmApp *app)
{
    return app ? app->startup.startup_search : NULL;
}

static GtkWidget *services_search(LsmApp *app)
{
    return app ? app->services.services_search : NULL;
}

static const LsmPageDescriptor page_descriptors[LSM_TAB_COUNT] = {
    [LSM_TAB_PERFORMANCE] = {
        .build = lsm_performance_build,
        .refresh = lsm_performance_refresh,
        .enter = lsm_performance_refresh
    },
    [LSM_TAB_PROCESSES] = {
        .build = lsm_processes_build,
        .enter = lsm_processes_present_snapshot,
        .search = processes_search,
        .process_foreground = TRUE,
        .process_on_enter = TRUE
    },
    [LSM_TAB_APP_HISTORY] = {
        .build = lsm_history_build,
        .refresh = lsm_history_refresh,
        .enter = lsm_history_refresh,
        .search = history_search,
        .enter_only_after_initial_build = TRUE
    },
    [LSM_TAB_STARTUP] = {
        .build = lsm_startup_build,
        .refresh = lsm_startup_refresh,
        .enter = lsm_startup_refresh,
        .search = startup_search
    },
    [LSM_TAB_USERS] = {
        .build = lsm_users_build,
        .refresh = lsm_users_refresh,
        .enter = lsm_users_refresh,
        .periodic_update = lsm_users_update,
        .periodic_interval = LSM_USER_UPDATE_INTERVAL_SECONDS,
        .periodic_whole_seconds = TRUE
    },
    [LSM_TAB_DETAILS] = {
        .build = lsm_details_build,
        .enter = lsm_details_present_snapshot,
        .search = details_search,
        .process_foreground = TRUE,
        .process_on_enter = TRUE
    },
    [LSM_TAB_SERVICES] = {
        .build = lsm_services_build,
        .refresh = lsm_services_refresh,
        .enter = lsm_services_refresh,
        .search = services_search,
        .periodic_update = lsm_services_update,
        .periodic_interval = LSM_SERVICE_UPDATE_INTERVAL_SECONDS,
        .periodic_whole_seconds = TRUE
    },
    [LSM_TAB_FILESYSTEMS] = {
        .build = lsm_filesystems_build,
        .refresh = lsm_filesystems_refresh,
        .enter = lsm_filesystems_refresh,
        .search = filesystem_search,
        .periodic_update = lsm_filesystems_update,
        .filesystem_interval = TRUE
    },
    [LSM_TAB_OVERVIEW] = {
        .build = lsm_overview_build,
        .enter = lsm_overview_refresh,
        .process_on_enter = TRUE
    }
};

static const LsmPageDescriptor *descriptor_for_page(LsmTabIndex page)
{
    if (page < 0 || page >= LSM_TAB_COUNT) return NULL;
    const LsmPageDescriptor *descriptor = &page_descriptors[page];
    return descriptor->build ? descriptor : NULL;
}

bool lsm_app_page_registry_build(LsmApp *app, LsmTabIndex page,
                                 GtkWidget *container)
{
    const LsmPageDescriptor *descriptor = descriptor_for_page(page);
    if (!app || !container || !descriptor) return false;
    descriptor->build(app, container);
    return true;
}

void lsm_app_page_registry_refresh_all(LsmApp *app)
{
    if (!app) return;
    for (int page = 0; page < LSM_TAB_COUNT; page++) {
        const LsmPageDescriptor *descriptor =
            descriptor_for_page((LsmTabIndex)page);
        if (descriptor && descriptor->refresh)
            descriptor->refresh(app);
    }
}

void lsm_app_page_registry_enter(LsmApp *app, LsmTabIndex page,
                                 gboolean page_was_built)
{
    const LsmPageDescriptor *descriptor = descriptor_for_page(page);
    if (!app || !descriptor) return;

    if (descriptor->process_on_enter)
        (void)lsm_app_refresh_processes_if_due(app, FALSE);

    if (descriptor->enter &&
        (!descriptor->enter_only_after_initial_build || page_was_built))
        descriptor->enter(app);
}

GtkWidget *lsm_app_page_registry_search_widget(LsmApp *app, LsmTabIndex page)
{
    const LsmPageDescriptor *descriptor = descriptor_for_page(page);
    return app && descriptor && descriptor->search
        ? descriptor->search(app) : NULL;
}

bool lsm_app_page_registry_process_foreground(LsmTabIndex page)
{
    const LsmPageDescriptor *descriptor = descriptor_for_page(page);
    return descriptor && descriptor->process_foreground;
}

bool lsm_app_page_registry_active_periodic_policy(
    const LsmApp *app, guint *interval, gboolean *whole_seconds)
{
    if (interval) *interval = 0U;
    if (whole_seconds) *whole_seconds = FALSE;
    if (!app || !interval || !whole_seconds) return false;

    const LsmTabIndex page = (LsmTabIndex)app->runtime.active_tab;
    const LsmPageDescriptor *descriptor = descriptor_for_page(page);
    if (!descriptor || !descriptor->periodic_update ||
        !app->runtime.page_built[page])
        return false;

    if (descriptor->filesystem_interval) {
        *interval = app->runtime.filesystem_update_interval_ms;
        *whole_seconds = FALSE;
    } else {
        *interval = descriptor->periodic_interval;
        *whole_seconds = descriptor->periodic_whole_seconds;
    }
    return *interval > 0U;
}

gboolean lsm_app_page_registry_active_periodic_update(gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app || app->runtime.shutting_down) return G_SOURCE_REMOVE;

    const LsmTabIndex page = (LsmTabIndex)app->runtime.active_tab;
    const LsmPageDescriptor *descriptor = descriptor_for_page(page);
    if (!descriptor || !descriptor->periodic_update ||
        !app->runtime.page_built[page])
        return G_SOURCE_REMOVE;

    return descriptor->periodic_update(app);
}

static void restore_page_scroll(LsmApp *app, guint page)
{
    if (!app || page >= LSM_TAB_COUNT ||
        !app->runtime.page_scrollers[page])
        return;
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(app->runtime.page_scrollers[page]));
    if (adjustment)
        gtk_adjustment_set_value(adjustment, app->runtime.page_scroll[page]);
}

static void sync_overview_chrome(LsmApp *app)
{
    if (!app || !app->shell.window) return;
    const gboolean integrated =
        !app->runtime.compact_summary &&
        app->runtime.active_tab == LSM_TAB_OVERVIEW;
    GtkWidget *menu_bar = g_object_get_data(
        G_OBJECT(app->shell.window), "lsm-main-menu-bar");
    if (menu_bar) gtk_widget_set_visible(menu_bar, !integrated);
    if (app->shell.summary_bar)
        gtk_widget_set_visible(
            app->shell.summary_bar, app->runtime.compact_summary);
}

static void on_registry_tab_switched(GtkNotebook *notebook, GtkWidget *page,
                                     guint page_number, gpointer user_data)
{
    (void)notebook;
    (void)page;
    LsmApp *app = user_data;
    if (!app || !app->runtime.shell_shown || page_number >= LSM_TAB_COUNT)
        return;

    lsm_app_shell_save_page_scroll(app, app->runtime.active_tab);
    app->runtime.active_tab = (gint)page_number;
    app->runtime.last_tab = (gint)page_number;
    lsm_app_runtime_navigation_changed(app);

    const gboolean page_was_built = app->runtime.page_built[page_number];
    lsm_app_ensure_page_built(app, (LsmTabIndex)page_number);
    lsm_app_page_registry_enter(
        app, (LsmTabIndex)page_number, page_was_built);

    restore_page_scroll(app, page_number);
    sync_overview_chrome(app);
    lsm_app_shell_sync_navigation(app);
}

void lsm_app_page_registry_connect_notebook(LsmApp *app)
{
    if (!app || !app->shell.notebook) return;
    g_signal_connect(app->shell.notebook, "switch-page",
                     G_CALLBACK(on_registry_tab_switched), app);
}

static gboolean focus_allows_pause(const LsmApp *app, GtkWidget *focus)
{
    return !focus || focus == app->shell.notebook ||
           focus == app->processes.processes_tree ||
           focus == app->details.details_tree ||
           focus == app->performance.performance_stack;
}

static gboolean registry_key_press(GtkWidget *widget, GdkEventKey *event,
                                   gpointer user_data)
{
    (void)widget;
    LsmApp *app = user_data;
    if (!app) return FALSE;

    const gboolean control = (event->state & GDK_CONTROL_MASK) != 0;
    const gboolean shift = (event->state & GDK_SHIFT_MASK) != 0;
    const gboolean alt = (event->state & GDK_MOD1_MASK) != 0;

    if (event->keyval == GDK_KEY_F5) {
        lsm_app_menu_refresh(NULL, app);
        return TRUE;
    }
    if (control && (event->keyval == GDK_KEY_f ||
                    event->keyval == GDK_KEY_F)) {
        const gint current = gtk_notebook_get_current_page(
            GTK_NOTEBOOK(app->shell.notebook));
        GtkWidget *search = current >= 0 && current < LSM_TAB_COUNT
            ? lsm_app_page_registry_search_widget(
                  app, (LsmTabIndex)current)
            : NULL;
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
        GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(app->shell.window));
        if ((current == LSM_TAB_PROCESSES &&
             focus == app->processes.processes_tree) ||
            (current == LSM_TAB_DETAILS &&
             focus == app->details.details_tree)) {
            lsm_process_export_copy_selected(app);
            return TRUE;
        }
    }
    if (alt && event->keyval >= GDK_KEY_1 && event->keyval <= GDK_KEY_9) {
        const gint page_index = (gint)(event->keyval - GDK_KEY_1);
        if (page_index < LSM_TAB_COUNT) {
            gtk_notebook_set_current_page(
                GTK_NOTEBOOK(app->shell.notebook), page_index);
            return TRUE;
        }
    }

    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(app->shell.window));
    if (event->keyval == GDK_KEY_space && focus_allows_pause(app, focus)) {
        if (app->shell.pause_menu_item)
            gtk_check_menu_item_set_active(
                GTK_CHECK_MENU_ITEM(app->shell.pause_menu_item),
                !app->runtime.paused);
        return TRUE;
    }

    const gint current = gtk_notebook_get_current_page(
        GTK_NOTEBOOK(app->shell.notebook));
    if ((current == LSM_TAB_PROCESSES &&
         focus == app->processes.processes_tree) ||
        (current == LSM_TAB_DETAILS &&
         focus == app->details.details_tree)) {
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

void lsm_app_page_registry_connect_window(LsmApp *app)
{
    if (!app || !app->shell.window) return;

    /* Preserve shell-owned close/geometry/window-state hooks, then replace
     * only the key handler whose page knowledge belongs in this registry. */
    lsm_app_shell_connect_window(app);
    const guint key_signal = g_signal_lookup(
        "key-press-event", GTK_TYPE_WIDGET);
    if (key_signal != 0U)
        g_signal_handlers_disconnect_matched(
            app->shell.window,
            G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
            key_signal, 0U, NULL, NULL, app);
    g_signal_connect(app->shell.window, "key-press-event",
                     G_CALLBACK(registry_key_press), app);
}
