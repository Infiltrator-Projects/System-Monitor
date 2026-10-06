// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_navigation.c
 * @brief Narrow GTK bridge between Processes and Details.
 */
#include "process_navigation.h"

#include "app_internal.h"
#include "details_page.h"

void lsm_processes_go_to_details(LsmApp *app)
{
    if (!app || app->process.selected_pid <= 0 || !app->shell.notebook)
        return;

    lsm_app_ensure_page_built(app, LSM_TAB_DETAILS);
    if (!app->details.details_search) return;

    gtk_entry_set_text(GTK_ENTRY(app->details.details_search), "");
    app->details.details_structure_valid = FALSE;
    app->details.details_model_dirty = TRUE;
    gtk_notebook_set_current_page(
        GTK_NOTEBOOK(app->shell.notebook), LSM_TAB_DETAILS);
    lsm_details_present_snapshot(app);
}
