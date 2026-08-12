// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file graph.c
 * @brief Cairo rendering for performance and sidebar history graphs.
 *
 * The renderer follows the compact visual structure of the original
 * SysMonTask graphs while remaining a native C implementation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "graph.h"
#include "ui_helpers.h"

#include <math.h>
#include <stdlib.h>

static double graph_maximum(const LsmGraph *graph)
{
    if (graph->percentage_scale) return 100.0;
    if (graph->fixed_max > 0.0) return graph->fixed_max;
    double maximum = 1.0;
    for (size_t i = 0; i < graph->primary.count; i++)
        maximum = fmax(maximum, lsm_sample_history_get(&graph->primary, i));
    if (graph->has_secondary) {
        for (size_t i = 0; i < graph->secondary.count; i++)
            maximum = fmax(maximum, lsm_sample_history_get(&graph->secondary, i));
    }
    if (graph->dynamic_step > 0.0) {
        const double rounded = ceil(maximum / graph->dynamic_step) * graph->dynamic_step;
        return fmax(graph->minimum_max, rounded);
    }
    maximum *= 1.15;
    double scale = 1.0;
    while (scale < maximum) scale *= 2.0;
    return fmax(graph->minimum_max, scale);
}

static void make_series_path(cairo_t *cr, const LsmSampleHistory *history,
                             double maximum, double width, double height)
{
    for (size_t i = 0; i < history->count; i++) {
        const double x = history->count > 1
            ? width * (double)i / (double)(history->count - 1) : 0.0;
        const double value = fmax(0.0, lsm_sample_history_get(history, i));
        const double y = height - fmin(height, height * value / maximum);
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
}

static void draw_series(cairo_t *cr, const LsmSampleHistory *history,
                        const GdkRGBA *colour, double maximum,
                        double width, double height, gboolean fill,
                        gboolean dashed, gboolean compact)
{
    if (history->count < 2 || maximum <= 0.0) return;

    if (dashed) {
        const double dashes[] = {3.0, 3.0};
        cairo_set_dash(cr, dashes, 2, 0.0);
    } else {
        cairo_set_dash(cr, NULL, 0, 0.0);
    }

    make_series_path(cr, history, maximum, width, height);
    if (fill) {
        cairo_line_to(cr, width, height);
        cairo_line_to(cr, 0.0, height);
        cairo_close_path(cr);
        cairo_set_source_rgba(cr, colour->red, colour->green, colour->blue,
                              compact ? 0.22 : 0.25);
        cairo_fill(cr);
        cairo_new_path(cr);
        make_series_path(cr, history, maximum, width, height);
    }

    cairo_set_source_rgba(cr, colour->red, colour->green, colour->blue, 1.0);
    cairo_set_line_width(cr, compact ? 1.45 : 1.65);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0.0);
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    LsmGraph *graph = user_data;
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    const double width = allocation.width;
    const double height = allocation.height;
    const GdkRGBA background = lsm_ui_background_colour(widget);

    cairo_set_source_rgba(cr, background.red, background.green, background.blue, 1.0);
    cairo_rectangle(cr, 0.0, 0.0, width, height);
    cairo_fill(cr);

    if (!graph->compact) {
        cairo_set_source_rgba(cr, graph->primary_colour.red,
                              graph->primary_colour.green,
                              graph->primary_colour.blue, 0.30);
        cairo_set_line_width(cr, 0.65);
        for (int i = 1; i < 10; i++) {
            const double x = width * i / 10.0;
            cairo_move_to(cr, x, 0.0);
            cairo_line_to(cr, x, height);
        }
        for (int i = 1; i < 10; i++) {
            const double y = height * i / 10.0;
            cairo_move_to(cr, 0.0, y);
            cairo_line_to(cr, width, y);
        }
        cairo_stroke(cr);
        if (graph->emphasise_midline) {
            cairo_set_source_rgba(cr, graph->primary_colour.red,
                                  graph->primary_colour.green,
                                  graph->primary_colour.blue, 0.48);
            cairo_set_line_width(cr, 0.9);
            cairo_move_to(cr, 0.0, height / 2.0);
            cairo_line_to(cr, width, height / 2.0);
            cairo_stroke(cr);
        }
    }

    const double maximum = graph_maximum(graph);
    draw_series(cr, &graph->primary, &graph->primary_colour, maximum,
                width, height, TRUE, FALSE, graph->compact);
    if (graph->has_secondary) {
        draw_series(cr, &graph->secondary, &graph->secondary_colour, maximum,
                    width, height, TRUE, TRUE, graph->compact);
    }

    cairo_set_source_rgba(cr, graph->primary_colour.red,
                          graph->primary_colour.green,
                          graph->primary_colour.blue, 1.0);
    cairo_set_line_width(cr, graph->compact ? 2.0 : 2.2);
    cairo_rectangle(cr, 1.0, 1.0, fmax(0.0, width - 2.0), fmax(0.0, height - 2.0));
    cairo_stroke(cr);
    return FALSE;
}

LsmGraph *lsm_graph_new(gboolean has_secondary,
                        gboolean percentage_scale,
                        double fixed_max,
                        int minimum_width,
                        int minimum_height)
{
    LsmGraph *graph = calloc(1, sizeof(*graph));
    if (!graph) return NULL;
    lsm_sample_history_init(&graph->primary);
    lsm_sample_history_init(&graph->secondary);
    graph->has_secondary = has_secondary;
    graph->percentage_scale = percentage_scale;
    graph->fixed_max = fixed_max;
    gdk_rgba_parse(&graph->primary_colour, "#39b8e3");
    graph->secondary_colour = graph->primary_colour;
    graph->area = gtk_drawing_area_new();
    gtk_widget_set_size_request(graph->area, minimum_width, minimum_height);
    gtk_widget_set_hexpand(graph->area, TRUE);
    gtk_widget_set_vexpand(graph->area, TRUE);
    g_signal_connect(graph->area, "draw", G_CALLBACK(on_draw), graph);
    return graph;
}

void lsm_graph_free(LsmGraph *graph)
{
    free(graph);
}

void lsm_graph_push(LsmGraph *graph, double primary, double secondary,
                    gboolean newer_on_right)
{
    if (!graph) return;
    lsm_sample_history_push(&graph->primary, primary, newer_on_right);
    if (graph->has_secondary)
        lsm_sample_history_push(&graph->secondary, secondary, newer_on_right);
    /* Hidden GtkStack pages still retain every sample, but GTK does not need
     * a redraw request until the drawing area is mapped. Sidebar graphs remain
     * mapped and continue to update normally. */
    if (gtk_widget_get_mapped(graph->area)) gtk_widget_queue_draw(graph->area);
}

void lsm_graph_queue_draw(LsmGraph *graph)
{
    if (graph && graph->area) gtk_widget_queue_draw(graph->area);
}

void lsm_graph_set_colours(LsmGraph *graph, const char *primary, const char *secondary)
{
    if (!graph) return;
    if (primary && *primary) gdk_rgba_parse(&graph->primary_colour, primary);
    if (secondary && *secondary) gdk_rgba_parse(&graph->secondary_colour, secondary);
    else graph->secondary_colour = graph->primary_colour;
}

void lsm_graph_set_compact(LsmGraph *graph, gboolean compact)
{
    if (!graph) return;
    graph->compact = compact;
    if (compact) {
        gtk_widget_set_hexpand(graph->area, FALSE);
        gtk_widget_set_vexpand(graph->area, FALSE);
    }
}


void lsm_graph_set_midline_emphasis(LsmGraph *graph, gboolean emphasise)
{
    if (!graph) return;
    graph->emphasise_midline = emphasise;
    if (gtk_widget_get_mapped(graph->area)) gtk_widget_queue_draw(graph->area);
}

void lsm_graph_set_dynamic_scale(LsmGraph *graph, double step, double minimum_max)
{
    if (!graph) return;
    graph->dynamic_step = step > 0.0 ? step : 0.0;
    graph->minimum_max = minimum_max > 0.0 ? minimum_max : 0.0;
    if (gtk_widget_get_mapped(graph->area)) gtk_widget_queue_draw(graph->area);
}

double lsm_graph_get_maximum(const LsmGraph *graph)
{
    return graph ? graph_maximum(graph) : 0.0;
}
