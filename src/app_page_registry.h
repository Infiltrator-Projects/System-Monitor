// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_page_registry.h
 * @brief Central GTK page construction, refresh and cadence ownership registry.
 *
 * The registry is the single composition point that maps top-level tab
 * identities to feature builders, manual refresh hooks and cadence classes.
 * Runtime code therefore schedules pages and process sampling generically
 * instead of depending on individual page modules.
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
 * Construct one top-level page through the central page registry.
 *
 * @param [in,out] app Application that owns the page state.
 * @param page Top-level page identity to construct.
 * @param [in,out] container Empty GTK container receiving the page contents.
 * @return true when @p page names a registered builder and construction was
 *         dispatched; false for invalid or unregistered page identities.
 */
bool lsm_app_page_registry_build(LsmApp *app, LsmTabIndex page,
                                 GtkWidget *container);

/**
 * Run every page-level manual refresh hook once.
 *
 * @param [in,out] app Application whose registered page models are refreshed.
 */
void lsm_app_page_registry_refresh_all(LsmApp *app);

/**
 * Report whether a page requires foreground process-sampling cadence.
 *
 * @param page Top-level page identity to classify.
 * @return true for pages whose live process presentation needs foreground
 *         cadence; false for every other page and invalid identities.
 */
bool lsm_app_page_registry_process_foreground(LsmTabIndex page);

/**
 * Resolve the optional periodic-refresh policy for the active page.
 *
 * @param [in] app Application containing active-page and lazy-build state.
 * @param [out] interval Receives milliseconds when @p whole_seconds is false,
 *              otherwise whole seconds suitable for g_timeout_add_seconds().
 * @param [out] whole_seconds Receives true for coalescable whole-second timers.
 * @return true when the active, constructed page owns a periodic refresh.
 */
bool lsm_app_page_registry_active_periodic_policy(
    const LsmApp *app, guint *interval, gboolean *whole_seconds);

/**
 * Dispatch the active page's registered periodic refresh callback.
 *
 * @param [in,out] user_data LsmApp supplied to the GLib source.
 * @return The registered page callback result, or G_SOURCE_REMOVE when the
 *         active page no longer owns a periodic refresh.
 */
gboolean lsm_app_page_registry_active_periodic_update(gpointer user_data);

#endif
