// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file summary_bar.c
 * @brief Low-cost cross-tab presentation of headline resource usage.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "summary_bar.h"

#include "app_internal.h"
#include "metric_format.h"
#include "ui_helpers.h"

#include <math.h>
#include <stdio.h>

static GtkWidget *summary_item(const char *caption, GtkWidget **value_out,
                               const char *tooltip)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *caption_label = gtk_label_new(NULL);
    char markup[96];
    (void)snprintf(markup, sizeof(markup),
                   "<span alpha='65%%'>%s</span>", caption);
    gtk_label_set_markup(GTK_LABEL(caption_label), markup);
    gtk_widget_set_halign(caption_label, GTK_ALIGN_CENTER);

    GtkWidget *value = gtk_label_new("N/A");
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
        summary_item("CPU", &app->shell.summary_cpu,
                     "Overall processor utilisation"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item("Memory", &app->shell.summary_memory,
                     "Physical memory currently in use"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item("Disk", &app->shell.summary_disk,
                     "Highest active time across physical disks"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item("Network", &app->shell.summary_network,
                     "Combined live send and receive rate"), 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid),
        summary_item("GPU", &app->shell.summary_gpu,
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
    const LsmMonitor *monitor = &app->monitor;
    lsm_ui_set_label_text(app->shell.summary_cpu, "%.0f%%",
                          monitor->cpu.usage_percent);
    lsm_ui_set_label_text(app->shell.summary_memory, "%.0f%%",
                          monitor->memory.usage_percent);

    double disk_peak = 0.0;
    for (size_t index = 0U; index < monitor->disk_count; index++)
        if (monitor->disks[index].active_percent > disk_peak)
            disk_peak = monitor->disks[index].active_percent;
    if (monitor->disk_count > 0U)
        lsm_ui_set_label_text(app->shell.summary_disk, "%.0f%%", disk_peak);
    else
        lsm_ui_set_label_text(app->shell.summary_disk, "N/A");

    long double network_rate = 0.0L;
    double network_peak = 0.0;
    for (size_t index = 0U; index < monitor->net_count; index++) {
        network_rate += (long double)monitor->nets[index].rx_bytes_per_sec +
                        (long double)monitor->nets[index].tx_bytes_per_sec;
        if (monitor->nets[index].utilisation_percent > network_peak)
            network_peak = monitor->nets[index].utilisation_percent;
    }
    if (monitor->net_count > 0U) {
        char rate[64];
        lsm_metric_format_network(network_rate, app->runtime.network_use_bits, true,
                                  rate, sizeof(rate));
        if (network_peak > 0.0)
            lsm_ui_set_label_text(app->shell.summary_network, "%s (%.0f%%)",
                                  rate, network_peak);
        else
            lsm_ui_set_label_text(app->shell.summary_network, "%s", rate);
    } else {
        lsm_ui_set_label_text(app->shell.summary_network, "N/A");
    }

    double gpu_peak = 0.0;
    bool gpu_available = false;
    for (size_t index = 0U; index < monitor->gpu_count; index++) {
        const LsmGpuInfo *gpu = &monitor->gpus[index];
        if (!gpu->utilization_available ||
            !isfinite(gpu->utilization_percent))
            continue;
        if (!gpu_available || gpu->utilization_percent > gpu_peak)
            gpu_peak = gpu->utilization_percent;
        gpu_available = true;
    }
    if (gpu_available)
        lsm_ui_set_label_text(app->shell.summary_gpu, "%.0f%%", gpu_peak);
    else
        lsm_ui_set_label_text(app->shell.summary_gpu, "N/A");
}
