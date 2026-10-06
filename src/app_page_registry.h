// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_page_registry.h
 * @brief Central GTK page construction, activation, refresh and cadence registry.
 *
 * The registry is the single composition point that maps top-level tab
 * identities to builders, activation behavior, search widgets, manual refresh
 * hooks and cadence classes. Shell/runtime code therefore stays generic.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_PAGE_REGISTRY_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_PAGE_REGISTRY_H

#include "app.h"
#include "app_config.h"

#include <stdbool.h>

bool lsm_app_page_registry_build(LsmApp *app, LsmTabIndex page,
                                 GtkWidget *container);

void lsm_app_page_registry_refresh_all(LsmApp *app);

/** Dispatch the one-time/revisit activation policy for a top-level page. */
void lsm_app_page_registry_enter(LsmApp *app, LsmTabIndex page,
                                 gboolean page_was_built);

/** Return the active page's search control, or NULL when it has none. */
GtkWidget *lsm_app_page_registry_search_widget(LsmApp *app, LsmTabIndex page);

bool lsm_app_page_registry_process_foreground(LsmTabIndex page);

bool lsm_app_page_registry_active_periodic_policy(
    const LsmApp *app, guint *interval, gboolean *whole_seconds);

gboolean lsm_app_page_registry_active_periodic_update(gpointer user_data);

#endif
