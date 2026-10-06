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
void lsm_app_page_registry_enter(LsmApp *app, LsmTabIndex page,
                                 gboolean page_was_built);
GtkWidget *lsm_app_page_registry_search_widget(LsmApp *app, LsmTabIndex page);
bool lsm_app_page_registry_process_foreground(LsmTabIndex page);
bool lsm_app_page_registry_active_periodic_policy(
    const LsmApp *app, guint *interval, gboolean *whole_seconds);
gboolean lsm_app_page_registry_active_periodic_update(gpointer user_data);

/** Connect the registry-owned top-level navigation callback. */
void lsm_app_page_registry_connect_notebook(LsmApp *app);

/** Connect shell mechanics and replace page-aware keyboard policy. */
void lsm_app_page_registry_connect_window(LsmApp *app);

/*
 * app.c includes this header before app_shell.h. Route only the two shell
 * connection points that carry top-level page policy through the registry;
 * app_shell.c itself is compiled without these aliases and continues to own
 * window mechanics, styling and navigation widgets.
 */
#ifndef LSM_APP_PAGE_REGISTRY_IMPLEMENTATION
#define lsm_app_shell_connect_notebook lsm_app_page_registry_connect_notebook
#define lsm_app_shell_connect_window lsm_app_page_registry_connect_window
#endif

#endif
