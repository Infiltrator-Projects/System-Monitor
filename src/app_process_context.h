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

bool lsm_app_process_runtime_view(const LsmApp *app,
                                  LsmProcessRuntimeView *view);
LsmProcessScanner *lsm_app_process_scanner(LsmApp *app);
bool lsm_app_process_recording_target(const LsmApp *app,
                                      LsmProcessId *pid,
                                      LsmProcessInstanceId *instance_id);
void lsm_app_process_publish_snapshot(LsmApp *app,
                                      LsmProcessInfo *processes,
                                      size_t count);

#endif
