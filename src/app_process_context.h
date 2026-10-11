// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_process_context.h
 * @brief Narrow application-state bridge for the shared process domain.
 *
 * Process coordination does not need the complete private LsmApp layout. This
 * contract exposes only the runtime facts, retained process data and ownership
 * transitions required by sampling, lifecycle and export code.
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

typedef struct LsmApplicationCatalog LsmApplicationCatalog;
typedef struct LsmProcessScanner LsmProcessScanner;

/**
 * @brief Process-domain runtime facts copied from private application state.
 *
 * This view deliberately contains no GTK widget pointers or feature-owned
 * mutable storage, keeping process coordinators independent of LsmApp layout.
 */
typedef struct {
    LsmTabIndex active_tab; /**< Active top-level page. */
    gboolean paused; /**< TRUE while process updates are paused. */
    gboolean shutting_down; /**< TRUE once application teardown begins. */
    gboolean processes_built; /**< TRUE once the grouped Processes page exists. */
    gboolean details_built; /**< TRUE once the technical Details page exists. */
    gboolean application_catalog_loading; /**< TRUE while discovery is active. */
    gboolean application_catalog_ready; /**< TRUE once discovery has published. */
} LsmProcessRuntimeView;

/** Completed process data and immutable selection state used by exporters. */
typedef struct {
    const LsmProcessInfo *processes;
    size_t process_count;
    LsmProcessId selected_pid;
    LsmProcessInstanceId selected_instance_id;
    const LsmProcessId *selected_group_pids;
    const LsmProcessInstanceId *selected_group_instance_ids;
    size_t selected_group_count;
} LsmProcessSelectionView;

/**
 * Copy the process domain's permitted runtime state out of LsmApp.
 * @param app Application owning process state.
 * @param view Destination for the narrow runtime view.
 * @return true when both arguments are valid; otherwise false.
 */
bool lsm_app_process_runtime_view(const LsmApp *app,
                                  LsmProcessRuntimeView *view);

/**
 * Copy the retained process snapshot and selection into a read-only view.
 * @param app Application owning the process snapshot and selection.
 * @param view Destination receiving stable pointers owned by @p app.
 * @return true when both arguments are valid; otherwise false.
 */
bool lsm_app_process_selection_view(const LsmApp *app,
                                    LsmProcessSelectionView *view);

/**
 * Return the application's process scanner without exposing LsmApp layout.
 * @param app Application owning the scanner.
 * @return Scanner pointer, or NULL when app is NULL.
 */
LsmProcessScanner *lsm_app_process_scanner(LsmApp *app);

/**
 * Replace the application-owned process scanner pointer.
 * @param app Application owning process-domain storage.
 * @param scanner New scanner pointer, or NULL during teardown.
 */
void lsm_app_process_set_scanner(LsmApp *app, LsmProcessScanner *scanner);

/**
 * Update asynchronous application-catalog loading state.
 * @param app Application owning process runtime state.
 * @param loading TRUE while catalogue discovery is active.
 */
void lsm_app_process_set_catalog_loading(LsmApp *app, gboolean loading);

/**
 * Replace the retained application catalogue.
 * @param app Application owning the catalogue pointer.
 * @param catalog New catalogue, or NULL during teardown.
 * @return Previous catalogue pointer; ownership transfers to the caller.
 */
LsmApplicationCatalog *lsm_app_process_replace_catalog(
    LsmApp *app, LsmApplicationCatalog *catalog);

/**
 * Transfer the retained process snapshot out of application ownership.
 * @param app Application owning the retained process snapshot.
 * @param count Optional destination receiving the transferred row count.
 * @return Previously retained process array, or NULL when unavailable.
 */
LsmProcessInfo *lsm_app_process_take_snapshot(LsmApp *app, size_t *count);

/**
 * Transfer persisted process filters out of application ownership.
 * @param app Application owning the filter array.
 * @return Previously retained filter array, or NULL when unavailable.
 */
GPtrArray *lsm_app_process_take_filters(LsmApp *app);

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
 *
 * This operation updates process-domain storage and monitor totals only. UI
 * invalidation is an explicit presentation concern handled by process_ui_bridge.
 *
 * @param app Application receiving the snapshot.
 * @param processes Owned process array transferred to the application.
 * @param count Number of valid entries in processes.
 */
void lsm_app_process_publish_snapshot(LsmApp *app,
                                      LsmProcessInfo *processes,
                                      size_t count);

#endif
