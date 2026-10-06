// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_runtime.c
 * @brief GTK-main-loop refresh cadence and timer ownership.
 *
 * This module owns when application refresh callbacks run. Feature-specific
 * page construction, cadence classification and refresh hooks are resolved
 * through app_page_registry so runtime cadence does not depend on page modules.
 * Private application storage is reached only through app_runtime_context.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_runtime.h"
#include "app_page_registry.h"
#include "app_runtime_context.h"

#include "common.h"
#include "performance.h"
#include "process_workspace.h"
#include "refresh_policy.h"

static guint process_refresh_interval(const LsmRuntimeControlView *view)
{
    return view->update_interval_ms < 1000U
        ? 1000U : view->update_interval_ms;
}

static guint effective_process_refresh_interval(
    const LsmRuntimeControlView *view)
{
    const guint foreground = process_refresh_interval(view);
    if (lsm_app_page_registry_process_foreground(view->active_tab) ||
        view->process_recording)
        return foreground;
    return foreground < LSM_PROCESS_UPDATE_INTERVAL_MS
        ? LSM_PROCESS_UPDATE_INTERVAL_MS : foreground;
}

gboolean lsm_app_refresh_processes_if_due(LsmApp *app, gboolean force)
{
    LsmRuntimeControlView view;
    if (!lsm_app_runtime_control_view(app, &view) ||
        (view.paused && !force))
        return G_SOURCE_CONTINUE;

    const double now = lsm_monotonic_seconds();
    const double interval =
        (double)effective_process_refresh_interval(&view) / 1000.0;
    if (!force && !lsm_refresh_interval_due(
                      now,
                      view.last_process_refresh_monotonic,
                      interval))
        return G_SOURCE_CONTINUE;

    const gboolean result = lsm_processes_update(app);
    if (now > 0.0)
        lsm_app_runtime_set_process_refresh_time(app, now);
    return result;
}

static gboolean process_timer_update(gpointer user_data)
{
    return lsm_app_refresh_processes_if_due(user_data, FALSE);
}

static void remove_timer(LsmApp *app, LsmRuntimeTimer timer)
{
    const guint source = lsm_app_runtime_take_timer(app, timer);
    if (source) g_source_remove(source);
}

static void reschedule_process_timer(LsmApp *app)
{
    LsmRuntimeControlView view;
    if (!lsm_app_runtime_control_view(app, &view) ||
        !view.shell_ready || view.shutting_down)
        return;

    remove_timer(app, LSM_RUNTIME_TIMER_PROCESS);
    const guint source = g_timeout_add(
        effective_process_refresh_interval(&view), process_timer_update, app);
    lsm_app_runtime_set_timer(app, LSM_RUNTIME_TIMER_PROCESS, source);
}

static void reschedule_periodic_page_timer(LsmApp *app)
{
    if (!app) return;
    remove_timer(app, LSM_RUNTIME_TIMER_PERIODIC_PAGE);

    LsmRuntimeControlView view;
    if (!lsm_app_runtime_control_view(app, &view) ||
        !view.shell_ready || view.shutting_down)
        return;

    guint interval = 0U;
    gboolean whole_seconds = FALSE;
    if (!lsm_app_page_registry_active_periodic_policy(
            app, &interval, &whole_seconds))
        return;

    const guint source = whole_seconds
        ? g_timeout_add_seconds(
              interval, lsm_app_page_registry_active_periodic_update, app)
        : g_timeout_add(
              interval, lsm_app_page_registry_active_periodic_update, app);
    lsm_app_runtime_set_timer(app, LSM_RUNTIME_TIMER_PERIODIC_PAGE, source);
}

void lsm_app_refresh_all(LsmApp *app)
{
    LsmRuntimeControlView view;
    if (!lsm_app_runtime_control_view(app, &view)) return;
    lsm_app_runtime_set_paused(app, FALSE);
    (void)lsm_app_refresh_processes_if_due(app, TRUE);
    lsm_app_page_registry_refresh_all(app);
    lsm_app_runtime_set_paused(app, view.paused);
}

void lsm_app_preferences_changed(LsmApp *app)
{
    LsmRuntimeControlView view;
    if (!lsm_app_runtime_control_view(app, &view) || !view.shell_ready ||
        (!view.performance_timer && !view.process_timer))
        return;

    remove_timer(app, LSM_RUNTIME_TIMER_PERFORMANCE);
    remove_timer(app, LSM_RUNTIME_TIMER_PROCESS);
    const guint performance_source = g_timeout_add(
        view.update_interval_ms, lsm_performance_update, app);
    lsm_app_runtime_set_timer(
        app, LSM_RUNTIME_TIMER_PERFORMANCE, performance_source);
    reschedule_process_timer(app);
    reschedule_periodic_page_timer(app);
}

void lsm_app_runtime_navigation_changed(LsmApp *app)
{
    LsmRuntimeControlView view;
    if (!lsm_app_runtime_control_view(app, &view)) return;
    if (view.process_timer)
        reschedule_process_timer(app);
    reschedule_periodic_page_timer(app);
}

void lsm_app_runtime_page_built(LsmApp *app, unsigned page)
{
    LsmRuntimeControlView view;
    if (!lsm_app_runtime_control_view(app, &view) ||
        view.shutting_down || page >= LSM_TAB_COUNT)
        return;
    if ((unsigned)view.active_tab == page)
        reschedule_periodic_page_timer(app);
}

void lsm_app_runtime_start(LsmApp *app)
{
    LsmRuntimeControlView view;
    if (!lsm_app_runtime_control_view(app, &view)) return;
    const guint performance_source = g_timeout_add(
        view.update_interval_ms, lsm_performance_update, app);
    lsm_app_runtime_set_timer(
        app, LSM_RUNTIME_TIMER_PERFORMANCE, performance_source);
    reschedule_process_timer(app);
    reschedule_periodic_page_timer(app);
}

void lsm_app_runtime_stop(LsmApp *app)
{
    if (!app) return;
    remove_timer(app, LSM_RUNTIME_TIMER_PERFORMANCE);
    remove_timer(app, LSM_RUNTIME_TIMER_PROCESS);
    remove_timer(app, LSM_RUNTIME_TIMER_PERIODIC_PAGE);
}
