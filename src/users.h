// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file users.h
 * @brief Public interface for the Users and Sessions tab.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_USERS_H
#define LINUX_SYSTEM_MONITOR_USERS_H

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Construct the login1 Users and Sessions tree and action controls.
 *
 * @param [in,out] app Application that owns the model and cancellables.
 * @param [in] container Empty GTK container receiving the users view.
 */
void lsm_users_build(LsmApp *app, GtkWidget *container);
/**
 * Request a non-blocking refresh of users and sessions from login1.
 *
 * @param [in,out] app Application whose users model is refreshed.
 */
void lsm_users_refresh(LsmApp *app);
/**
 * Schedule the periodic Users refresh from the GTK main loop.
 *
 * @param [in,out] user_data Pointer to the owning LsmApp.
 * @return G_SOURCE_CONTINUE while scheduled updates should remain active;
 *         otherwise G_SOURCE_REMOVE during shutdown.
 */
gboolean lsm_users_update(gpointer user_data);
/**
 * Cancel outstanding login1 requests and release tab-owned state.
 *
 * @param [in,out] app Application being shut down.
 */
void lsm_users_destroy(LsmApp *app);

#endif
