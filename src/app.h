// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app.h
 * @brief Opaque lifetime boundary for the Linux System Monitor application.
 *
 * Feature modules receive LsmApp pointers through their own interfaces; the
 * concrete GTK/state layout is private to app_internal.h. Keeping this public
 * boundary opaque prevents main.c and unrelated consumers from depending on
 * the complete application object.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_APP_H
#define LINUX_SYSTEM_MONITOR_APP_H

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Allocate a zero-initialised application context.
 *
 * @return Newly allocated opaque application context, or NULL on allocation
 *         failure.
 */
LsmApp *lsm_app_create(void);

/**
 * Release an application context after its owned subsystems are shut down.
 *
 * @param [in,out] app Application context to release; NULL is accepted.
 */
void lsm_app_free(LsmApp *app);

/**
 * Construct the single-window GUI on first activation or present it thereafter.
 *
 * @param [in] application Active GTK application instance.
 * @param [in,out] user_data Opaque LsmApp context supplied at signal binding.
 */
void lsm_app_activate(GtkApplication *application, gpointer user_data);

/**
 * Stop timers and workers and release all subsystem-owned state.
 *
 * The operation is idempotent so GTK and GApplication shutdown paths may both
 * call it safely.
 *
 * @param [in,out] app Application context to shut down; NULL is accepted.
 */
void lsm_app_shutdown(LsmApp *app);

/**
 * Recreate presentation timers after refresh preferences change.
 *
 * @param [in,out] app Active application context whose cadence changed.
 */
void lsm_app_preferences_changed(LsmApp *app);

#endif
