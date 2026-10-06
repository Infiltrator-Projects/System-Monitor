// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_runtime.c
 * @brief GTK-main-loop refresh cadence and timer ownership.
 *
 * This module owns when application refresh callbacks run. Feature-specific
 * page construction and refresh hooks are resolved through app_page_registry
 * so runtime cadence does not depend on individual slow-page modules.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_runtime.h"
#include "app_internal.h"
#include "app_page_registry.h"

#include "common.h"
#include "performance.h"
#include "processes_ui.h"
#include "refresh_policy.h"

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
    if (process_pages_active(app) || app->process.recorder)
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

static void remove_source(guint *source);

static void reschedule_process_timer(LsmApp *app)
{
    if (!app || !app->shell.window || app->runtime.shutting_down)
        return;
    if (app->runtime.process_timer)
        g_source_remove(app->runtime.process_timer);
    app->runtime.process_timer = g_timeout_add(
        effective_process_refresh_interval(app), process_timer_update, app);
}

static void reschedule_periodic_page_timer(LsmApp *app)
{
    if (!app) return;
    remove_source(&app->runtime.periodic_page_timer);
    if (!app->shell.window || app->runtime.shutting_down)
        return;

    guint interval = 0U;
    gboolean whole_seconds = FALSE;
    if (!lsm_app_page_registry_active_periodic_policy(
            app, &interval, &whole_seconds))
        return;

    app->runtime.periodic_page_timer = whole_seconds
        ? g_timeout_add_seconds(
              interval, lsm_app_page_registry_active_periodic_update, app)
        : g_timeout_add(
              interval, lsm_app_page_registry_active_periodic_update, app);
}

void lsm_app_refresh_all(LsmApp *app)
{
    if (!app) return;
    const gboolean was_paused = app->runtime.paused;
    app->runtime.paused = FALSE;
    (void)lsm_app_refresh_processes_if_due(app, TRUE);
    lsm_app_page_registry_refresh_all(app);
    app->runtime.paused = was_paused;
}

void lsm_app_preferences_changed(LsmApp *app)
{
    if (!app || !app->shell.window ||
        (!app->runtime.performance_timer && !app->runtime.process_timer))
        return;
    if (app->runtime.performance_timer)
        g_source_remove(app->runtime.performance_timer);
    if (app->runtime.process_timer) {
        g_source_remove(app->runtime.process_timer);
        app->runtime.process_timer = 0U;
    }
    app->runtime.performance_timer = g_timeout_add(
        app->runtime.update_interval_ms, lsm_performance_update, app);
    reschedule_process_timer(app);
    reschedule_periodic_page_timer(app);
}

void lsm_app_runtime_navigation_changed(LsmApp *app)
{
    if (!app) return;
    if (app->runtime.process_timer)
        reschedule_process_timer(app);
    reschedule_periodic_page_timer(app);
}

void lsm_app_runtime_page_built(LsmApp *app, unsigned page)
{
    if (!app || app->runtime.shutting_down || page >= LSM_TAB_COUNT)
        return;
    if ((unsigned)app->runtime.active_tab == page)
        reschedule_periodic_page_timer(app);
}

void lsm_app_runtime_start(LsmApp *app)
{
    if (!app) return;
    app->runtime.performance_timer = g_timeout_add(
        app->runtime.update_interval_ms, lsm_performance_update, app);
    reschedule_process_timer(app);
    reschedule_periodic_page_timer(app);
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
    remove_source(&app->runtime.periodic_page_timer);
}
