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

#include "app_internal.h"
#include "processes_ui.h"

gboolean lsm_process_heatmap_enabled(const LsmApp *app)
{
    return app && app->details.process_heatmap;
}

void lsm_process_ui_snapshot_changed(LsmApp *app)
{
    if (!app) return;
    app->processes.processes_model_dirty = TRUE;
    app->details.details_model_dirty = TRUE;
}

void lsm_process_ui_application_catalog_changed(LsmApp *app)
{
    if (!app) return;
    if (app->processes.process_identity_cache)
        g_hash_table_remove_all(app->processes.process_identity_cache);
    app->processes.processes_structure_valid = FALSE;
    app->processes.processes_model_dirty = TRUE;
    if (app->runtime.active_tab == LSM_TAB_PROCESSES &&
        app->runtime.page_built[LSM_TAB_PROCESSES])
        lsm_processes_present_snapshot(app);
}

void lsm_process_record_action_sync(LsmApp *app,
                                    gboolean ordinary_selection,
                                    gboolean grouped_selection)
{
    if (!app || !app->details.process_record_menu_item) return;
    const gboolean sensitive =
        (ordinary_selection && !grouped_selection) ||
        app->process.recorder != NULL;
    gtk_widget_set_sensitive(app->details.process_record_menu_item, sensitive);
}
