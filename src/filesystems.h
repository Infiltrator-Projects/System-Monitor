// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file filesystems.h
 * @brief Graphical mounted-filesystem inventory and visibility controls.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_FILESYSTEMS_H
#define LINUX_SYSTEM_MONITOR_FILESYSTEMS_H

#include "app.h"

/**
 * Construct the File Systems tab and its retained GTK model.
 *
 * @param [in,out] app Owning application state, accessed on the GTK main thread.
 * @param [in,out] container Empty page container receiving the tab contents.
 */
void lsm_filesystems_build(LsmApp *app, GtkWidget *container);

/**
 * Re-read mountinfo and filesystem-capacity data into the visible model.
 *
 * @param [in,out] app Owning application state.
 */
void lsm_filesystems_refresh(LsmApp *app);


/**
 * Release the File Systems model reference retained by the application.
 *
 * GTK widgets release their own references during window destruction; this
 * function releases the model's construction reference and clears borrowed
 * widget pointers so repeated lifecycle tests observe complete ownership.
 *
 * @param [in,out] app Owning application state, or NULL.
 */
void lsm_filesystems_destroy(LsmApp *app);

/**
 * Timer callback that refreshes the File Systems tab while updates are active.
 *
 * @param [in,out] user_data LsmApp pointer supplied to GLib.
 * @return G_SOURCE_CONTINUE while the application remains active.
 */
gboolean lsm_filesystems_update(gpointer user_data);

#endif
