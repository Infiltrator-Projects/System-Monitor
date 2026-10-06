// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_runtime_context.h
 * @brief Narrow application-state bridge for main-loop cadence ownership.
 *
 * Runtime scheduling needs only timer identifiers, cadence preferences and a
 * handful of lifecycle facts. Keeping that view separate prevents the timer
 * coordinator from depending on the complete private LsmApp layout.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_RUNTIME_CONTEXT_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_RUNTIME_CONTEXT_H

#include "app.h"
#include "app_config.h"

#include <glib.h>
#include <stdbool.h>

typedef enum {
    LSM_RUNTIME_TIMER_PERFORMANCE,
    LSM_RUNTIME_TIMER_PROCESS,
    LSM_RUNTIME_TIMER_PERIODIC_PAGE
} LsmRuntimeTimer;

/** Runtime facts copied out of private application state for cadence policy. */
typedef struct {
    guint update_interval_ms;
    guint performance_timer;
    guint process_timer;
    guint periodic_page_timer;
    double last_process_refresh_monotonic;
    LsmTabIndex active_tab;
    gboolean paused;
    gboolean shutting_down;
    gboolean shell_ready;
    gboolean process_recording;
} LsmRuntimeControlView;

/**
 * Copy the timer coordinator's permitted view of application state.
 * @param [in] app Application owning runtime state.
 * @param [out] view Destination receiving the narrow runtime snapshot.
 * @return true when both arguments are valid; otherwise false.
 */
bool lsm_app_runtime_control_view(const LsmApp *app,
                                  LsmRuntimeControlView *view);

/**
 * Remove one timer identifier from application ownership and return it.
 * @param [in,out] app Application owning timer state.
 * @param [in] timer Timer slot to clear.
 * @return Previous GLib source identifier, or zero when unavailable.
 */
guint lsm_app_runtime_take_timer(LsmApp *app, LsmRuntimeTimer timer);

/**
 * Store one timer identifier in application-owned runtime state.
 * @param [in,out] app Application owning timer state.
 * @param [in] timer Timer slot to update.
 * @param [in] source GLib source identifier to retain.
 */
void lsm_app_runtime_set_timer(LsmApp *app, LsmRuntimeTimer timer,
                               guint source);

/**
 * Update the process cadence baseline without exposing LsmRuntimeState.
 * @param [in,out] app Application owning process cadence state.
 * @param [in] monotonic Most recent successful process refresh time.
 */
void lsm_app_runtime_set_process_refresh_time(LsmApp *app, double monotonic);

/**
 * Update pause state without exposing LsmRuntimeState.
 * @param [in,out] app Application owning pause state.
 * @param [in] paused TRUE to pause periodic presentation updates.
 */
void lsm_app_runtime_set_paused(LsmApp *app, gboolean paused);

#endif
