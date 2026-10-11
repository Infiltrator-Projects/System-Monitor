// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_table_ui.c
 * @brief Shared GTK table construction and process-cell presentation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_table_ui.h"

#include "common.h"
#include "process_model.h"
#include "temporal_presentation.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>

#define LSM_PROCESS_TABLE_PRESENTATION_KEY "lsm-process-table-presentation"
#define LSM_PROCESS_TABLE_COLUMN_KEY "lsm-process-table-column"
#define LSM_PROCESS_TABLE_VALUE_KIND_KEY "lsm-process-table-value-kind"
#define LSM_PROCESS_TABLE_HEAT_KIND_KEY "lsm-process-table-heat-kind"

static const LsmProcessTablePresentation *process_table_presentation(
    GtkWidget *tree)
{
    return tree
        ? g_object_get_data(
              G_OBJECT(tree), LSM_PROCESS_TABLE_PRESENTATION_KEY)
        : NULL;
}

void lsm_process_table_set_presentation(
    GtkWidget *tree, const LsmProcessTablePresentation *presentation)
{
    if (!tree || !presentation) return;

    LsmProcessTablePresentation *stored = g_object_get_data(
        G_OBJECT(tree), LSM_PROCESS_TABLE_PRESENTATION_KEY);
    if (!stored) {
        stored = g_new0(LsmProcessTablePresentation, 1U);
        g_object_set_data_full(
            G_OBJECT(tree), LSM_PROCESS_TABLE_PRESENTATION_KEY,
            stored, g_free);
    }
    *stored = *presentation;
}

static void apply_resource_heatmap(
    GtkWidget *tree, GtkCellRenderer *renderer,
    LsmProcessTableHeatKind heat_kind, double value)
{
    const LsmProcessTablePresentation *presentation =
        process_table_presentation(tree);
    if (!presentation || !presentation->heatmap || value <= 0.0 ||
        heat_kind == LSM_PROCESS_TABLE_HEAT_NONE) {
        g_object_set(renderer, "cell-background-set", FALSE, NULL);
        return;
    }

    double intensity = 0.0;
    switch (heat_kind) {
        case LSM_PROCESS_TABLE_HEAT_NONE:
            break;
        case LSM_PROCESS_TABLE_HEAT_PERCENT:
            intensity = fmin(value / 100.0, 1.0);
            break;
        case LSM_PROCESS_TABLE_HEAT_RATE:
            intensity = fmin(log10(value + 1.0) / 9.0, 1.0);
            break;
    }

    GdkRGBA colour = {0.96, 0.48, 0.10, 0.08 + 0.30 * intensity};
    g_object_set(renderer,
                 "cell-background-rgba", &colour,
                 "cell-background-set", TRUE,
                 NULL);
}

static void process_cell_data(GtkTreeViewColumn *view_column,
                              GtkCellRenderer *renderer,
                              GtkTreeModel *model, GtkTreeIter *iter,
                              gpointer user_data)
{
    (void)view_column;
    GtkWidget *tree = user_data;
    const int column = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(renderer), LSM_PROCESS_TABLE_COLUMN_KEY));
    const LsmProcessTableValueKind value_kind =
        (LsmProcessTableValueKind)GPOINTER_TO_INT(
            g_object_get_data(
                G_OBJECT(renderer), LSM_PROCESS_TABLE_VALUE_KIND_KEY));
    const LsmProcessTableHeatKind heat_kind =
        (LsmProcessTableHeatKind)GPOINTER_TO_INT(
            g_object_get_data(
                G_OBJECT(renderer), LSM_PROCESS_TABLE_HEAT_KIND_KEY));
    const LsmProcessTablePresentation *presentation =
        process_table_presentation(tree);
    char text[256] = "";
    double heat_value = 0.0;

    switch (value_kind) {
        case LSM_PROCESS_TABLE_TEXT: {
            gchar *value = NULL;
            gtk_tree_model_get(model, iter, column, &value, -1);
            lsm_copy_string(text, sizeof(text), value ? value : "");
            g_free(value);
            break;
        }
        case LSM_PROCESS_TABLE_SIGNED: {
            gint value = 0;
            gtk_tree_model_get(model, iter, column, &value, -1);
            snprintf(text, sizeof(text), "%d", value);
            break;
        }
        case LSM_PROCESS_TABLE_UNSIGNED: {
            guint value = 0U;
            gtk_tree_model_get(model, iter, column, &value, -1);
            snprintf(text, sizeof(text), "%u", value);
            break;
        }
        case LSM_PROCESS_TABLE_UINT64: {
            guint64 value = 0U;
            gtk_tree_model_get(model, iter, column, &value, -1);
            snprintf(text, sizeof(text), "%llu",
                     (unsigned long long)value);
            break;
        }
        case LSM_PROCESS_TABLE_HANDLE_COUNT: {
            guint value = 0U;
            gtk_tree_model_get(model, iter, column, &value, -1);
            if (value == UINT_MAX)
                snprintf(text, sizeof(text), "N/A");
            else
                snprintf(text, sizeof(text), "%u", value);
            break;
        }
        case LSM_PROCESS_TABLE_PERCENT:
        case LSM_PROCESS_TABLE_CPU_PERCENT: {
            double value = 0.0;
            gtk_tree_model_get(model, iter, column, &value, -1);
            if (!isfinite(value)) {
                snprintf(text, sizeof(text), "N/A");
                break;
            }
            if (value_kind == LSM_PROCESS_TABLE_CPU_PERCENT && presentation &&
                presentation->cpu_percent_per_core) {
                const unsigned cores = presentation->logical_cores
                    ? presentation->logical_cores : 1U;
                value *= (double)cores;
            }
            snprintf(text, sizeof(text), "%.1f%%", value);
            heat_value = value;
            break;
        }
        case LSM_PROCESS_TABLE_BYTES: {
            guint64 value = 0U;
            gtk_tree_model_get(model, iter, column, &value, -1);
            if (value == UINT64_MAX)
                snprintf(text, sizeof(text), "N/A");
            else
                lsm_format_bytes(value, text, sizeof(text));
            break;
        }
        case LSM_PROCESS_TABLE_RATE: {
            double value = 0.0;
            gtk_tree_model_get(model, iter, column, &value, -1);
            if (!isfinite(value)) {
                snprintf(text, sizeof(text), "N/A");
                break;
            }
            lsm_format_rate(value, text, sizeof(text));
            heat_value = value;
            break;
        }
        case LSM_PROCESS_TABLE_PRIORITY: {
            gint value = 0;
            gtk_tree_model_get(model, iter, column, &value, -1);
            lsm_copy_string(
                text, sizeof(text),
                lsm_process_priority_name((LsmProcessPriority)value));
            break;
        }
        case LSM_PROCESS_TABLE_TIME: {
            gint64 epoch = 0;
            gtk_tree_model_get(model, iter, column, &epoch, -1);
            if (epoch <= 0 ||
                !lsm_temporal_format_epoch_seconds(
                    epoch, true, false, true, text, sizeof(text)))
                snprintf(text, sizeof(text), "N/A");
            break;
        }
        case LSM_PROCESS_TABLE_DURATION: {
            guint64 seconds = 0U;
            gtk_tree_model_get(model, iter, column, &seconds, -1);
            (void)lsm_temporal_format_duration_seconds(
                seconds, text, sizeof(text));
            break;
        }
        case LSM_PROCESS_TABLE_ELAPSED: {
            guint64 seconds = 0U;
            gtk_tree_model_get(model, iter, column, &seconds, -1);
            (void)lsm_temporal_format_elapsed_seconds(
                seconds, text, sizeof(text));
            break;
        }
    }

    g_object_set(renderer, "text", text, NULL);
    apply_resource_heatmap(tree, renderer, heat_kind, heat_value);
}

GtkTreeViewColumn *lsm_process_table_append_process_column(
    GtkWidget *tree, const LsmProcessTableColumnSpec *spec)
{
    if (!tree || !spec || spec->column < 0 || !spec->title) return NULL;

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set_data(G_OBJECT(renderer), LSM_PROCESS_TABLE_COLUMN_KEY,
                      GINT_TO_POINTER(spec->column));
    g_object_set_data(G_OBJECT(renderer), LSM_PROCESS_TABLE_VALUE_KIND_KEY,
                      GINT_TO_POINTER((int)spec->value_kind));
    g_object_set_data(G_OBJECT(renderer), LSM_PROCESS_TABLE_HEAT_KIND_KEY,
                      GINT_TO_POINTER((int)spec->heat_kind));
    g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    if (spec->value_kind != LSM_PROCESS_TABLE_TEXT)
        g_object_set(renderer, "xalign", 1.0, NULL);

    GtkTreeViewColumn *column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, spec->title);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(
        column, renderer, process_cell_data, tree, NULL);
    gtk_tree_view_column_set_sort_column_id(column, spec->column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, spec->expand);
    gtk_tree_view_column_set_min_width(column, spec->minimum_width);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    return column;
}

static GtkTreeViewColumn *append_text_column(GtkWidget *tree,
                                             const char *title,
                                             int model_column,
                                             gboolean expand)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        title, renderer, "text", model_column, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, model_column);
    gtk_tree_view_column_set_expand(column, expand);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    return column;
}

GtkWidget *lsm_process_table_page(GtkListStore *store,
                                  const char *const *titles,
                                  size_t title_count,
                                  int expand_column,
                                  GtkWidget **status_out)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(box), 8);
    GtkWidget *status = gtk_label_new("Not yet refreshed");
    gtk_widget_set_halign(status, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), status, FALSE, FALSE, 0);
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree), TRUE);
    for (size_t index = 0U; index < title_count; index++)
        append_text_column(tree, titles[index], (int)index,
                           (int)index == expand_column);
    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_container_add(GTK_CONTAINER(scroller), tree);
    gtk_box_pack_start(GTK_BOX(box), scroller, TRUE, TRUE, 0);
    if (status_out) *status_out = status;
    return box;
}
