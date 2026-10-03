// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_menu.h
 * @brief Internal overflow-menu and user-action coordination API.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_MENU_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_MENU_H

#include "app.h"

/**
 * Build the specialist Tools/Help popup used by the header overflow control.
 *
 * @param [in,out] app Active application context receiving menu handles.
 * @return Newly created GTK popup menu owned by the header overflow control.
 */
GtkWidget *lsm_app_menu_build(LsmApp *app);

/**
 * Attach the specialist overflow control to the InfiltratorOS header.
 *
 * @param [in,out] app Active application with an already-created shell header.
 */
void lsm_app_menu_attach_to_header(LsmApp *app);

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
