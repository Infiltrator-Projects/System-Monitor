// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_ui_bridge.h
 * @brief Narrow shared process-presentation state accessors.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_UI_BRIDGE_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_UI_BRIDGE_H

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Report whether the shared process resource heatmap is enabled.
 * @param app Application containing presentation preferences.
 * @return TRUE when the process heatmap is enabled; otherwise FALSE.
 */
gboolean lsm_process_heatmap_enabled(const LsmApp *app);

/**
 * Mark both process presentations dirty after a new completed snapshot.
 * @param app Application owning process presentation state.
 */
void lsm_process_ui_snapshot_changed(LsmApp *app);

/**
 * Invalidate grouped-process identity state after catalogue replacement.
 *
 * The visible Processes page is refreshed immediately when it is active;
 * otherwise invalidation is retained until its next presentation pass.
 *
 * @param app Application owning grouped process presentation state.
 */
void lsm_process_ui_application_catalog_changed(LsmApp *app);

/**
 * Synchronise the shared process-record action sensitivity.
 * @param app Application owning the record menu action.
 * @param ordinary_selection TRUE when one ordinary process is selected.
 * @param grouped_selection TRUE when a grouped selection is active.
 */
void lsm_process_record_action_sync(LsmApp *app,
                                    gboolean ordinary_selection,
                                    gboolean grouped_selection);

#endif
