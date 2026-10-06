// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_ui_bridge.h
 * @brief Narrow shared process-presentation state accessors.
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_UI_BRIDGE_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_UI_BRIDGE_H

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

gboolean lsm_process_heatmap_enabled(const LsmApp *app);
void lsm_process_record_action_sync(LsmApp *app,
                                    gboolean ordinary_selection,
                                    gboolean grouped_selection);

#endif
