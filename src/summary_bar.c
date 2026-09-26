// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file summary_bar.c
 * @brief Low-cost headline resource presentation for Compact Summary mode.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "summary_bar.h"

#include "app_internal.h"
#include "ui_helpers.h"
#include "performance_view.h"

#include <stdio.h>

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
    gtk_label_set_selectable(GTK_LABEL(value), TRUE);
    gtk_widget_set_tooltip_text(box, tooltip);
    gtk_box_pack_start(GTK_BOX(box), caption_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), value, FALSE, FALSE, 0);
    *value_out = value;
    return box;
}

GtkWidget *lsm_summary_bar_build(LsmApp *app)
{
    if (!app) return NULL;
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_widget_set_name(frame, "lsm-summary-bar");
    GtkWidget *grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 28);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);

    gtk_grid_attach(GTK_GRID(grid),
        summary_item(lsm_summary_label(LSM_SUMMARY_CPU),
                     &app->shell.summary_cpu,
                     "Overall processor utilisation"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item(lsm_summary_label(LSM_SUMMARY_MEMORY),
                     &app->shell.summary_memory,
                     "Physical memory currently in use"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item(lsm_summary_label(LSM_SUMMARY_DISK),
                     &app->shell.summary_disk,
                     "Highest active time across physical disks"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item(lsm_summary_label(LSM_SUMMARY_NETWORK),
                     &app->shell.summary_network,
                     "Combined live send and receive rate"), 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item(lsm_summary_label(LSM_SUMMARY_GPU),
                     &app->shell.summary_gpu,
                     "Highest readable graphics-adapter utilisation"),
        4, 0, 1, 1);
    gtk_container_add(GTK_CONTAINER(frame), grid);
    app->shell.summary_bar = frame;
    lsm_summary_bar_update(app);
    return frame;
}

void lsm_summary_bar_update(LsmApp *app)
{
    if (!app || !app->shell.summary_bar) return;

    LsmSummaryPerformanceView view;
    lsm_summary_performance_view(
        &app->monitor, app->runtime.network_use_bits, &view);

    lsm_ui_set_label_text(
        app->shell.summary_cpu, "%s", view.values[LSM_SUMMARY_CPU]);
    lsm_ui_set_label_text(
        app->shell.summary_memory, "%s", view.values[LSM_SUMMARY_MEMORY]);
    lsm_ui_set_label_text(
        app->shell.summary_disk, "%s", view.values[LSM_SUMMARY_DISK]);
    lsm_ui_set_label_text(
        app->shell.summary_network, "%s", view.values[LSM_SUMMARY_NETWORK]);
    lsm_ui_set_label_text(
        app->shell.summary_gpu, "%s", view.values[LSM_SUMMARY_GPU]);
}
