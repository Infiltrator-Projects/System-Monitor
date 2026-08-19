// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_menu.h
 * @brief Internal menu and user-action coordination API.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_APP_MENU_H
#define LINUX_SYSTEM_MONITOR_APP_MENU_H

#include "app.h"

/**
 * Build the global menu bar and bind application actions.
 *
 * @param [in,out] app Active application context receiving menu handles.
 * @return Newly created GTK menu bar owned by the receiving widget hierarchy.
 */
GtkWidget *lsm_app_menu_build(LsmApp *app);

/**
 * Refresh all user-visible inventories once while preserving pause state.
 *
 * @param [in] item Optional activating menu item; may be NULL for key actions.
 * @param [in,out] user_data Owning LsmApp context.
 */
void lsm_app_menu_refresh(GtkMenuItem *item, gpointer user_data);

/**
 * Show the graphical destination chooser and save a diagnostic snapshot.
 *
 * @param [in] item Optional activating menu item; may be NULL for key actions.
 * @param [in,out] user_data Owning LsmApp context.
 */
void lsm_app_menu_save_snapshot(GtkMenuItem *item, gpointer user_data);

#endif
