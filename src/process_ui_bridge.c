// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_ui_bridge.c
 * @brief Private adapter for process UI state shared across presentations.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_ui_bridge.h"

#include "app_presentation_context.h"
#include "app_process_context.h"
#include "processes_ui.h"

gboolean lsm_process_heatmap_enabled(const LsmApp *app)
{
    return lsm_app_process_heatmap_enabled(app);
}

void lsm_process_ui_snapshot_changed(LsmApp *app)
{
    lsm_app_process_ui_snapshot_changed(app);
}

void lsm_process_ui_application_catalog_changed(LsmApp *app)
{
    if (lsm_app_process_ui_catalog_changed(app))
        lsm_processes_present_snapshot(app);
}

void lsm_process_record_action_sync(LsmApp *app,
                                    gboolean ordinary_selection,
                                    gboolean grouped_selection)
{
    const gboolean sensitive = lsm_app_process_record_action_sensitive(
        app, ordinary_selection, grouped_selection);
    lsm_app_process_record_action_set_sensitive(app, sensitive);
}
