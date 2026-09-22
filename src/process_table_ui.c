// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_table_ui.c
 * @brief Shared GTK table construction for process inspection tools.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_table_ui.h"

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
