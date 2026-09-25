// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file overview.c
 * @brief GTK presentation for the completed-snapshot system Overview.
 *
 * The Overview never samples hardware itself. Performance's retained monitor
 * path records completed snapshots into overview_history.c even while this tab
 * is closed; this module only projects that bounded history into cards.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "overview.h"

#include "app_internal.h"
#include "common.h"
#include "metric_format.h"
#include "overview_history.h"
#include "performance.h"
#include "presentation_contract.h"
#include "temporal_presentation.h"
#include "ui_helpers.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *const overview_titles[LSM_OVERVIEW_METRIC_COUNT] = {
    "CPU", "Memory", "Disk activity", "Network traffic", "GPU",
    "Temperature", "CPU pressure", "Memory pressure", "I/O pressure"
};

static const char *const overview_style_classes[LSM_OVERVIEW_METRIC_COUNT] = {
    "lsm-overview-cpu",
    "lsm-overview-memory",
    "lsm-overview-disk",
    "lsm-overview-network",
    "lsm-overview-gpu",
    "lsm-overview-temperature",
    "lsm-overview-cpu-pressure",
    "lsm-overview-memory-pressure",
    "lsm-overview-io-pressure"
};

/* Dashboard geometry is intentionally asymmetric.  The primary resources
 * occupy the wide first row, the device/activity cards share the second row,
 * and compact diagnostic cards form the third row.  This mirrors the approved
 * graphical north star and, on a 1080-line desktop, keeps the whole Overview
 * visible without turning every metric into the same-sized procedural tile. */
typedef struct {
    LsmOverviewMetric metric;
    gint column;
    gint row;
    gint width;
} LsmOverviewPlacement;

static const LsmOverviewPlacement overview_layout[LSM_OVERVIEW_METRIC_COUNT] = {
    { LSM_OVERVIEW_CPU,             0, 0, 7 },
    { LSM_OVERVIEW_MEMORY,          7, 0, 5 },
    { LSM_OVERVIEW_DISK,            0, 1, 5 },
    { LSM_OVERVIEW_NETWORK,         5, 1, 4 },
    { LSM_OVERVIEW_GPU,             9, 1, 3 },
    { LSM_OVERVIEW_TEMPERATURE,     0, 2, 4 },
    { LSM_OVERVIEW_CPU_PRESSURE,    4, 2, 2 },
    { LSM_OVERVIEW_MEMORY_PRESSURE, 6, 2, 3 },
    { LSM_OVERVIEW_IO_PRESSURE,     9, 2, 3 }
};

static const char *overview_colour(LsmOverviewMetric metric)
{
    switch (metric) {
        case LSM_OVERVIEW_CPU:
        case LSM_OVERVIEW_CPU_PRESSURE:
            return LSM_COLOUR_CPU;
        case LSM_OVERVIEW_MEMORY:
        case LSM_OVERVIEW_MEMORY_PRESSURE:
            return LSM_COLOUR_MEMORY;
        case LSM_OVERVIEW_DISK:
        case LSM_OVERVIEW_IO_PRESSURE:
            return LSM_COLOUR_DISK;
        case LSM_OVERVIEW_NETWORK:
            return LSM_COLOUR_NETWORK;
        case LSM_OVERVIEW_GPU:
            return LSM_COLOUR_GPU;
        case LSM_OVERVIEW_TEMPERATURE:
            return LSM_COLOUR_BATTERY;
        case LSM_OVERVIEW_METRIC_COUNT:
            break;
    }
    return LSM_COLOUR_CPU;
}

static const char *overview_icon_colour(LsmOverviewMetric metric)
{
    switch (metric) {
        case LSM_OVERVIEW_CPU:
        case LSM_OVERVIEW_CPU_PRESSURE:
            return "#27d8ff";
        case LSM_OVERVIEW_MEMORY:
        case LSM_OVERVIEW_MEMORY_PRESSURE:
            return "#d84cff";
        case LSM_OVERVIEW_DISK:
        case LSM_OVERVIEW_IO_PRESSURE:
            return "#65eb46";
        case LSM_OVERVIEW_NETWORK:
            return "#2adcf3";
        case LSM_OVERVIEW_GPU:
            return "#e45dff";
        case LSM_OVERVIEW_TEMPERATURE:
            return "#ff4fa7";
        case LSM_OVERVIEW_METRIC_COUNT:
            break;
    }
    return "#27d8ff";
}

static void overview_stroke_neon(cairo_t *cr, const GdkRGBA *colour)
{
    cairo_set_source_rgba(
        cr, colour->red, colour->green, colour->blue, 0.18);
    cairo_set_line_width(cr, 5.0);
    cairo_stroke_preserve(cr);
    cairo_set_source_rgba(
        cr, colour->red, colour->green, colour->blue, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);
}

static gboolean overview_resource_icon_draw(
    GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    (void)user_data;
    const gint stored = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(widget), "lsm-overview-icon-metric"));
    if (stored <= 0 || stored > (gint)LSM_OVERVIEW_METRIC_COUNT)
        return FALSE;
    const LsmOverviewMetric metric = (LsmOverviewMetric)(stored - 1);
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    const double w = (double)allocation.width;
    const double h = (double)allocation.height;
    const double s = fmin(w, h);
    const double x = (w - s) / 2.0;
    const double y = (h - s) / 2.0;
    GdkRGBA colour = {0.15, 0.85, 1.0, 1.0};
    (void)gdk_rgba_parse(&colour, overview_icon_colour(metric));
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_new_path(cr);

    switch (metric) {
        case LSM_OVERVIEW_CPU:
        case LSM_OVERVIEW_GPU: {
            const double left = x + s * 0.28;
            const double top = y + s * 0.28;
            const double side = s * 0.44;
            cairo_rectangle(cr, left, top, side, side);
            cairo_rectangle(
                cr, x + s * 0.39, y + s * 0.39, s * 0.22, s * 0.22);
            for (int pin = 0; pin < 3; pin++) {
                const double p = 0.36 + 0.14 * (double)pin;
                cairo_move_to(cr, x + s * p, y + s * 0.14);
                cairo_line_to(cr, x + s * p, top);
                cairo_move_to(cr, x + s * p, top + side);
                cairo_line_to(cr, x + s * p, y + s * 0.86);
                cairo_move_to(cr, x + s * 0.14, y + s * p);
                cairo_line_to(cr, left, y + s * p);
                cairo_move_to(cr, left + side, y + s * p);
                cairo_line_to(cr, x + s * 0.86, y + s * p);
            }
            break;
        }
        case LSM_OVERVIEW_MEMORY:
        case LSM_OVERVIEW_MEMORY_PRESSURE:
            for (int row = 0; row < 3; row++)
                cairo_rectangle(
                    cr, x + s * 0.20, y + s * (0.24 + 0.22 * row),
                    s * 0.60, s * 0.11);
            break;
        case LSM_OVERVIEW_DISK:
        case LSM_OVERVIEW_IO_PRESSURE:
            cairo_rectangle(
                cr, x + s * 0.18, y + s * 0.26, s * 0.64, s * 0.48);
            cairo_move_to(cr, x + s * 0.25, y + s * 0.59);
            cairo_line_to(cr, x + s * 0.66, y + s * 0.59);
            cairo_arc(
                cr, x + s * 0.72, y + s * 0.59, s * 0.025, 0.0, 2.0 * G_PI);
            break;
        case LSM_OVERVIEW_NETWORK:
            cairo_arc(
                cr, x + s * 0.50, y + s * 0.67, s * 0.12,
                1.15 * G_PI, 1.85 * G_PI);
            cairo_arc(
                cr, x + s * 0.50, y + s * 0.69, s * 0.26,
                1.18 * G_PI, 1.82 * G_PI);
            cairo_arc(
                cr, x + s * 0.50, y + s * 0.71, s * 0.40,
                1.20 * G_PI, 1.80 * G_PI);
            cairo_arc(
                cr, x + s * 0.50, y + s * 0.72, s * 0.035, 0.0, 2.0 * G_PI);
            break;
        case LSM_OVERVIEW_TEMPERATURE:
            cairo_move_to(cr, x + s * 0.50, y + s * 0.20);
            cairo_line_to(cr, x + s * 0.50, y + s * 0.62);
            cairo_arc(
                cr, x + s * 0.50, y + s * 0.70, s * 0.12,
                0.0, 2.0 * G_PI);
            break;
        case LSM_OVERVIEW_CPU_PRESSURE:
            cairo_move_to(cr, x + s * 0.12, y + s * 0.56);
            cairo_line_to(cr, x + s * 0.28, y + s * 0.56);
            cairo_line_to(cr, x + s * 0.38, y + s * 0.30);
            cairo_line_to(cr, x + s * 0.50, y + s * 0.72);
            cairo_line_to(cr, x + s * 0.61, y + s * 0.42);
            cairo_line_to(cr, x + s * 0.71, y + s * 0.56);
            cairo_line_to(cr, x + s * 0.88, y + s * 0.56);
            break;
        case LSM_OVERVIEW_METRIC_COUNT:
            return FALSE;
    }
    overview_stroke_neon(cr, &colour);
    return FALSE;
}

static GtkWidget *overview_make_resource_icon(
    LsmOverviewMetric metric, gint size)
{
    GtkWidget *icon = gtk_drawing_area_new();
    gtk_widget_set_size_request(icon, size, size);
    gtk_widget_set_hexpand(icon, FALSE);
    gtk_widget_set_vexpand(icon, FALSE);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    g_object_set_data(
        G_OBJECT(icon), "lsm-overview-icon-metric",
        GINT_TO_POINTER((gint)metric + 1));
    g_signal_connect(
        icon, "draw", G_CALLBACK(overview_resource_icon_draw), NULL);
    return icon;
}

static double overview_sample_value(const LsmOverviewSample *sample,
                                    LsmOverviewMetric metric);

static gint overview_graph_height(LsmOverviewMetric metric)
{
    switch (metric) {
        case LSM_OVERVIEW_CPU:
            return 96;
        case LSM_OVERVIEW_MEMORY:
            return 88;
        case LSM_OVERVIEW_DISK:
            return 72;
        case LSM_OVERVIEW_NETWORK:
            return 66;
        case LSM_OVERVIEW_GPU:
            return 60;
        case LSM_OVERVIEW_TEMPERATURE:
            return 50;
        case LSM_OVERVIEW_CPU_PRESSURE:
            return 44;
        case LSM_OVERVIEW_MEMORY_PRESSURE:
            return 48;
        case LSM_OVERVIEW_IO_PRESSURE:
            return 46;
        case LSM_OVERVIEW_METRIC_COUNT:
            break;
    }
    return 82;
}

static gint overview_gauge_size(LsmOverviewMetric metric)
{
    switch (metric) {
        case LSM_OVERVIEW_CPU:
            return 126;
        case LSM_OVERVIEW_MEMORY:
            return 118;
        case LSM_OVERVIEW_GPU:
            return 92;
        case LSM_OVERVIEW_DISK:
        case LSM_OVERVIEW_NETWORK:
        case LSM_OVERVIEW_TEMPERATURE:
        case LSM_OVERVIEW_CPU_PRESSURE:
        case LSM_OVERVIEW_MEMORY_PRESSURE:
        case LSM_OVERVIEW_IO_PRESSURE:
        case LSM_OVERVIEW_METRIC_COUNT:
            return 0;
    }
    return 0;
}

static void overview_gauge_colours(LsmOverviewMetric metric,
                                   GdkRGBA colours[3])
{
    const char *stops[3] = { "#00adef", "#22dcff", "#7f58ff" };
    if (metric == LSM_OVERVIEW_MEMORY) {
        stops[0] = "#6f45ff";
        stops[1] = "#e23cff";
        stops[2] = "#ffad24";
    } else if (metric == LSM_OVERVIEW_GPU) {
        stops[0] = "#16c77a";
        stops[1] = "#18e8c0";
        stops[2] = "#62ef77";
    }
    for (size_t index = 0U; index < 3U; index++) {
        colours[index] = (GdkRGBA){0.0, 0.68, 0.94, 1.0};
        (void)gdk_rgba_parse(&colours[index], stops[index]);
    }
}


static gboolean overview_gauge_draw(GtkWidget *widget, cairo_t *cr,
                                      gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app || !widget || !cr || !app->overview.history)
        return FALSE;

    const gint stored = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(widget), "lsm-overview-gauge-metric"));
    if (stored <= 0 || stored > (gint)LSM_OVERVIEW_METRIC_COUNT)
        return FALSE;
    const LsmOverviewMetric metric = (LsmOverviewMetric)(stored - 1);

    LsmOverviewSample sample;
    const gboolean have_sample =
        lsm_overview_history_latest(app->overview.history, &sample) &&
        !sample.gap;
    const double value = have_sample
        ? overview_sample_value(&sample, metric) : NAN;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    const double width = (double)allocation.width;
    const double height = (double)allocation.height;
    const double cx = width / 2.0;
    const double cy = height / 2.0;
    const double radius = fmax(4.0, fmin(width, height) / 2.0 - 9.0);

    GdkRGBA colours[3];
    overview_gauge_colours(metric, colours);

    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_width(cr, 9.0);
    cairo_set_source_rgba(
        cr, colours[0].red, colours[0].green, colours[0].blue, 0.13);
    cairo_arc(cr, cx, cy, radius, 0.0, 2.0 * G_PI);
    cairo_stroke(cr);

    if (isfinite(value)) {
        const double fraction = fmax(0.0, fmin(1.0, value / 100.0));
        const double start = -G_PI / 2.0;
        const double sweep = fraction * 2.0 * G_PI;
        const int segments = 48;

        cairo_set_line_width(cr, 16.0);
        cairo_set_source_rgba(
            cr, colours[1].red, colours[1].green, colours[1].blue, 0.12);
        cairo_arc(cr, cx, cy, radius, start, start + sweep);
        cairo_stroke(cr);

        cairo_set_line_width(cr, 8.0);
        for (int segment = 0; segment < segments; segment++) {
            const double t0 = (double)segment / (double)segments;
            const double t1 = (double)(segment + 1) / (double)segments;
            if (t0 >= fraction)
                break;
            const double visible_t1 = fmin(t1, fraction);
            const double colour_t = (t0 + visible_t1) * 0.5;
            const int colour_index = colour_t < 0.5 ? 0 : 1;
            const double local_t = colour_t < 0.5
                ? colour_t * 2.0 : (colour_t - 0.5) * 2.0;
            const GdkRGBA *from = &colours[colour_index];
            const GdkRGBA *to = &colours[colour_index + 1];
            const double red = from->red + (to->red - from->red) * local_t;
            const double green =
                from->green + (to->green - from->green) * local_t;
            const double blue =
                from->blue + (to->blue - from->blue) * local_t;

            cairo_set_source_rgba(cr, red, green, blue, 1.0);
            cairo_arc(
                cr, cx, cy, radius,
                start + t0 * 2.0 * G_PI,
                start + visible_t1 * 2.0 * G_PI + 0.012);
            cairo_stroke(cr);
        }
    }

    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(
        cr, colours[1].red, colours[1].green, colours[1].blue, 0.30);
    cairo_arc(cr, cx, cy, radius - 14.0, 0.0, 2.0 * G_PI);
    cairo_stroke(cr);
    return FALSE;
}

static const char *overview_gauge_caption(LsmOverviewMetric metric)
{
    switch (metric) {
        case LSM_OVERVIEW_CPU: return "CPU Usage";
        case LSM_OVERVIEW_MEMORY: return "In Use";
        case LSM_OVERVIEW_GPU: return "GPU Usage";
        case LSM_OVERVIEW_DISK:
        case LSM_OVERVIEW_NETWORK:
        case LSM_OVERVIEW_TEMPERATURE:
        case LSM_OVERVIEW_CPU_PRESSURE:
        case LSM_OVERVIEW_MEMORY_PRESSURE:
        case LSM_OVERVIEW_IO_PRESSURE:
        case LSM_OVERVIEW_METRIC_COUNT:
            return "";
    }
    return "";
}

static GtkWidget *overview_make_gauge(LsmApp *app, LsmOverviewMetric metric,
                                      GtkWidget *value)
{
    if (!app || !value ||
        (metric != LSM_OVERVIEW_CPU &&
         metric != LSM_OVERVIEW_MEMORY &&
         metric != LSM_OVERVIEW_GPU))
        return NULL;

    GtkWidget *area = gtk_drawing_area_new();
    const gint gauge_size = overview_gauge_size(metric);
    gtk_widget_set_size_request(area, gauge_size, gauge_size);
    gtk_widget_set_hexpand(area, FALSE);
    gtk_widget_set_vexpand(area, FALSE);
    gtk_widget_set_valign(area, GTK_ALIGN_CENTER);
    g_object_set_data(
        G_OBJECT(area), "lsm-overview-gauge-metric",
        GINT_TO_POINTER((gint)metric + 1));
    g_signal_connect(area, "draw", G_CALLBACK(overview_gauge_draw), app);

    GtkWidget *overlay = gtk_overlay_new();
    gtk_widget_set_size_request(overlay, gauge_size, gauge_size);
    gtk_widget_set_hexpand(overlay, FALSE);
    gtk_widget_set_vexpand(overlay, FALSE);
    gtk_widget_set_valign(overlay, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(overlay), area);

    GtkWidget *centre = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(centre, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(centre, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(value, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(value), "lsm-overview-gauge-value");
    GtkWidget *caption = gtk_label_new(overview_gauge_caption(metric));
    gtk_widget_set_halign(caption, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(caption), "lsm-overview-gauge-caption");
    gtk_box_pack_start(GTK_BOX(centre), value, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(centre), caption, FALSE, FALSE, 0);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), centre);

    g_object_set_data(G_OBJECT(overlay), "lsm-overview-gauge-area", area);
    return overlay;
}

static double overview_sample_value(const LsmOverviewSample *sample,
                                    LsmOverviewMetric metric)
{
    if (!sample || sample->gap) return NAN;
    switch (metric) {
        case LSM_OVERVIEW_CPU:
            return sample->cpu_available ? sample->cpu_percent : NAN;
        case LSM_OVERVIEW_MEMORY:
            return sample->memory_available ? sample->memory_percent : NAN;
        case LSM_OVERVIEW_DISK:
            return sample->disk_available ? sample->disk_percent : NAN;
        case LSM_OVERVIEW_NETWORK:
            return sample->network_available
                ? sample->network_bytes_per_sec : NAN;
        case LSM_OVERVIEW_GPU:
            return sample->gpu_available ? sample->gpu_percent : NAN;
        case LSM_OVERVIEW_TEMPERATURE:
            return sample->temperature_available
                ? sample->temperature_c : NAN;
        case LSM_OVERVIEW_CPU_PRESSURE:
            return sample->cpu_pressure_available
                ? sample->cpu_pressure_percent : NAN;
        case LSM_OVERVIEW_MEMORY_PRESSURE:
            return sample->memory_pressure_available
                ? sample->memory_pressure_percent : NAN;
        case LSM_OVERVIEW_IO_PRESSURE:
            return sample->io_pressure_available
                ? sample->io_pressure_percent : NAN;
        case LSM_OVERVIEW_METRIC_COUNT:
            break;
    }
    return NAN;
}

static void overview_push_sample(LsmApp *app,
                                 const LsmOverviewSample *sample)
{
    if (!app || !sample) return;
    for (size_t metric = 0U; metric < LSM_OVERVIEW_METRIC_COUNT; metric++) {
        LsmGraph *graph = app->overview.graphs[metric];
        if (!graph) continue;

        if ((LsmOverviewMetric)metric == LSM_OVERVIEW_CPU) {
            const bool split_available =
                !sample->gap && sample->cpu_breakdown_available;
            lsm_graph_push(
                graph,
                split_available ? sample->cpu_user_percent : NAN,
                split_available ? sample->cpu_kernel_percent : NAN,
                app->runtime.newer_on_right);
            continue;
        }
        if ((LsmOverviewMetric)metric == LSM_OVERVIEW_MEMORY) {
            lsm_graph_push(
                graph,
                !sample->gap && sample->memory_available
                    ? sample->memory_percent : NAN,
                !sample->gap && sample->memory_breakdown_available
                    ? sample->memory_available_percent : NAN,
                app->runtime.newer_on_right);
            continue;
        }
        if ((LsmOverviewMetric)metric == LSM_OVERVIEW_NETWORK) {
            lsm_graph_push(
                graph,
                !sample->gap && sample->network_available
                    ? sample->network_receive_bytes_per_sec : NAN,
                !sample->gap && sample->network_available
                    ? sample->network_send_bytes_per_sec : NAN,
                app->runtime.newer_on_right);
            continue;
        }

        lsm_graph_push(
            graph,
            overview_sample_value(sample, (LsmOverviewMetric)metric),
            NAN, app->runtime.newer_on_right);
    }
}

static const char *overview_stat_key(gint index)
{
    static const char *const keys[4] = {
        "lsm-overview-stat-0", "lsm-overview-stat-1",
        "lsm-overview-stat-2", "lsm-overview-stat-3"
    };
    return index >= 0 && index < 4 ? keys[index] : NULL;
}

static gboolean overview_stat_icon_draw(
    GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    (void)user_data;
    const gint code = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(widget), "lsm-overview-stat-code"));
    const LsmOverviewMetric metric =
        (LsmOverviewMetric)(code / 10 - 1);
    const gint index = code % 10;
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    const double w = allocation.width;
    const double h = allocation.height;
    GdkRGBA colour = {0.15, 0.85, 1.0, 1.0};
    static const char *const memory_colours[4] = {
        "#d63cff", "#ffb51b", "#23cfff", "#9ecbff"
    };
    if (metric == LSM_OVERVIEW_MEMORY && index >= 0 && index < 4)
        (void)gdk_rgba_parse(&colour, memory_colours[index]);
    else if (metric == LSM_OVERVIEW_CPU && index == 3)
        (void)gdk_rgba_parse(&colour, "#ff4fc5");
    else
        (void)gdk_rgba_parse(&colour, "#39d9ff");

    cairo_set_source_rgba(
        cr, colour.red, colour.green, colour.blue, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    if (metric == LSM_OVERVIEW_MEMORY) {
        cairo_arc(cr, w / 2.0, h / 2.0, fmin(w, h) * 0.22,
                  0.0, 2.0 * G_PI);
        cairo_fill(cr);
        return FALSE;
    }

    cairo_new_path(cr);
    if (index == 0) {
        cairo_move_to(cr, w * 0.08, h * 0.55);
        cairo_line_to(cr, w * 0.28, h * 0.55);
        cairo_line_to(cr, w * 0.40, h * 0.28);
        cairo_line_to(cr, w * 0.55, h * 0.74);
        cairo_line_to(cr, w * 0.68, h * 0.42);
        cairo_line_to(cr, w * 0.80, h * 0.55);
        cairo_line_to(cr, w * 0.94, h * 0.55);
    } else if (index == 1) {
        cairo_rectangle(cr, w * 0.25, h * 0.25, w * 0.50, h * 0.50);
        cairo_rectangle(cr, w * 0.39, h * 0.39, w * 0.22, h * 0.22);
    } else if (index == 2) {
        for (int layer = 0; layer < 3; layer++)
            cairo_rectangle(
                cr, w * (0.20 + 0.06 * layer),
                h * (0.28 + 0.16 * layer), w * 0.56, h * 0.10);
    } else {
        cairo_move_to(cr, w * 0.50, h * 0.16);
        cairo_line_to(cr, w * 0.50, h * 0.62);
        cairo_arc(cr, w * 0.50, h * 0.72, w * 0.12,
                  0.0, 2.0 * G_PI);
    }
    cairo_stroke(cr);
    return FALSE;
}

static GtkWidget *overview_make_stat_cell(
    LsmOverviewMetric metric, gint index, const char *caption)
{
    GtkWidget *cell = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(cell), "lsm-overview-stat");

    GtkWidget *icon = gtk_drawing_area_new();
    gtk_widget_set_size_request(icon, 28, 28);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    g_object_set_data(
        G_OBJECT(icon), "lsm-overview-stat-code",
        GINT_TO_POINTER(((gint)metric + 1) * 10 + index));
    g_signal_connect(
        icon, "draw", G_CALLBACK(overview_stat_icon_draw), NULL);

    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *value = gtk_label_new("N/A");
    gtk_widget_set_halign(value, GTK_ALIGN_START);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(value), "lsm-overview-stat-value");
    GtkWidget *caption_label = gtk_label_new(caption);
    gtk_widget_set_halign(caption_label, GTK_ALIGN_START);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(caption_label),
        "lsm-overview-stat-caption");
    gtk_box_pack_start(GTK_BOX(text), value, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(text), caption_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cell), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cell), text, TRUE, TRUE, 0);
    g_object_set_data(G_OBJECT(cell), "lsm-overview-stat-value", value);
    return cell;
}

static GtkWidget *overview_make_primary_stats(
    GtkWidget *button, LsmOverviewMetric metric)
{
    if (!button ||
        (metric != LSM_OVERVIEW_CPU && metric != LSM_OVERVIEW_MEMORY))
        return NULL;
    static const char *const cpu_captions[4] = {
        "Max Frequency", "Cores", "Threads", "Temperature"
    };
    static const char *const memory_captions[4] = {
        "Used", "Cached", "Available", "Swap"
    };
    const char *const *captions =
        metric == LSM_OVERVIEW_CPU ? cpu_captions : memory_captions;

    GtkWidget *row = gtk_grid_new();
    gtk_style_context_add_class(
        gtk_widget_get_style_context(row), "lsm-overview-stat-row");
    gtk_grid_set_column_spacing(GTK_GRID(row), 10);
    gtk_widget_set_hexpand(row, TRUE);
    for (gint column = 0; column < 4; column++) {
        GtkWidget *cell = overview_make_stat_cell(
            metric, column, captions[column]);
        gtk_widget_set_hexpand(cell, TRUE);
        gtk_grid_attach(GTK_GRID(row), cell, column, 0, 1, 1);
        g_object_set_data(
            G_OBJECT(button), overview_stat_key(column),
            g_object_get_data(G_OBJECT(cell), "lsm-overview-stat-value"));
    }
    return row;
}

static GtkWidget *overview_card_data_widget(
    LsmApp *app, LsmOverviewMetric metric, const char *key)
{
    if (!app || metric >= LSM_OVERVIEW_METRIC_COUNT || !key)
        return NULL;
    GtkWidget *button = app->overview.buttons[metric];
    return button ? g_object_get_data(G_OBJECT(button), key) : NULL;
}

static GtkWidget *overview_make_card(LsmApp *app, LsmOverviewMetric metric)
{
    GtkWidget *button = gtk_button_new();
    gtk_widget_set_hexpand(button, TRUE);
    gtk_widget_set_vexpand(button, TRUE);
    gtk_widget_set_name(button, "lsm-overview-card");
    gtk_widget_set_tooltip_text(button, "Open detailed performance view");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(button), "lsm-performance-card");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(button), "lsm-overview-card");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(button), overview_style_classes[metric]);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 10);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *header_icon = overview_make_resource_icon(metric, 24);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(header_icon), "lsm-overview-icon");

    GtkWidget *title = gtk_label_new(overview_titles[metric]);
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(title), "lsm-performance-title");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(title), "lsm-overview-card-title");

    GtkWidget *value = gtk_label_new("Initialising…");
    gtk_widget_set_halign(value, GTK_ALIGN_END);
    /*
     * Live headline values must not resize the graph beside them, but their
     * stable allocation must also stay local to that card. Character-width
     * requests fed large natural sizes into the shared grid in 1.0.112 and,
     * together with homogeneous columns, made the whole window wider than the
     * desktop. Small fixed pixel reservations prevent both behaviours.
     */
    if (metric != LSM_OVERVIEW_CPU &&
        metric != LSM_OVERVIEW_MEMORY &&
        metric != LSM_OVERVIEW_GPU) {
        const gint value_width =
            metric == LSM_OVERVIEW_NETWORK ? 108 :
            metric == LSM_OVERVIEW_TEMPERATURE ? 76 : 72;
        gtk_widget_set_size_request(value, value_width, -1);
        gtk_label_set_ellipsize(GTK_LABEL(value), PANGO_ELLIPSIZE_END);
    }
    gtk_style_context_add_class(
        gtk_widget_get_style_context(value), "lsm-metric-value");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(value), "lsm-overview-value");

    GtkWidget *meta = gtk_label_new("");
    gtk_widget_set_halign(meta, GTK_ALIGN_END);
    gtk_label_set_ellipsize(GTK_LABEL(meta), PANGO_ELLIPSIZE_END);
    if (metric == LSM_OVERVIEW_CPU ||
        metric == LSM_OVERVIEW_MEMORY ||
        metric == LSM_OVERVIEW_DISK ||
        metric == LSM_OVERVIEW_NETWORK ||
        metric == LSM_OVERVIEW_GPU)
        gtk_widget_set_size_request(meta, 120, -1);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(meta), "lsm-overview-card-meta");
    GtkWidget *chevron = gtk_image_new_from_icon_name(
        "go-next-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_image_set_pixel_size(GTK_IMAGE(chevron), 14);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(chevron), "lsm-overview-chevron");

    gtk_box_pack_start(GTK_BOX(header), header_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(header), title, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(header), chevron, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(header), meta, FALSE, FALSE, 0);
    if (metric != LSM_OVERVIEW_CPU &&
        metric != LSM_OVERVIEW_MEMORY &&
        metric != LSM_OVERVIEW_GPU)
        gtk_box_pack_end(GTK_BOX(header), value, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(button), "lsm-overview-meta", meta);

    GtkWidget *detail = gtk_label_new("");
    gtk_widget_set_halign(detail, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(detail), PANGO_ELLIPSIZE_END);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(detail), "lsm-performance-summary");

    const gboolean percentage =
        metric != LSM_OVERVIEW_NETWORK &&
        metric != LSM_OVERVIEW_TEMPERATURE;
    const double maximum =
        metric == LSM_OVERVIEW_TEMPERATURE ? 120.0 :
        (percentage ? 100.0 : 0.0);
    const gboolean dual_series =
        metric == LSM_OVERVIEW_CPU ||
        metric == LSM_OVERVIEW_MEMORY ||
        metric == LSM_OVERVIEW_NETWORK;
    LsmGraph *graph = lsm_graph_new(
        dual_series, percentage, maximum, 220, overview_graph_height(metric));
    if (graph) {
        const char *primary = overview_colour(metric);
        const char *secondary = NULL;
        switch (metric) {
            case LSM_OVERVIEW_CPU:
                primary = "#00d9ff";
                secondary = "#8b35ff";
                break;
            case LSM_OVERVIEW_MEMORY:
                primary = "#e33cff";
                secondary = "#1bcfff";
                break;
            case LSM_OVERVIEW_DISK:
                primary = "#24e38b";
                break;
            case LSM_OVERVIEW_NETWORK:
                primary = "#18ceff";
                secondary = "#ef3bff";
                break;
            case LSM_OVERVIEW_GPU:
                primary = "#18e89a";
                break;
            case LSM_OVERVIEW_TEMPERATURE:
                primary = "#ff9d1c";
                break;
            case LSM_OVERVIEW_CPU_PRESSURE:
                primary = "#1bcfff";
                break;
            case LSM_OVERVIEW_MEMORY_PRESSURE:
                primary = "#a33cff";
                break;
            case LSM_OVERVIEW_IO_PRESSURE:
                primary = "#43e35d";
                break;
            case LSM_OVERVIEW_METRIC_COUNT:
                break;
        }
        lsm_graph_set_colours(graph, primary, secondary);
        lsm_graph_set_smooth(graph, TRUE);
        lsm_graph_set_secondary_dashed(graph, FALSE);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(graph->area), "lsm-overview-graph");
        if (metric == LSM_OVERVIEW_NETWORK)
            lsm_graph_set_dynamic_scale(graph, 1000000.0, 1000000.0);
    }

    gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);

    GtkWidget *visual = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *gauge = overview_make_gauge(app, metric, value);
    if (gauge)
        gtk_box_pack_start(GTK_BOX(visual), gauge, FALSE, FALSE, 0);
    if (graph)
        gtk_box_pack_start(GTK_BOX(visual), graph->area, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), visual, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(box), detail, FALSE, FALSE, 0);
    GtkWidget *stats = overview_make_primary_stats(button, metric);
    if (stats)
        gtk_box_pack_start(GTK_BOX(box), stats, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button), box);

    app->overview.buttons[metric] = button;
    app->overview.values[metric] = value;
    app->overview.details[metric] = detail;
    app->overview.graphs[metric] = graph;
    app->overview.gauges[metric] = gauge
        ? g_object_get_data(G_OBJECT(gauge), "lsm-overview-gauge-area")
        : NULL;
    g_object_set_data(
        G_OBJECT(button), "lsm-overview-metric",
        GINT_TO_POINTER((gint)metric + 1));
    return button;
}

static bool overview_destination(LsmApp *app, LsmOverviewMetric metric,
                                 LsmPageType *type, size_t *index)
{
    if (!app || !type || !index || !app->overview.history)
        return false;
    LsmOverviewSample sample;
    if (!lsm_overview_history_latest(app->overview.history, &sample) ||
        sample.gap)
        return false;

    switch (metric) {
        case LSM_OVERVIEW_CPU:
        case LSM_OVERVIEW_CPU_PRESSURE:
            *type = LSM_PAGE_CPU;
            *index = 0U;
            return true;
        case LSM_OVERVIEW_MEMORY:
        case LSM_OVERVIEW_MEMORY_PRESSURE:
            *type = LSM_PAGE_MEMORY;
            *index = 0U;
            return true;
        case LSM_OVERVIEW_DISK:
        case LSM_OVERVIEW_IO_PRESSURE:
            *type = LSM_PAGE_DISK;
            *index = 0U;
            return app->monitor.disk_count > 0U;
        case LSM_OVERVIEW_NETWORK:
            *type = LSM_PAGE_NETWORK;
            *index = lsm_overview_resolve_network(&app->monitor, &sample);
            return *index != SIZE_MAX;
        case LSM_OVERVIEW_GPU:
            *type = LSM_PAGE_GPU;
            *index = lsm_overview_resolve_gpu(&app->monitor, &sample);
            return *index != SIZE_MAX;
        case LSM_OVERVIEW_TEMPERATURE:
            if (sample.temperature_source == LSM_OVERVIEW_TEMPERATURE_CPU) {
                *type = LSM_PAGE_CPU;
                *index = 0U;
                return true;
            }
            if (sample.temperature_source == LSM_OVERVIEW_TEMPERATURE_GPU) {
                *type = LSM_PAGE_GPU;
                LsmOverviewSample gpu_sample = sample;
                gpu_sample.gpu_available = true;
                gpu_sample.gpu_index = sample.temperature_gpu_index;
                memcpy(
                    gpu_sample.gpu_identity, sample.temperature_identity,
                    sizeof(gpu_sample.gpu_identity));
                memcpy(
                    gpu_sample.gpu_name, sample.temperature_name,
                    sizeof(gpu_sample.gpu_name));
                *index = lsm_overview_resolve_gpu(
                    &app->monitor, &gpu_sample);
                return *index != SIZE_MAX;
            }
            return false;
        case LSM_OVERVIEW_METRIC_COUNT:
            return false;
    }
    return false;
}

static void overview_card_clicked(GtkButton *button, gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app || !button) return;
    const gint stored = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "lsm-overview-metric"));
    if (stored <= 0 || stored > (gint)LSM_OVERVIEW_METRIC_COUNT)
        return;

    LsmPageType type;
    size_t index = 0U;
    if (!overview_destination(
            app, (LsmOverviewMetric)(stored - 1), &type, &index))
        return;
    lsm_performance_show_resource(app, type, index);
}

static void overview_set_percent(GtkWidget *label, bool available,
                                 double value)
{
    char text[64];
    lsm_metric_format_percent(available, value, text, sizeof(text));
    lsm_ui_set_label_text(label, "%s", text);
}

static void overview_set_pressure(GtkWidget *label, bool available,
                                  double value)
{
    if (!available || !isfinite(value)) {
        lsm_ui_set_label_text(label, "N/A");
        return;
    }

    /*
     * Linux PSI is commonly well below one percent on a healthy workstation.
     * The ordinary whole-percent formatter made genuine 0.01-0.49%% stalls
     * look permanently zero, so preserve the precision supplied by procfs.
     */
    lsm_ui_set_label_text(label, "%.2f%%", value);
}

static void overview_set_latest_values(LsmApp *app,
                                       const LsmOverviewSample *sample)
{
    if (!app || !sample) return;

    overview_set_percent(
        app->overview.values[LSM_OVERVIEW_CPU],
        sample->cpu_available, sample->cpu_percent);
    lsm_ui_set_label_text(
        overview_card_data_widget(app, LSM_OVERVIEW_CPU, "lsm-overview-meta"),
        "%s", app->monitor.cpu.model[0] ? app->monitor.cpu.model : "Processor");
    if (sample->cpu_breakdown_available) {
        lsm_ui_set_label_text(
            app->overview.details[LSM_OVERVIEW_CPU],
            "User %.1f%% · Kernel %.1f%%",
            sample->cpu_user_percent, sample->cpu_kernel_percent);
    } else {
        lsm_ui_set_label_text(
            app->overview.details[LSM_OVERVIEW_CPU],
            "User/kernel split unavailable");
    }
    {
        char speed[48], temperature_text[48], cores[32], threads[32];
        if (isfinite(app->monitor.cpu.max_frequency_ghz) &&
            app->monitor.cpu.max_frequency_ghz > 0.0)
            snprintf(speed, sizeof(speed), "%.2f GHz",
                     app->monitor.cpu.max_frequency_ghz);
        else
            snprintf(speed, sizeof(speed), "N/A");
        lsm_metric_format_celsius(
            app->monitor.cpu.temperature_available,
            app->monitor.cpu.temperature_c,
            temperature_text, sizeof(temperature_text));
        snprintf(cores, sizeof(cores), "%u", app->monitor.cpu.physical_cores);
        snprintf(threads, sizeof(threads), "%u", app->monitor.cpu.logical_cores);
        const char *values[4] = {
            speed,
            app->monitor.cpu.physical_cores ? cores : "N/A",
            app->monitor.cpu.logical_cores ? threads : "N/A",
            temperature_text
        };
        for (gint index = 0; index < 4; index++)
            lsm_ui_set_label_text(
                overview_card_data_widget(
                    app, LSM_OVERVIEW_CPU, overview_stat_key(index)),
                "%s", values[index]);
    }

    overview_set_percent(
        app->overview.values[LSM_OVERVIEW_MEMORY],
        sample->memory_available, sample->memory_percent);
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_MEMORY],
        "Physical memory in use");
    {
        char total[64], used[64], cached[64], available[64];
        char swap_used[64], swap_total[64], swap[144], identity[192];
        lsm_format_bytes(app->monitor.memory.total_bytes, total, sizeof(total));
        lsm_format_bytes(app->monitor.memory.used_bytes, used, sizeof(used));
        lsm_format_bytes(app->monitor.memory.cached_bytes, cached, sizeof(cached));
        lsm_format_bytes(
            app->monitor.memory.available_bytes, available, sizeof(available));
        lsm_format_bytes(
            app->monitor.memory.swap_used_bytes, swap_used, sizeof(swap_used));
        lsm_format_bytes(
            app->monitor.memory.swap_total_bytes, swap_total, sizeof(swap_total));
        snprintf(swap, sizeof(swap), "%s / %s", swap_used, swap_total);
        const char *kind =
            app->monitor.memory.module_count > 0U &&
            app->monitor.memory.modules[0].memory_type[0]
                ? app->monitor.memory.modules[0].memory_type : "";
        if (kind[0])
            snprintf(identity, sizeof(identity), "%s %s", total, kind);
        else
            snprintf(identity, sizeof(identity), "%s", total);
        lsm_ui_set_label_text(
            overview_card_data_widget(
                app, LSM_OVERVIEW_MEMORY, "lsm-overview-meta"),
            "%s", identity);
        const char *values[4] = { used, cached, available, swap };
        for (gint index = 0; index < 4; index++)
            lsm_ui_set_label_text(
                overview_card_data_widget(
                    app, LSM_OVERVIEW_MEMORY, overview_stat_key(index)),
                "%s", values[index]);
    }

    overview_set_percent(
        app->overview.values[LSM_OVERVIEW_DISK],
        sample->disk_available, sample->disk_percent);
    if (sample->disk_available) {
        char read_rate[64];
        char write_rate[64];
        char detail[512];
        lsm_metric_format_network(
            sample->disk_read_bytes_per_sec, false, true,
            read_rate, sizeof(read_rate));
        lsm_metric_format_network(
            sample->disk_write_bytes_per_sec, false, true,
            write_rate, sizeof(write_rate));
        snprintf(
            detail, sizeof(detail), "%zu physical disk%s — R %s / W %s",
            app->monitor.disk_count,
            app->monitor.disk_count == 1U ? "" : "s",
            read_rate, write_rate);
        lsm_ui_set_label_text(
            app->overview.details[LSM_OVERVIEW_DISK], "%s", detail);
        lsm_ui_set_label_text(
            overview_card_data_widget(
                app, LSM_OVERVIEW_DISK, "lsm-overview-meta"),
            "%zu physical disk%s", app->monitor.disk_count,
            app->monitor.disk_count == 1U ? "" : "s");
    } else {
        lsm_ui_set_label_text(
            app->overview.details[LSM_OVERVIEW_DISK],
            "No measured disk activity");
    }

    if (sample->network_available) {
        char rate[64];
        char detail[512];
        lsm_metric_format_network(
            sample->network_bytes_per_sec, app->runtime.network_use_bits,
            true, rate, sizeof(rate));
        lsm_ui_set_label_text(
            app->overview.values[LSM_OVERVIEW_NETWORK], "%s", rate);
        snprintf(
            detail, sizeof(detail), "%s — busiest adapter",
            sample->network_name[0]
                ? sample->network_name : "Network");
        lsm_ui_set_label_text(
            app->overview.details[LSM_OVERVIEW_NETWORK], "%s", detail);
        lsm_ui_set_label_text(
            overview_card_data_widget(
                app, LSM_OVERVIEW_NETWORK, "lsm-overview-meta"),
            "%s", sample->network_name[0] ? sample->network_name : "Network");
    } else {
        lsm_ui_set_label_text(
            app->overview.values[LSM_OVERVIEW_NETWORK], "N/A");
        lsm_ui_set_label_text(
            app->overview.details[LSM_OVERVIEW_NETWORK],
            "No measured network traffic");
    }

    overview_set_percent(
        app->overview.values[LSM_OVERVIEW_GPU],
        sample->gpu_available, sample->gpu_percent);
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_GPU], "%s",
        sample->gpu_available && sample->gpu_name[0]
            ? sample->gpu_name : "GPU telemetry unavailable");
    lsm_ui_set_label_text(
        overview_card_data_widget(app, LSM_OVERVIEW_GPU, "lsm-overview-meta"),
        "%s", sample->gpu_name[0] ? sample->gpu_name : "Graphics");

    char temperature[64];
    lsm_metric_format_celsius(
        sample->temperature_available, sample->temperature_c,
        temperature, sizeof(temperature));
    lsm_ui_set_label_text(
        app->overview.values[LSM_OVERVIEW_TEMPERATURE],
        "%s", temperature);
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_TEMPERATURE], "%s",
        sample->temperature_available && sample->temperature_name[0]
            ? sample->temperature_name : "Temperature telemetry unavailable");

    overview_set_pressure(
        app->overview.values[LSM_OVERVIEW_CPU_PRESSURE],
        sample->cpu_pressure_available, sample->cpu_pressure_percent);
    overview_set_pressure(
        app->overview.values[LSM_OVERVIEW_MEMORY_PRESSURE],
        sample->memory_pressure_available, sample->memory_pressure_percent);
    overview_set_pressure(
        app->overview.values[LSM_OVERVIEW_IO_PRESSURE],
        sample->io_pressure_available, sample->io_pressure_percent);
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_CPU_PRESSURE],
        "Kernel PSI · 10 s runnable-work stall average");
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_MEMORY_PRESSURE],
        "Kernel PSI · 10 s memory stall average");
    lsm_ui_set_label_text(
        app->overview.details[LSM_OVERVIEW_IO_PRESSURE],
        "Kernel PSI · 10 s I/O stall average");

    GtkWidget *page = app->runtime.page_containers[LSM_TAB_OVERVIEW];
    GtkWidget *uptime_widget = page
        ? g_object_get_data(G_OBJECT(page), "lsm-overview-uptime") : NULL;
    if (uptime_widget) {
        char uptime[128], block[176];
        if (!lsm_temporal_format_elapsed_seconds(
                app->monitor.cpu.uptime_seconds, uptime, sizeof(uptime)))
            snprintf(uptime, sizeof(uptime), "N/A");
        snprintf(block, sizeof(block), "Uptime\n%s", uptime);
        lsm_ui_set_label_text(uptime_widget, "%s", block);
    }
    GtkWidget *status = page
        ? g_object_get_data(G_OBJECT(page), "lsm-overview-status") : NULL;
    if (status) {
        GtkStyleContext *style = gtk_widget_get_style_context(status);
        gtk_style_context_remove_class(style, "lsm-status-warning");
        gtk_style_context_remove_class(style, "lsm-status-fault");
        if (sample->temperature_available && sample->temperature_c >= 95.0) {
            lsm_ui_set_label_text(status, "●  Thermal Fault");
            gtk_style_context_add_class(style, "lsm-status-fault");
        } else if (sample->temperature_available &&
                   sample->temperature_c >= 80.0) {
            lsm_ui_set_label_text(status, "●  Thermal Warning");
            gtk_style_context_add_class(style, "lsm-status-warning");
        } else {
            lsm_ui_set_label_text(status, "●  All Systems Nominal");
        }
    }
}

static void overview_view_all_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    if (!app || !app->shell.notebook) return;
    lsm_app_ensure_page_built(app, LSM_TAB_PROCESSES);
    gtk_notebook_set_current_page(
        GTK_NOTEBOOK(app->shell.notebook), LSM_TAB_PROCESSES);
}

static void overview_refresh_processes(LsmApp *app)
{
    if (!app) return;
    size_t indices[LSM_OVERVIEW_TOP_PROCESS_COUNT];
    const size_t selected = lsm_overview_top_cpu_processes(
        app->process.process_snapshot,
        app->process.process_snapshot_count, indices);

    for (size_t row = 0U; row < LSM_OVERVIEW_TOP_PROCESS_COUNT; row++) {
        GtkWidget *container = app->overview.process_rows[row];
        GtkWidget *name = app->overview.process_names[row];
        GtkWidget *bar = app->overview.process_cpu_bars[row];
        GtkWidget *cpu = app->overview.process_cpu_values[row];
        GtkWidget *memory = app->overview.process_memory_values[row];
        if (!container || !name || !bar || !cpu || !memory) continue;

        if (row >= selected || indices[row] == SIZE_MAX) {
            gtk_widget_set_visible(container, FALSE);
            continue;
        }

        const LsmProcessInfo *process =
            &app->process.process_snapshot[indices[row]];
        char cpu_text[64];
        char memory_text[64];
        snprintf(cpu_text, sizeof(cpu_text), "%.1f%% CPU", process->cpu_percent);
        lsm_format_bytes(process->rss_bytes, memory_text, sizeof(memory_text));
        lsm_ui_set_label_text(
            name, "%s", process->name[0] ? process->name : "Unnamed process");
        lsm_ui_set_label_text(cpu, "%s", cpu_text);
        lsm_ui_set_label_text(memory, "%s", memory_text);
        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR(bar),
            fmax(0.0, fmin(1.0, process->cpu_percent / 100.0)));
        gtk_widget_set_visible(container, TRUE);
    }
}

void lsm_overview_build(LsmApp *app, GtkWidget *container)
{
    if (!app || !container) return;
    if (!app->overview.history)
        app->overview.history = lsm_overview_history_create();

    /* Overview is a dashboard, not a document.  It must consume the available
     * viewport as one composition rather than growing a scrollable canvas. */
    app->runtime.page_scrollers[LSM_TAB_OVERVIEW] = NULL;

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_name(root, "lsm-overview-root");
    gtk_widget_set_hexpand(root, TRUE);
    gtk_widget_set_vexpand(root, TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(root), 10);
    gtk_box_pack_start(GTK_BOX(container), root, TRUE, TRUE, 0);

    GtkWidget *hero = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_name(hero, "lsm-overview-hero");
    gtk_widget_set_hexpand(hero, TRUE);

    GtkWidget *brand_mark = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(brand_mark, "lsm-overview-brand-mark");
    GtkWidget *brand_icon =
        overview_make_resource_icon(LSM_OVERVIEW_CPU_PRESSURE, 38);
    gtk_container_add(GTK_CONTAINER(brand_mark), brand_icon);
    gtk_widget_set_valign(brand_mark, GTK_ALIGN_CENTER);

    GtkWidget *hero_text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(hero_text, TRUE);
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(
        GTK_LABEL(title),
        "<span size='22000' weight='bold'>System Monitor</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    GtkWidget *subtitle = gtk_label_new(
        "Live performance, health and activity at a glance");
    gtk_widget_set_halign(subtitle, GTK_ALIGN_START);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(subtitle), "lsm-performance-summary");
    gtk_box_pack_start(GTK_BOX(hero_text), title, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hero_text), subtitle, FALSE, FALSE, 0);

    GtkWidget *status = gtk_label_new("●  All Systems Nominal");
    gtk_widget_set_name(status, "lsm-overview-live");
    gtk_widget_set_halign(status, GTK_ALIGN_END);
    gtk_widget_set_valign(status, GTK_ALIGN_CENTER);

    GtkWidget *uptime = gtk_label_new("Uptime\nN/A");
    gtk_widget_set_name(uptime, "lsm-overview-uptime");
    gtk_widget_set_halign(uptime, GTK_ALIGN_END);
    gtk_widget_set_valign(uptime, GTK_ALIGN_CENTER);

    gtk_box_pack_start(GTK_BOX(hero), brand_mark, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hero), hero_text, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(hero), uptime, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hero), status, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root), hero, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(container), "lsm-overview-status", status);
    g_object_set_data(G_OBJECT(container), "lsm-overview-uptime", uptime);

    GtkWidget *grid = gtk_grid_new();
    /*
     * Keep the asymmetric twelve-column layout content-flexible. Homogeneous
     * columns amplify the minimum width of narrow two-column cards across all
     * twelve columns, which can force a maximised window wider than the work
     * area. Stable live-label allocations above stop graph breathing without
     * turning the grid itself into a minimum-width multiplier.
     */
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_hexpand(grid, TRUE);
    gtk_widget_set_vexpand(grid, TRUE);
    for (size_t slot = 0U; slot < LSM_OVERVIEW_METRIC_COUNT; slot++) {
        const LsmOverviewPlacement *placement = &overview_layout[slot];
        GtkWidget *card = overview_make_card(app, placement->metric);
        g_signal_connect(
            card, "clicked", G_CALLBACK(overview_card_clicked), app);
        gtk_grid_attach(
            GTK_GRID(grid), card,
            placement->column, placement->row, placement->width, 1);
    }
    gtk_box_pack_start(GTK_BOX(root), grid, TRUE, TRUE, 0);

    GtkWidget *process_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_widget_set_name(process_card, "lsm-overview-process-card");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(process_card), "lsm-performance-card");

    GtkWidget *process_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
    GtkWidget *process_icon =
        overview_make_resource_icon(LSM_OVERVIEW_CPU_PRESSURE, 21);
    GtkWidget *process_title = gtk_label_new("Top CPU processes");
    gtk_widget_set_halign(process_title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(process_title, TRUE);
    gtk_style_context_add_class(
        gtk_widget_get_style_context(process_title),
        "lsm-performance-title");
    GtkWidget *view_all = gtk_button_new_with_label("View All");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(view_all), "lsm-overview-view-all");
    g_signal_connect(
        view_all, "clicked", G_CALLBACK(overview_view_all_clicked), app);
    gtk_box_pack_start(
        GTK_BOX(process_header), process_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(
        GTK_BOX(process_header), process_title, TRUE, TRUE, 0);
    gtk_box_pack_end(
        GTK_BOX(process_header), view_all, FALSE, FALSE, 0);
    gtk_box_pack_start(
        GTK_BOX(process_card), process_header, FALSE, FALSE, 0);
    for (size_t row = 0U; row < LSM_OVERVIEW_TOP_PROCESS_COUNT; row++) {
        GtkWidget *process_row =
            gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(process_row),
            "lsm-overview-process-row");

        GtkWidget *name = gtk_label_new("");
        gtk_widget_set_halign(name, GTK_ALIGN_START);
        gtk_widget_set_size_request(name, 210, -1);
        gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(name),
            "lsm-overview-process-name");

        GtkWidget *bar = gtk_progress_bar_new();
        gtk_widget_set_hexpand(bar, TRUE);
        gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);
        gtk_widget_set_size_request(bar, 220, 7);
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(bar), FALSE);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(bar),
            "lsm-overview-process-bar");

        GtkWidget *cpu = gtk_label_new("");
        gtk_widget_set_halign(cpu, GTK_ALIGN_END);
        gtk_widget_set_size_request(cpu, 86, -1);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(cpu),
            "lsm-overview-process-cpu");

        GtkWidget *memory = gtk_label_new("");
        gtk_widget_set_halign(memory, GTK_ALIGN_END);
        gtk_widget_set_size_request(memory, 92, -1);
        gtk_style_context_add_class(
            gtk_widget_get_style_context(memory),
            "lsm-overview-process-memory");

        gtk_box_pack_start(GTK_BOX(process_row), name, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(process_row), bar, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(process_row), cpu, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(process_row), memory, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(process_card), process_row, FALSE, FALSE, 0);

        app->overview.process_rows[row] = process_row;
        app->overview.process_names[row] = name;
        app->overview.process_cpu_bars[row] = bar;
        app->overview.process_cpu_values[row] = cpu;
        app->overview.process_memory_values[row] = memory;
    }
    gtk_box_pack_start(GTK_BOX(root), process_card, FALSE, FALSE, 0);

    if (app->overview.history) {
        const size_t count =
            lsm_overview_history_count(app->overview.history);
        for (size_t index = 0U; index < count; index++) {
            LsmOverviewSample sample;
            if (lsm_overview_history_get(
                    app->overview.history, index, &sample))
                overview_push_sample(app, &sample);
        }
    }
    lsm_overview_refresh(app);
}

void lsm_overview_record_monitor_sample(LsmApp *app)
{
    if (!app) return;
    if (!app->overview.history)
        app->overview.history = lsm_overview_history_create();
    if (!app->overview.history ||
        !lsm_overview_history_record(
            app->overview.history, &app->monitor))
        return;

    if (app->runtime.page_built[LSM_TAB_OVERVIEW]) {
        LsmOverviewSample sample;
        if (lsm_overview_history_latest(
                app->overview.history, &sample))
            overview_push_sample(app, &sample);
    }
}

void lsm_overview_refresh(LsmApp *app)
{
    if (!app || !app->runtime.page_built[LSM_TAB_OVERVIEW] ||
        app->runtime.active_tab != LSM_TAB_OVERVIEW ||
        !app->overview.history)
        return;

    LsmOverviewSample sample;
    const gboolean have_monitor =
        lsm_overview_history_latest(app->overview.history, &sample) &&
        !sample.gap;
    const gboolean monitor_changed =
        have_monitor &&
        sample.generation != app->overview.displayed_monitor_generation;
    const gboolean process_changed =
        app->process.process_snapshot_generation !=
        app->overview.displayed_process_generation;

    if (monitor_changed) {
        overview_set_latest_values(app, &sample);
        for (size_t metric = 0U; metric < LSM_OVERVIEW_METRIC_COUNT; metric++)
            if (app->overview.gauges[metric])
                gtk_widget_queue_draw(app->overview.gauges[metric]);
        app->overview.displayed_monitor_generation = sample.generation;
    }
    if (process_changed) {
        overview_refresh_processes(app);
        app->overview.displayed_process_generation =
            app->process.process_snapshot_generation;
    }
}

void lsm_overview_destroy(LsmApp *app)
{
    if (!app) return;
    for (size_t metric = 0U; metric < LSM_OVERVIEW_METRIC_COUNT; metric++) {
        lsm_graph_free(app->overview.graphs[metric]);
        app->overview.graphs[metric] = NULL;
        app->overview.gauges[metric] = NULL;
        app->overview.buttons[metric] = NULL;
        app->overview.values[metric] = NULL;
        app->overview.details[metric] = NULL;
    }
    for (size_t row = 0U; row < LSM_OVERVIEW_TOP_PROCESS_COUNT; row++) {
        app->overview.process_rows[row] = NULL;
        app->overview.process_names[row] = NULL;
        app->overview.process_cpu_bars[row] = NULL;
        app->overview.process_cpu_values[row] = NULL;
        app->overview.process_memory_values[row] = NULL;
    }
    lsm_overview_history_destroy(app->overview.history);
    app->overview.history = NULL;
    app->overview.displayed_monitor_generation = 0U;
    app->overview.displayed_process_generation = 0U;
}
