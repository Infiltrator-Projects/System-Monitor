// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file startup.h
 * @brief Public interface for the Startup Applications tab.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_STARTUP_H
#define LINUX_SYSTEM_MONITOR_STARTUP_H

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Construct the Startup Applications tab from XDG autostart semantics.
 *
 * @param [in,out] app Application that owns the model and action widgets.
 * @param [in] container Empty GTK container receiving the startup view.
 */
void lsm_startup_build(LsmApp *app, GtkWidget *container);
/**
 * Reconcile system and per-user XDG autostart entries into the current model.
 *
 * User overrides are interpreted according to XDG precedence rather than as
 * independent duplicates.
 *
 * @param [in,out] app Application whose startup model is refreshed.
 */
void lsm_startup_refresh(LsmApp *app);

/**
 * Release the Startup Applications model's creator reference.
 *
 * @param [in,out] app Application whose search timer has already stopped.
 */
void lsm_startup_destroy(LsmApp *app);

#endif
