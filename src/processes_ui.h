// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file processes_ui.h
 * @brief Friendly grouped Processes page.
 *
 * Presentation depends explicitly on process actions, navigation and the shared
 * snapshot workspace rather than receiving those APIs transitively from one
 * oversized process header.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESSES_UI_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESSES_UI_H

#include "process_actions.h"
#include "process_navigation.h"
#include "process_ui_bridge.h"
#include "process_workspace.h"

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

void lsm_processes_build(LsmApp *app, GtkWidget *container);
void lsm_processes_present_snapshot(LsmApp *app);
gboolean lsm_processes_page_visible(const LsmApp *app);
void lsm_processes_destroy(LsmApp *app);

#endif
