// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_ui_bridge.c
 * @brief Private adapter for process UI state shared across presentations.
 */
#include "process_ui_bridge.h"

#include "app_internal.h"

gboolean lsm_process_heatmap_enabled(const LsmApp *app)
{
    return app && app->details.process_heatmap;
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
