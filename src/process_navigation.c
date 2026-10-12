// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_navigation.c
 * @brief Narrow GTK bridge between Processes and Details.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_navigation.h"

#include "app_presentation_context.h"
#include "app_process_context.h"
#include "details_page.h"

void lsm_processes_go_to_details(LsmApp *app)
{
    if (!app) return;

    LsmProcessSelectionView selection;
    if (!lsm_app_process_selection_view(app, &selection) ||
        selection.selected_pid == 0U)
        return;

    GtkWidget *notebook = lsm_app_page_registry_notebook(app);
    if (!notebook) return;

    lsm_app_page_registry_ensure_built(app, LSM_TAB_DETAILS);
    GtkWidget *search = lsm_app_page_registry_search_target(
        app, LSM_TAB_DETAILS);
    if (!search) return;

    gtk_entry_set_text(GTK_ENTRY(search), "");
    lsm_app_details_invalidate(app);
    gtk_notebook_set_current_page(
        GTK_NOTEBOOK(notebook), LSM_TAB_DETAILS);
    lsm_details_present_snapshot(app);
}
