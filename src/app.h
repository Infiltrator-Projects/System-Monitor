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

/** Allocate a zeroed application context. */
LsmApp *lsm_app_create(void);
/** Release an application context after lsm_app_shutdown(). */
void lsm_app_free(LsmApp *app);
/** Construct the single-window GUI on first activation or present it again. */
void lsm_app_activate(GtkApplication *application, gpointer user_data);
/** Stop timers, workers and owned state; safe for repeated calls. */
void lsm_app_shutdown(LsmApp *app);
/** Recreate fast presentation timers after refresh preferences change. */
void lsm_app_preferences_changed(LsmApp *app);

#endif
