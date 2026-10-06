// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_page_registry.h
 * @brief Central GTK page construction and refresh ownership registry.
 *
 * The registry is the single composition point that maps top-level tab
 * identities to feature builders, manual refresh hooks and optional periodic
 * refresh callbacks. App runtime code therefore schedules pages generically
 * instead of depending on Services, Users and File Systems individually.
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
 * This preserves the existing F5/application-wide refresh semantics while
 * keeping knowledge of feature-specific refresh functions in one composition
 * module rather than in the timer owner.
 *
 * @param [in,out] app Application whose registered page models are refreshed.
 */
void lsm_app_page_registry_refresh_all(LsmApp *app);

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
