// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file services.h
 * @brief Public interface for the systemd Services tab.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_SERVICES_H
#define LINUX_SYSTEM_MONITOR_SERVICES_H

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Construct the Services tab and bind its asynchronous D-Bus actions.
 *
 * @param [in,out] app Application that owns the model and cancellables.
 * @param [in] container Empty GTK container that receives the service view.
 */
void lsm_services_build(LsmApp *app, GtkWidget *container);
/**
 * Request a non-blocking refresh of the systemd service inventory.
 *
 * Overlapping requests are coalesced; absence of systemd disables only this
 * tab rather than affecting the rest of the application.
 *
 * @param [in,out] app Application whose service model is refreshed.
 */
void lsm_services_refresh(LsmApp *app);
/**
 * Schedule the periodic Services refresh from the GTK main loop.
 *
 * @param [in,out] user_data Pointer to the owning LsmApp.
 * @return G_SOURCE_CONTINUE while the timer should remain active, otherwise
 *         G_SOURCE_REMOVE during shutdown.
 */
gboolean lsm_services_update(gpointer user_data);
/**
 * Cancel outstanding service operations and release tab-owned state.
 *
 * @param [in,out] app Application being shut down.
 */
void lsm_services_destroy(LsmApp *app);

#endif
