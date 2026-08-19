// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_runtime.c
 * @brief GTK-main-loop refresh cadence and timer ownership.
 *
 * This module owns when application refresh callbacks run. Feature modules
 * continue to own what they collect and present.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_runtime.h"
#include "app_internal.h"

#include "common.h"
#include "details_page.h"
#include "filesystems.h"
#include "history.h"
#include "performance.h"
#include "processes_ui.h"
#include "refresh_policy.h"
#include "services.h"
#include "startup.h"
#include "users.h"

static guint process_refresh_interval(const LsmApp *app)
{
    return app->runtime.update_interval_ms < 1000U
        ? 1000U : app->runtime.update_interval_ms;
}

static gboolean process_pages_active(const LsmApp *app)
{
    return app->runtime.active_tab == LSM_TAB_PROCESSES ||
           app->runtime.active_tab == LSM_TAB_DETAILS;
}

static guint effective_process_refresh_interval(const LsmApp *app)
{
    const guint foreground = process_refresh_interval(app);
    if (process_pages_active(app) || app->process.record_file)
        return foreground;
    return foreground < LSM_PROCESS_UPDATE_INTERVAL_MS
        ? LSM_PROCESS_UPDATE_INTERVAL_MS : foreground;
}

gboolean lsm_app_refresh_processes_if_due(LsmApp *app, gboolean force)
{
    if (!app || (app->runtime.paused && !force))
        return G_SOURCE_CONTINUE;

    const double now = lsm_monotonic_seconds();
    const double interval =
        (double)effective_process_refresh_interval(app) / 1000.0;
    if (!force && !lsm_refresh_interval_due(
                      now,
                      app->runtime.last_process_refresh_monotonic,
                      interval))
        return G_SOURCE_CONTINUE;

    const gboolean result = lsm_processes_update(app);
    if (now > 0.0)
        app->runtime.last_process_refresh_monotonic = now;
    return result;
}

static gboolean process_timer_update(gpointer user_data)
{
    return lsm_app_refresh_processes_if_due(user_data, FALSE);
}

void lsm_app_refresh_all(LsmApp *app)
{
    if (!app) return;
    const gboolean was_paused = app->runtime.paused;
    app->runtime.paused = FALSE;
    (void)lsm_app_refresh_processes_if_due(app, TRUE);
    lsm_performance_refresh(app);
    lsm_history_refresh(app);
    lsm_filesystems_refresh(app);
    lsm_startup_refresh(app);
    lsm_services_refresh(app);
    lsm_users_refresh(app);
    app->runtime.paused = was_paused;
}

void lsm_app_preferences_changed(LsmApp *app)
{
    if (!app || !app->shell.window ||
        (!app->runtime.performance_timer && !app->runtime.process_timer))
        return;
    if (app->runtime.performance_timer)
        g_source_remove(app->runtime.performance_timer);
    if (app->runtime.process_timer)
        g_source_remove(app->runtime.process_timer);
    app->runtime.performance_timer = g_timeout_add(
        app->runtime.update_interval_ms, lsm_performance_update, app);
    app->runtime.process_timer = g_timeout_add(
        process_refresh_interval(app), process_timer_update, app);
}

void lsm_app_runtime_start(LsmApp *app)
{
    if (!app) return;
    app->runtime.performance_timer = g_timeout_add(
        app->runtime.update_interval_ms, lsm_performance_update, app);
    app->runtime.process_timer = g_timeout_add(
        process_refresh_interval(app), process_timer_update, app);
    app->runtime.services_timer = g_timeout_add_seconds(
        LSM_SERVICE_UPDATE_INTERVAL_SECONDS, lsm_services_update, app);
    app->runtime.users_timer = g_timeout_add_seconds(
        LSM_USER_UPDATE_INTERVAL_SECONDS, lsm_users_update, app);
    app->runtime.filesystem_timer = g_timeout_add_seconds(
        LSM_FILESYSTEM_UPDATE_INTERVAL_SECONDS, lsm_filesystems_update, app);
}

static void remove_source(guint *source)
{
    if (!source || !*source) return;
    g_source_remove(*source);
    *source = 0U;
}

void lsm_app_runtime_stop(LsmApp *app)
{
    if (!app) return;
    remove_source(&app->runtime.performance_timer);
    remove_source(&app->runtime.process_timer);
    remove_source(&app->runtime.services_timer);
    remove_source(&app->runtime.users_timer);
    remove_source(&app->runtime.filesystem_timer);
}
