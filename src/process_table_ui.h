// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_table_ui.h
 * @brief Shared GTK table construction for process inspection tools.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_TABLE_UI_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_TABLE_UI_H

#include <gtk/gtk.h>
#include <stddef.h>

/**
 * Build a scrollable sortable text table with a status label.
 *
 * @param [in] store Model whose string columns map directly to @p titles.
 * @param [in] titles Column captions.
 * @param [in] title_count Number of captions/model columns.
 * @param [in] expand_column Zero-based column that receives spare width.
 * @param [out] status_out Optional destination for the status label.
 * @return Newly created GTK box containing status and table widgets.
 */
GtkWidget *lsm_process_table_page(GtkListStore *store,
                                  const char *const *titles,
                                  size_t title_count,
                                  int expand_column,
                                  GtkWidget **status_out);

#endif
