// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file summary_bar.c
 * @brief Low-cost headline resource presentation for Compact Summary mode.
 *
 * This renderer owns only summary widgets and formatted monitor values. The
 * application adapter lives in summary_bar_adapter.c so this module does not
 * depend on the complete LsmApp object.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "summary_bar_view.h"

#include "performance_view.h"
#include "ui_helpers.h"

#include <stdio.h>
#include <string.h>

static GtkWidget *summary_item(const char *caption, GtkWidget **value_out,
                               const char *tooltip)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *caption_label = gtk_label_new(NULL);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(caption_label), "lsm-summary-caption");
    char markup[96];
    (void)snprintf(markup, sizeof(markup),
                   "<span alpha='65%%'>%s</span>", caption);
    gtk_label_set_markup(GTK_LABEL(caption_label), markup);
    gtk_widget_set_halign(caption_label, GTK_ALIGN_CENTER);

    GtkWidget *value = gtk_label_new("N/A");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(value), "lsm-summary-value");
    gtk_widget_set_halign(value, GTK_ALIGN_CENTER);
    gtk_label_set_width_chars(GTK_LABEL(value), 12);
    gtk_label_set_max_width_chars(GTK_LABEL(value), 12);
    gtk_label_set_xalign(GTK_LABEL(value), 0.5f);
    PangoAttrList *attributes = pango_attr_list_new();
    PangoAttribute *features = pango_attr_font_features_new("tnum=1");
    pango_attr_list_insert(attributes, features);
    gtk_label_set_attributes(GTK_LABEL(value), attributes);
    pango_attr_list_unref(attributes);
    gtk_label_set_selectable(GTK_LABEL(value), TRUE);
    gtk_widget_set_tooltip_text(box, tooltip);
    gtk_box_pack_start(GTK_BOX(box), caption_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), value, FALSE, FALSE, 0);
    *value_out = value;
    return box;
}

GtkWidget *lsm_summary_bar_view_build(LsmSummaryBarView *view)
{
    if (!view) return NULL;
    memset(view, 0, sizeof(*view));

    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_widget_set_name(frame, "lsm-summary-bar");
    GtkWidget *grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 28);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);

    gtk_grid_attach(GTK_GRID(grid),
        summary_item(lsm_summary_label(LSM_SUMMARY_CPU),
                     &view->values[LSM_SUMMARY_CPU],
                     "Overall processor utilisation"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item(lsm_summary_label(LSM_SUMMARY_MEMORY),
                     &view->values[LSM_SUMMARY_MEMORY],
                     "Physical memory currently in use"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item(lsm_summary_label(LSM_SUMMARY_DISK),
                     &view->values[LSM_SUMMARY_DISK],
                     "Highest active time across physical disks"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item(lsm_summary_label(LSM_SUMMARY_NETWORK),
                     &view->values[LSM_SUMMARY_NETWORK],
                     "Combined live send and receive rate"), 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item(lsm_summary_label(LSM_SUMMARY_GPU),
                     &view->values[LSM_SUMMARY_GPU],
                     "Highest readable graphics-adapter utilisation"),
        4, 0, 1, 1);
    gtk_container_add(GTK_CONTAINER(frame), grid);
    view->frame = frame;
    return frame;
}

void lsm_summary_bar_view_update(LsmSummaryBarView *view,
                                 const LsmMonitor *monitor,
                                 bool network_use_bits)
{
    if (!view || !view->frame) return;

    LsmSummaryPerformanceView values;
    lsm_summary_performance_view(monitor, network_use_bits, &values);

    for (size_t field = 0U; field < LSM_SUMMARY_COUNT; field++)
        lsm_ui_set_label_text(
            view->values[field], "%s", values.values[field]);
}
