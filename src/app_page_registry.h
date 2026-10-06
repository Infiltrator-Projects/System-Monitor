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

/**
 * Construct one registered top-level page.
 *
 * @param [in,out] app Application receiving page-owned presentation state.
 * @param [in] page Stable top-level page identity.
 * @param [in] container GTK container receiving the page contents.
 * @return true when a registered builder was invoked, otherwise false.
 */
bool lsm_app_page_registry_build(LsmApp *app, LsmTabIndex page,
                                 GtkWidget *container);

/**
 * Run every registered application-wide manual refresh hook.
 *
 * @param [in,out] app Application whose registered page models are refreshed.
 */
void lsm_app_page_registry_refresh_all(LsmApp *app);

/**
 * Dispatch the activation policy for one top-level page.
 *
 * @param [in,out] app Application entering the page.
 * @param [in] page Stable top-level page identity.
 * @param [in] page_was_built TRUE when the page existed before this entry.
 */
void lsm_app_page_registry_enter(LsmApp *app, LsmTabIndex page,
                                 gboolean page_was_built);

/**
 * Resolve the search control associated with a top-level page.
 *
 * @param [in,out] app Application containing page widgets.
 * @param [in] page Stable top-level page identity.
 * @return Search widget for the page, or NULL when the page has none.
 */
GtkWidget *lsm_app_page_registry_search_widget(LsmApp *app, LsmTabIndex page);

/**
 * Report whether a page requires foreground process-sampling cadence.
 *
 * @param [in] page Stable top-level page identity.
 * @return true for foreground process pages, otherwise false.
 */
bool lsm_app_page_registry_process_foreground(LsmTabIndex page);

/**
 * Resolve optional periodic refresh policy for the active page.
 *
 * @param [in] app Application containing current navigation/runtime state.
 * @param [out] interval Resolved timeout interval.
 * @param [out] whole_seconds TRUE when the timeout uses whole-second cadence.
 * @return true when the active page owns periodic work, otherwise false.
 */
bool lsm_app_page_registry_active_periodic_policy(
    const LsmApp *app, guint *interval, gboolean *whole_seconds);

/**
 * Run the active page's registered periodic callback.
 *
 * @param [in,out] user_data Pointer to the owning LsmApp.
 * @return G_SOURCE_CONTINUE or G_SOURCE_REMOVE from the page callback.
 */
gboolean lsm_app_page_registry_active_periodic_update(gpointer user_data);

/**
 * Connect the registry-owned top-level navigation callback.
 *
 * @param [in,out] app Application whose notebook navigation is connected.
 */
void lsm_app_page_registry_connect_notebook(LsmApp *app);

/**
 * Connect shell mechanics and replace page-aware keyboard policy.
 *
 * @param [in,out] app Application whose top-level window is connected.
 */
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
