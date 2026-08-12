// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file summary_bar.h
 * @brief Persistent whole-system summary visible above every application tab.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_SUMMARY_BAR_H
#define LINUX_SYSTEM_MONITOR_SUMMARY_BAR_H

#include <gtk/gtk.h>

typedef struct LsmApp LsmApp;

/**
 * Build the persistent CPU, memory, disk, network and GPU summary.
 *
 * @param [in,out] app Application receiving the summary widget references.
 * @return Newly created summary widget, or NULL for an invalid application.
 */
GtkWidget *lsm_summary_bar_build(LsmApp *app);

/**
 * Present the current retained monitor snapshot in the summary.
 *
 * @param [in,out] app Application whose labels receive current values.
 */
void lsm_summary_bar_update(LsmApp *app);

#endif
