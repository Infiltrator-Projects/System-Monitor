// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance.h
 * @brief Public interface for the Performance tab.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PERFORMANCE_H
#define LINUX_SYSTEM_MONITOR_PERFORMANCE_H

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Construct the Performance tab and all pages required by current topology.
 *
 * The function establishes widget ownership inside @p app and creates graph
 * objects whose sample histories persist across ordinary refreshes. Dynamic
 * device pages may later be reconciled when topology generation changes.
 *
 * @param [in,out] app Long-lived application state owned by the GTK main loop.
 * @param [in] container Empty GTK container that receives the tab hierarchy.
 */
void lsm_performance_build(LsmApp *app, GtkWidget *container);
/**
 * Sample the retained monitor backend and present the newest visible values.
 *
 * This is a GTK timeout callback. It honours pause state, topology cadence and
 * page-visibility policy, and therefore must execute on the GTK main thread.
 *
 * @param [in,out] user_data Pointer to the owning LsmApp.
 * @return G_SOURCE_CONTINUE while periodic updates should remain scheduled;
 *         G_SOURCE_REMOVE during shutdown or for invalid application state.
 */
gboolean lsm_performance_update(gpointer user_data);
/**
 * Force a topology-aware Performance presentation outside the normal cadence.
 *
 * @param [in,out] app Application whose retained monitor and pages are updated.
 */
void lsm_performance_refresh(LsmApp *app);
/**
 * Re-negotiate the Performance layout after a toplevel window state change.
 *
 * Window managers can deliver maximise/restore state and configure events in
 * different orders, especially through remote X11 sessions. This function
 * invalidates only GTK layout state; it does not rebuild pages or samples.
 *
 * @param [in,out] app Application whose Performance hierarchy is reflowed.
 */
void lsm_performance_reflow(LsmApp *app);
/**
 * Release graph histories, device-page records and Performance-tab resources.
 *
 * The GTK widget hierarchy remains owned by GTK; this function releases only
 * project-owned auxiliary state and is safe to call during partial teardown.
 *
 * @param [in,out] app Application being shut down.
 */
void lsm_performance_destroy(LsmApp *app);

#endif
