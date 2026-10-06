// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_page_registry.c
 * @brief Central GTK page construction and refresh ownership registry.
 *
 * Top-level page wiring belongs here so app.c and app_runtime.c do not each
 * maintain their own parallel switches, timer slots and feature include sets.
 * The descriptor table is indexed by stable LsmTabIndex identity and uses
 * designated initialisers so enum order changes cannot silently retarget a
 * callback to another page.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_page_registry.h"
#include "app_internal.h"

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
    GSourceFunc periodic_update;
    guint periodic_interval;
    gboolean periodic_whole_seconds;
    gboolean filesystem_interval;
} LsmPageDescriptor;

static const LsmPageDescriptor page_descriptors[LSM_TAB_COUNT] = {
    [LSM_TAB_PERFORMANCE] = {
        lsm_performance_build, lsm_performance_refresh, NULL, 0U, FALSE, FALSE
    },
    [LSM_TAB_PROCESSES] = {
        lsm_processes_build, NULL, NULL, 0U, FALSE, FALSE
    },
    [LSM_TAB_APP_HISTORY] = {
        lsm_history_build, lsm_history_refresh, NULL, 0U, FALSE, FALSE
    },
    [LSM_TAB_STARTUP] = {
        lsm_startup_build, lsm_startup_refresh, NULL, 0U, FALSE, FALSE
    },
    [LSM_TAB_USERS] = {
        lsm_users_build, lsm_users_refresh, lsm_users_update,
        LSM_USER_UPDATE_INTERVAL_SECONDS, TRUE, FALSE
    },
    [LSM_TAB_DETAILS] = {
        lsm_details_build, NULL, NULL, 0U, FALSE, FALSE
    },
    [LSM_TAB_SERVICES] = {
        lsm_services_build, lsm_services_refresh, lsm_services_update,
        LSM_SERVICE_UPDATE_INTERVAL_SECONDS, TRUE, FALSE
    },
    [LSM_TAB_FILESYSTEMS] = {
        lsm_filesystems_build, lsm_filesystems_refresh, lsm_filesystems_update,
        0U, FALSE, TRUE
    },
    [LSM_TAB_OVERVIEW] = {
        lsm_overview_build, NULL, NULL, 0U, FALSE, FALSE
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
