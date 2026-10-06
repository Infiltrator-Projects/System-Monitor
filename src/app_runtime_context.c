// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_runtime_context.c
 * @brief Private adapter between LsmApp storage and runtime cadence policy.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_runtime_context.h"
#include "app_internal.h"

static guint *timer_slot(LsmApp *app, LsmRuntimeTimer timer)
{
    if (!app) return NULL;
    switch (timer) {
        case LSM_RUNTIME_TIMER_PERFORMANCE:
            return &app->runtime.performance_timer;
        case LSM_RUNTIME_TIMER_PROCESS:
            return &app->runtime.process_timer;
        case LSM_RUNTIME_TIMER_PERIODIC_PAGE:
            return &app->runtime.periodic_page_timer;
    }
    return NULL;
}

bool lsm_app_runtime_control_view(const LsmApp *app,
                                  LsmRuntimeControlView *view)
{
    if (!app || !view) return false;
    *view = (LsmRuntimeControlView) {
        .update_interval_ms = app->runtime.update_interval_ms,
        .performance_timer = app->runtime.performance_timer,
        .process_timer = app->runtime.process_timer,
        .periodic_page_timer = app->runtime.periodic_page_timer,
        .last_process_refresh_monotonic =
            app->runtime.last_process_refresh_monotonic,
        .active_tab = (LsmTabIndex)app->runtime.active_tab,
        .paused = app->runtime.paused,
        .shutting_down = app->runtime.shutting_down,
        .shell_ready = app->shell.window != NULL,
        .process_recording = app->process.recorder != NULL
    };
    return true;
}

guint lsm_app_runtime_take_timer(LsmApp *app, LsmRuntimeTimer timer)
{
    guint *slot = timer_slot(app, timer);
    if (!slot) return 0U;
    const guint source = *slot;
    *slot = 0U;
    return source;
}

void lsm_app_runtime_set_timer(LsmApp *app, LsmRuntimeTimer timer,
                               guint source)
{
    guint *slot = timer_slot(app, timer);
    if (slot) *slot = source;
}

void lsm_app_runtime_set_process_refresh_time(LsmApp *app, double monotonic)
{
    if (app) app->runtime.last_process_refresh_monotonic = monotonic;
}

void lsm_app_runtime_set_paused(LsmApp *app, gboolean paused)
{
    if (app) app->runtime.paused = paused;
}
