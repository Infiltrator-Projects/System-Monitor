// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_page_registry.c
 * @brief Central GTK page construction, activation, refresh and cadence registry.
 *
 * Top-level page wiring belongs here so the shell and runtime do not maintain
 * parallel switches, timer policy or feature include sets. The descriptor
 * table is indexed by stable LsmTabIndex identity and uses designated
 * initialisers so adding fields cannot silently retarget existing policy.
 * Keyboard command routing is owned separately by app_keyboard, while private
 * application layout is reached only through app_presentation_context.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define LSM_APP_PAGE_REGISTRY_IMPLEMENTATION
#include "app_page_registry.h"
#undef LSM_APP_PAGE_REGISTRY_IMPLEMENTATION

#include "app_keyboard.h"
#include "app_presentation_context.h"
#include "app_runtime.h"
#include "app_shell.h"
#include "app_shell_window.h"
#include "details_page.h"
#include "filesystems.h"
#include "history.h"
#include "overview.h"
#include "performance.h"
#include "processes_ui.h"
#include "services.h"
#include "startup.h"
#include "users.h"

typedef void (*LsmPageBuildFunction)(LsmApp *app, GtkWidget *container);
typedef void (*LsmPageRefreshFunction)(LsmApp *app);

typedef struct {
    LsmPageBuildFunction build;
    LsmPageRefreshFunction refresh;
    LsmPageRefreshFunction enter;
    GSourceFunc periodic_update;
    guint periodic_interval;
    gboolean periodic_whole_seconds;
    gboolean filesystem_interval;
    gboolean process_foreground;
    gboolean process_on_enter;
    gboolean enter_only_after_initial_build;
} LsmPageDescriptor;

static const LsmPageDescriptor page_descriptors[LSM_TAB_COUNT] = {
    [LSM_TAB_PERFORMANCE] = {
        .build = lsm_performance_build,
        .refresh = lsm_performance_refresh,
        .enter = lsm_performance_refresh
    },
    [LSM_TAB_PROCESSES] = {
        .build = lsm_processes_build,
        .enter = lsm_processes_present_snapshot,
        .process_foreground = TRUE,
        .process_on_enter = TRUE
    },
    [LSM_TAB_APP_HISTORY] = {
        .build = lsm_history_build,
        .refresh = lsm_history_refresh,
        .enter = lsm_history_refresh,
        .enter_only_after_initial_build = TRUE
    },
    [LSM_TAB_STARTUP] = {
        .build = lsm_startup_build,
        .refresh = lsm_startup_refresh,
        .enter = lsm_startup_refresh
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
        .process_foreground = TRUE,
        .process_on_enter = TRUE
    },
    [LSM_TAB_SERVICES] = {
        .build = lsm_services_build,
        .refresh = lsm_services_refresh,
        .enter = lsm_services_refresh,
        .periodic_update = lsm_services_update,
        .periodic_interval = LSM_SERVICE_UPDATE_INTERVAL_SECONDS,
        .periodic_whole_seconds = TRUE
    },
    [LSM_TAB_FILESYSTEMS] = {
        .build = lsm_filesystems_build,
        .refresh = lsm_filesystems_refresh,
        .enter = lsm_filesystems_refresh,
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
    return lsm_app_page_registry_search_target(app, page);
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
    LsmPageRegistryControlView view;
    if (!interval || !whole_seconds ||
        !lsm_app_page_registry_control_view(app, &view))
        return false;

    const LsmTabIndex page = view.active_tab;
    const LsmPageDescriptor *descriptor = descriptor_for_page(page);
    if (!descriptor || !descriptor->periodic_update ||
        !lsm_app_page_registry_page_built(app, page))
        return false;

    if (descriptor->filesystem_interval) {
        *interval = view.filesystem_update_interval_ms;
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
    LsmPageRegistryControlView view;
    if (!lsm_app_page_registry_control_view(app, &view) ||
        view.shutting_down)
        return G_SOURCE_REMOVE;

    const LsmTabIndex page = view.active_tab;
    const LsmPageDescriptor *descriptor = descriptor_for_page(page);
    if (!descriptor || !descriptor->periodic_update ||
        !lsm_app_page_registry_page_built(app, page))
        return G_SOURCE_REMOVE;

    return descriptor->periodic_update(app);
}

static void on_registry_tab_switched(GtkNotebook *notebook, GtkWidget *page,
                                     guint page_number, gpointer user_data)
{
    (void)notebook;
    (void)page;
    LsmApp *app = user_data;
    LsmPageRegistryControlView view;
    if (!lsm_app_page_registry_control_view(app, &view) ||
        !view.shell_shown || page_number >= LSM_TAB_COUNT)
        return;

    lsm_app_shell_save_page_scroll(app, (gint)view.active_tab);
    const LsmTabIndex target = (LsmTabIndex)page_number;
    const gboolean page_was_built =
        lsm_app_page_registry_page_built(app, target);
    lsm_app_page_registry_set_active(app, target);
    lsm_app_runtime_navigation_changed(app);

    lsm_app_page_registry_ensure_built(app, target);
    lsm_app_page_registry_enter(app, target, page_was_built);

    lsm_app_page_registry_restore_scroll(app, target);
    lsm_app_page_registry_sync_overview_chrome(app);
    lsm_app_shell_sync_navigation(app);
}

void lsm_app_page_registry_connect_notebook(LsmApp *app)
{
    GtkWidget *notebook = lsm_app_page_registry_notebook(app);
    if (!notebook) return;
    g_signal_connect(notebook, "switch-page",
                     G_CALLBACK(on_registry_tab_switched), app);
}

void lsm_app_page_registry_connect_window(LsmApp *app)
{
    if (!lsm_app_page_registry_window(app)) return;

    /* Page-aware keyboard commands and window-manager mechanics have separate
     * owners; page registration contains neither command nor WM policy. */
    lsm_app_keyboard_connect(app);
    lsm_app_shell_window_connect(app);
}
