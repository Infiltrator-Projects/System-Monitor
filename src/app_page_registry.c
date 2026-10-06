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
#include "app_page_registry.h"
#include "app_internal.h"
#include "app_runtime.h"

#include "details_page.h"
#include "filesystems.h"
#include "history.h"
#include "overview.h"
#include "performance.h"
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
