// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_table_ui.h
 * @brief Shared GTK table construction and process-cell presentation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_TABLE_UI_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_TABLE_UI_H

#include <gtk/gtk.h>
#include <stddef.h>

/** Semantic formatting applied to one technical process-table column. */
typedef enum {
    LSM_PROCESS_TABLE_TEXT,
    LSM_PROCESS_TABLE_SIGNED,
    LSM_PROCESS_TABLE_UNSIGNED,
    LSM_PROCESS_TABLE_UINT64,
    LSM_PROCESS_TABLE_HANDLE_COUNT,
    LSM_PROCESS_TABLE_PERCENT,
    LSM_PROCESS_TABLE_CPU_PERCENT,
    LSM_PROCESS_TABLE_BYTES,
    LSM_PROCESS_TABLE_RATE,
    LSM_PROCESS_TABLE_PRIORITY,
    LSM_PROCESS_TABLE_TIME,
    LSM_PROCESS_TABLE_DURATION,
    LSM_PROCESS_TABLE_ELAPSED
} LsmProcessTableValueKind;

/** Optional resource-usage highlighting owned by the table renderer. */
typedef enum {
    LSM_PROCESS_TABLE_HEAT_NONE,
    LSM_PROCESS_TABLE_HEAT_PERCENT,
    LSM_PROCESS_TABLE_HEAT_RATE
} LsmProcessTableHeatKind;

/** Stable description of one technical process-table column. */
typedef struct {
    int column;
    const char *key;
    const char *title;
    LsmProcessTableValueKind value_kind;
    gboolean default_visible;
    gboolean expand;
    int minimum_width;
    LsmProcessTableHeatKind heat_kind;
} LsmProcessTableColumnSpec;

/** Narrow live policy projected into the process-table renderer. */
typedef struct {
    gboolean cpu_percent_per_core;
    unsigned logical_cores;
    gboolean heatmap;
} LsmProcessTablePresentation;

/**
 * Update the small presentation view consumed by process cell renderers.
 *
 * The renderer retains its own copy so callers do not expose application state
 * or couple cell formatting to the complete private application object.
 *
 * @param tree Technical process tree owning the renderers.
 * @param presentation Current display policy and CPU-capacity facts.
 */
void lsm_process_table_set_presentation(
    GtkWidget *tree, const LsmProcessTablePresentation *presentation);

/**
 * Append one typed technical-process column to a tree.
 *
 * @param tree Destination GtkTreeView.
 * @param spec Stable column description and formatting contract.
 * @return Newly created column, or NULL for invalid input.
 */
GtkTreeViewColumn *lsm_process_table_append_process_column(
    GtkWidget *tree, const LsmProcessTableColumnSpec *spec);

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
