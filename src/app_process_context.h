// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_process_context.h
 * @brief Narrow application-state bridge for the shared process workspace.
 *
 * Process sampling does not need the complete private LsmApp layout. This
 * contract exposes only the runtime facts and ownership transitions required
 * to collect and publish process snapshots.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_PROCESS_CONTEXT_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_PROCESS_CONTEXT_H

#include "app.h"
#include "app_config.h"
#include "monitor_types.h"

#include <glib.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct LsmProcessScanner LsmProcessScanner;

typedef struct {
    LsmTabIndex active_tab;
    gboolean paused;
    gboolean shutting_down;
    gboolean processes_built;
    gboolean details_built;
} LsmProcessRuntimeView;

/**
 * Copy the process workspace's permitted runtime state out of LsmApp.
 * @param app Application owning process state.
 * @param view Destination for the narrow runtime view.
 * @return true when both arguments are valid; otherwise false.
 */
bool lsm_app_process_runtime_view(const LsmApp *app,
                                  LsmProcessRuntimeView *view);

/**
 * Return the application's process scanner without exposing LsmApp layout.
 * @param app Application owning the scanner.
 * @return Scanner pointer, or NULL when app is NULL.
 */
LsmProcessScanner *lsm_app_process_scanner(LsmApp *app);

/**
 * Resolve the active recording identity for the process workspace.
 * @param app Application owning recording state.
 * @param pid Destination for the recorded PID.
 * @param instance_id Destination for the recorded process instance identity.
 * @return true when recording is active and both outputs are populated.
 */
bool lsm_app_process_recording_target(const LsmApp *app,
                                      LsmProcessId *pid,
                                      LsmProcessInstanceId *instance_id);

/**
 * Publish one completed process snapshot into application-owned state.
 * @param app Application receiving the snapshot.
 * @param processes Owned process array transferred to the application.
 * @param count Number of valid entries in processes.
 */
void lsm_app_process_publish_snapshot(LsmApp *app,
                                      LsmProcessInfo *processes,
                                      size_t count);

#endif
