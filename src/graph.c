// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file graph.c
 * @brief Cairo rendering for performance and sidebar history graphs.
 *
 * The renderer owns System Monitor's native Cairo graph presentation,
 * including retained history, scaling, labels and threshold styling.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "graph.h"
#include "ui_helpers.h"

#include <infiltratr/design.h>

#include <math.h>
#include <stdlib.h>

static void rounded_rectangle(cairo_t *cr, double x, double y,
                              double width, double height, double radius)
{
    static const double half_pi = 1.57079632679489661923;
    const double right = x + width;
    const double bottom = y + height;
    double r = radius;

    if (r < 0.0) r = 0.0;
    if (r > width / 2.0) r = width / 2.0;
    if (r > height / 2.0) r = height / 2.0;
    if (r <= 0.0) {
        cairo_rectangle(cr, x, y, width, height);
        return;
    }

    cairo_new_path(cr);
    cairo_move_to(cr, x + r, y);
    cairo_line_to(cr, right - r, y);
    cairo_arc(cr, right - r, y + r, r, -half_pi, 0.0);
    cairo_line_to(cr, right, bottom - r);
    cairo_arc(cr, right - r, bottom - r, r, 0.0, half_pi);
    cairo_line_to(cr, x + r, bottom);
    cairo_arc(cr, x + r, bottom - r, r, half_pi, 2.0 * half_pi);
    cairo_line_to(cr, x, y + r);
    cairo_arc(cr, x + r, y + r, r, 2.0 * half_pi, 3.0 * half_pi);
    cairo_close_path(cr);
}

static double graph_maximum(const LsmGraph *graph)
{
    if (graph->percentage_scale) return 100.0;
    if (graph->fixed_max > 0.0) return graph->fixed_max;
    double maximum = 1.0;
    for (size_t i = 0; i < graph->primary.count; i++) {
        if (lsm_sample_history_is_valid(&graph->primary, i))
            maximum = fmax(maximum,
                           lsm_sample_history_get(&graph->primary, i));
    }
    if (graph->has_secondary) {
        for (size_t i = 0; i < graph->secondary.count; i++) {
            if (lsm_sample_history_is_valid(&graph->secondary, i))
                maximum = fmax(maximum,
                               lsm_sample_history_get(&graph->secondary, i));
        }
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

static void append_series_segment(cairo_t *cr,
                                  const double *x, const double *y,
                                  size_t count, bool smooth)
{
    if (!cr || !x || !y || count == 0U) return;
    cairo_move_to(cr, x[0], y[0]);
    if (!smooth || count < 3U) {
        for (size_t i = 1U; i < count; i++)
            cairo_line_to(cr, x[i], y[i]);
        return;
    }

    for (size_t i = 0U; i + 1U < count; i++) {
        const size_t p0 = i > 0U ? i - 1U : i;
        const size_t p1 = i;
        const size_t p2 = i + 1U;
        const size_t p3 = i + 2U < count ? i + 2U : i + 1U;
        for (int step = 1; step <= 5; step++) {
            const double t = (double)step / 5.0;
            const double t2 = t * t;
            const double t3 = t2 * t;
            const double px = 0.5 * (
                (2.0 * x[p1]) +
                (-x[p0] + x[p2]) * t +
                (2.0 * x[p0] - 5.0 * x[p1] +
                 4.0 * x[p2] - x[p3]) * t2 +
                (-x[p0] + 3.0 * x[p1] -
                 3.0 * x[p2] + x[p3]) * t3);
            const double py = 0.5 * (
                (2.0 * y[p1]) +
                (-y[p0] + y[p2]) * t +
                (2.0 * y[p0] - 5.0 * y[p1] +
                 4.0 * y[p2] - y[p3]) * t2 +
                (-y[p0] + 3.0 * y[p1] -
                 3.0 * y[p2] + y[p3]) * t3);
            cairo_line_to(cr, px, py);
        }
    }
}

static bool make_series_path(cairo_t *cr,
                             const LsmSampleHistory *history,
                             double maximum, double width, double height,
                             bool close_to_baseline, bool smooth)
{
    bool any = false;
    double x[LSM_HISTORY_LENGTH];
    double y[LSM_HISTORY_LENGTH];
    size_t segment_count = 0U;

    for (size_t i = 0U; i <= history->count; i++) {
        const bool valid =
            i < history->count && lsm_sample_history_is_valid(history, i);
        if (valid) {
            x[segment_count] = history->count > 1U
                ? width * (double)i / (double)(history->count - 1U) : 0.0;
            const double value =
                fmax(0.0, lsm_sample_history_get(history, i));
            y[segment_count] =
                height - fmin(height, height * value / maximum);
            segment_count++;
            any = true;
            continue;
        }

        if (segment_count == 0U) continue;
        append_series_segment(cr, x, y, segment_count, smooth);
        if (close_to_baseline) {
            cairo_line_to(cr, x[segment_count - 1U], height);
            cairo_line_to(cr, x[0], height);
            cairo_close_path(cr);
        }
        segment_count = 0U;
    }
    return any;
}

static void draw_series(cairo_t *cr, const LsmSampleHistory *history,
                        const GdkRGBA *colour, double maximum,
                        double width, double height, gboolean fill,
                        gboolean dashed, gboolean compact, gboolean smooth)
{
    if (history->count < 2 || maximum <= 0.0) return;

    if (fill) {
        cairo_new_path(cr);
        if (make_series_path(cr, history, maximum, width, height, true, smooth)) {
            cairo_pattern_t *gradient =
                cairo_pattern_create_linear(0.0, 0.0, 0.0, height);
            cairo_pattern_add_color_stop_rgba(
                gradient, 0.0, colour->red, colour->green, colour->blue,
                compact ? 0.22 : 0.38);
            cairo_pattern_add_color_stop_rgba(
                gradient, 0.58, colour->red, colour->green, colour->blue,
                compact ? 0.14 : 0.20);
            cairo_pattern_add_color_stop_rgba(
                gradient, 1.0, colour->red, colour->green, colour->blue, 0.025);
            cairo_set_source(cr, gradient);
            cairo_fill(cr);
            cairo_pattern_destroy(gradient);
        }
    }

    if (dashed) {
        const double dashes[] = {3.0, 3.0};
        cairo_set_dash(cr, dashes, 2, 0.0);
    } else {
        cairo_set_dash(cr, NULL, 0, 0.0);
    }

    cairo_new_path(cr);
    if (make_series_path(cr, history, maximum, width, height, false, smooth)) {
        cairo_set_source_rgba(
            cr, colour->red, colour->green, colour->blue,
            compact ? 0.16 : 0.20);
        cairo_set_line_width(cr, compact ? 5.0 : 7.0);
        cairo_stroke_preserve(cr);
        cairo_set_source_rgba(cr, colour->red, colour->green, colour->blue, 1.0);
        cairo_set_line_width(cr, compact ? 1.55 : 1.85);
        cairo_stroke(cr);
    }
    cairo_set_dash(cr, NULL, 0, 0.0);
}

static gboolean on_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    LsmGraph *graph = user_data;
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    const double width = allocation.width;
    const double height = allocation.height;
    const InfiltratrDesignMetrics *metrics = infiltratr_design_metrics();
    const double radius = metrics
        ? (graph->compact ? (double)metrics->small_radius
                          : (double)metrics->card_radius)
        : (graph->compact ? 6.0 : 12.0);
    const GdkRGBA fallback_background = lsm_ui_background_colour(widget);
    GdkRGBA background = fallback_background;
    GdkRGBA border = {0.21, 0.23, 0.25, 1.0};
    GtkStyleContext *style = gtk_widget_get_style_context(widget);
    if (style) {
        const char *surface_name = graph->compact ? "lsm_surface" : "lsm_card";
        (void)gtk_style_context_lookup_color(style, surface_name, &background);
        (void)gtk_style_context_lookup_color(style, "lsm_border", &border);
    }

    cairo_save(cr);
    rounded_rectangle(cr, 0.0, 0.0, width, height, radius);
    cairo_clip(cr);

    cairo_set_source_rgba(cr, background.red, background.green, background.blue, 1.0);
    cairo_rectangle(cr, 0.0, 0.0, width, height);
    cairo_fill(cr);

    {
        const int divisions = graph->compact ? 4 : 10;
        cairo_set_source_rgba(cr, graph->primary_colour.red,
                              graph->primary_colour.green,
                              graph->primary_colour.blue,
                              graph->compact ? 0.055 : 0.13);
        cairo_set_line_width(cr, graph->compact ? 0.35 : 0.50);
        for (int i = 1; i < divisions; i++) {
            const double x = width * i / (double)divisions;
            cairo_move_to(cr, x, 0.0);
            cairo_line_to(cr, x, height);
        }
        for (int i = 1; i < divisions; i++) {
            const double y = height * i / (double)divisions;
            cairo_move_to(cr, 0.0, y);
            cairo_line_to(cr, width, y);
        }
        cairo_stroke(cr);
        if (!graph->compact && graph->emphasise_midline) {
            cairo_set_source_rgba(cr, graph->primary_colour.red,
                                  graph->primary_colour.green,
                                  graph->primary_colour.blue, 0.24);
            cairo_set_line_width(cr, 0.70);
            cairo_move_to(cr, 0.0, height / 2.0);
            cairo_line_to(cr, width, height / 2.0);
            cairo_stroke(cr);
        }
    }

    const double maximum = graph_maximum(graph);
    draw_series(cr, &graph->primary, &graph->primary_colour, maximum,
                width, height, TRUE, FALSE, graph->compact, graph->smooth);
    if (graph->has_secondary) {
        draw_series(cr, &graph->secondary, &graph->secondary_colour, maximum,
                    width, height, TRUE, graph->secondary_dashed,
                    graph->compact, graph->smooth);
    }

    cairo_restore(cr);
    cairo_set_source_rgba(cr, border.red, border.green, border.blue, border.alpha);
    cairo_set_line_width(cr, 1.0);
    rounded_rectangle(cr, 0.5, 0.5, fmax(0.0, width - 1.0),
                      fmax(0.0, height - 1.0), radius);
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
    graph->secondary_dashed = TRUE;
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

void lsm_graph_set_smooth(LsmGraph *graph, gboolean smooth)
{
    if (!graph) return;
    graph->smooth = smooth;
    if (gtk_widget_get_mapped(graph->area)) gtk_widget_queue_draw(graph->area);
}

void lsm_graph_set_secondary_dashed(LsmGraph *graph, gboolean dashed)
{
    if (!graph) return;
    graph->secondary_dashed = dashed;
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
