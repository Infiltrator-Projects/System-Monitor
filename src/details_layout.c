// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file details_layout.c
 * @brief Technical Details column-layout persistence and migration owner.
 *
 * Table construction/filtering remains in details_page.c; this module owns the
 * durable layout format, migration rules, sort state and column geometry.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "details_layout.h"

#include "details_page.h"
#include "app_internal.h"
#include "atomic_file.h"
#include "common.h"

#include <infiltratr/config.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void lsm_details_save_layout(const LsmApp *app)
{
    if (!app || !app->details.details_tree || !app->details.details_sort_model ||
        lsm_mkdir_parents(app->paths.config_dir, 0700U) != 0)
        return;

    gint order[PROC_N_COLUMNS];
    for (int index = 0; index < PROC_N_COLUMNS; index++) order[index] = index;
    GList *columns = gtk_tree_view_get_columns(
        GTK_TREE_VIEW(app->details.details_tree));
    gint position = 0;
    for (GList *item = columns; item; item = item->next, position++) {
        for (int index = 0; index < PROC_N_COLUMNS; index++) {
            if (item->data == app->details.details_columns[index]) {
                order[index] = position;
                break;
            }
        }
    }
    g_list_free(columns);

    GString *text = g_string_new(NULL);
    g_string_append_printf(text,
        "layout_version=5\ntree=%d\nscope=%d\nheatmap=%d\n",
        app->details.details_tree_mode ? 1 : 0,
        app->details.details_scope,
        app->details.process_heatmap ? 1 : 0);
    for (int i = 0; i < PROC_N_COLUMNS; i++) {
        g_string_append_printf(text, "%s=%d\n", lsm_details_column_specs[i].key,
            gtk_tree_view_column_get_visible(app->details.details_columns[i])
                ? 1 : 0);
        g_string_append_printf(text, "width.%s=%d\n", lsm_details_column_specs[i].key,
            gtk_tree_view_column_get_width(app->details.details_columns[i]));
        g_string_append_printf(text, "order.%s=%d\n", lsm_details_column_specs[i].key,
                               order[i]);
    }

    gint sort_column = -1;
    GtkSortType sort_order = GTK_SORT_ASCENDING;
    if (gtk_tree_sortable_get_sort_column_id(
            GTK_TREE_SORTABLE(app->details.details_sort_model),
            &sort_column, &sort_order)) {
        g_string_append_printf(text, "sort_column=%d\nsort_order=%s\n",
            sort_column,
            sort_order == GTK_SORT_DESCENDING ? "descending" : "ascending");
    }
    const int failure = lsm_atomic_file_write_bytes(
        app->paths.column_path, LSM_ATOMIC_FILE_PRIVATE,
        text->str, text->len);
    if (failure != 0)
        fprintf(stderr, "Unable to save process column layout: %s\n",
                strerror(failure));
    g_string_free(text, TRUE);
}

static gboolean layout_boolean(const char *value, gboolean fallback)
{
    bool parsed = false;
    if (!infiltratr_config_parse_bool(value, &parsed)) return fallback;
    return parsed ? TRUE : FALSE;
}

static int layout_integer(const char *value, int minimum, int maximum,
                          int fallback)
{
    int64_t parsed = 0;
    return lsm_parse_i64_range(value, 10U, minimum, maximum, &parsed)
        ? (int)parsed : fallback;
}

void lsm_details_layout_load(LsmApp *app)
{
    gboolean visible[PROC_N_COLUMNS];
    gint widths[PROC_N_COLUMNS];
    gint order[PROC_N_COLUMNS];
    gint sort_column = -1;
    GtkSortType sort_order = GTK_SORT_ASCENDING;
    int layout_version = 0;

    app->details.details_tree_mode = TRUE;
    app->details.details_scope = PROCESS_SCOPE_ALL;
    app->details.process_heatmap = TRUE;
    for (int i = 0; i < PROC_N_COLUMNS; i++) {
        visible[i] = lsm_details_column_specs[i].default_visible;
        widths[i] = lsm_details_column_specs[i].minimum_width;
        order[i] = i;
    }

    char *content = NULL;
    size_t content_length = 0U;
    if (lsm_read_text_file_alloc(app->paths.column_path, &content,
                                 &content_length) == INFILTRATR_IO_OK) {
        char *save = NULL;
        for (char *line = strtok_r(content, "\n", &save); line;
             line = strtok_r(NULL, "\n", &save)) {
            char *key = NULL;
            char *value = NULL;
            if (infiltratr_config_parse_line(line, &key, &value) !=
                INFILTRATR_CONFIG_LINE_ENTRY)
                continue;

            if (strcmp(key, "layout_version") == 0)
                layout_version = layout_integer(
                    value, 0, INT_MAX, layout_version);
            else if (strcmp(key, "tree") == 0)
                app->details.details_tree_mode = layout_boolean(
                    value, app->details.details_tree_mode);
            else if (strcmp(key, "scope") == 0)
                app->details.details_scope = layout_integer(
                    value, PROCESS_SCOPE_ALL, PROCESS_SCOPE_MINE,
                    app->details.details_scope);
            else if (strcmp(key, "heatmap") == 0)
                app->details.process_heatmap = layout_boolean(
                    value, app->details.process_heatmap);
            else if (strcmp(key, "sort_column") == 0)
                sort_column = layout_integer(
                    value, 0, PROC_N_COLUMNS - 1, sort_column);
            else if (strcmp(key, "sort_order") == 0) {
                if (strcmp(value, "descending") == 0)
                    sort_order = GTK_SORT_DESCENDING;
                else if (strcmp(value, "ascending") == 0)
                    sort_order = GTK_SORT_ASCENDING;
            } else {
                for (int i = 0; i < PROC_N_COLUMNS; i++) {
                    char width_key[64];
                    char order_key[64];
                    snprintf(width_key, sizeof(width_key), "width.%s",
                             lsm_details_column_specs[i].key);
                    snprintf(order_key, sizeof(order_key), "order.%s",
                             lsm_details_column_specs[i].key);
                    if (strcmp(key, lsm_details_column_specs[i].key) == 0) {
                        visible[i] = layout_boolean(value, visible[i]);
                        break;
                    }
                    if (strcmp(key, width_key) == 0) {
                        widths[i] = layout_integer(
                            value, 40, 2000, widths[i]);
                        break;
                    }
                    if (strcmp(key, order_key) == 0) {
                        order[i] = layout_integer(
                            value, 0, PROC_N_COLUMNS - 1, order[i]);
                        break;
                    }
                }
            }
        }
        free(content);
    }
    if (layout_version < 2) {
        for (int i = 0; i < PROC_N_COLUMNS; i++)
            visible[i] = lsm_details_column_specs[i].default_visible;
    }
    if (layout_version < 3) {
        const int previous_memory_column = 6;
        const int previous_read_total_column = 11;
        if (sort_column >= previous_read_total_column)
            sort_column += 3;
        else if (sort_column >= previous_memory_column)
            sort_column += 1;
        for (int i = 0; i < PROC_N_COLUMNS; i++) order[i] = i;
    }
    if (layout_version < 4) {
        if (sort_column >= PROC_COL_GPU_ENGINE) sort_column++;
        for (int i = 0; i < PROC_N_COLUMNS; i++) order[i] = i;
    }
    if (layout_version < 5) {
        sort_column = -1;
        for (int i = 0; i < PROC_N_COLUMNS; i++) order[i] = i;
    }
    for (int i = 0; i < PROC_N_COLUMNS; i++) {
        gtk_tree_view_column_set_visible(
            app->details.details_columns[i], visible[i]);
        gtk_tree_view_column_set_sizing(
            app->details.details_columns[i], GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_fixed_width(
            app->details.details_columns[i], widths[i]);
    }
    GtkTreeViewColumn *previous = NULL;
    for (int position = 0; position < PROC_N_COLUMNS; position++) {
        for (int index = 0; index < PROC_N_COLUMNS; index++) {
            if (order[index] != position) continue;
            gtk_tree_view_move_column_after(
                GTK_TREE_VIEW(app->details.details_tree),
                app->details.details_columns[index], previous);
            previous = app->details.details_columns[index];
            break;
        }
    }
    if (sort_column >= 0)
        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(app->details.details_sort_model),
            sort_column, sort_order);

    gtk_tree_view_column_set_visible(
        app->details.details_columns[PROC_COL_NAME], TRUE);
}
