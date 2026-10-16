// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_log_plot.h
 * @brief Process-recording log plotting presentation action.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef SYSTEM_MONITOR_PROCESS_LOG_PLOT_H
#define SYSTEM_MONITOR_PROCESS_LOG_PLOT_H

#include <gtk/gtk.h>

/**
 * Open a recorded-process CSV and asynchronously build its CPU/memory graph.
 *
 * @param item Activating menu item; may be NULL for programmatic invocation.
 * @param user_data LsmApp owning the parent window and shutdown state.
 */
void lsm_process_log_plot_show(GtkMenuItem *item, gpointer user_data);

#endif
