// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file graph.h
 * @brief Reusable GTK/Cairo history graph widget.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_GRAPH_H
#define LINUX_SYSTEM_MONITOR_GRAPH_H

#include <gtk/gtk.h>
#include "sample_history.h"

/** Runtime state for one single- or dual-series graph. */
typedef struct {
    GtkWidget *area;
    LsmSampleHistory primary;
    LsmSampleHistory secondary;
    gboolean has_secondary;
    gboolean percentage_scale;
    gboolean compact;
    gboolean emphasise_midline;
    double fixed_max;
    double dynamic_step;
    double minimum_max;
    GdkRGBA primary_colour;
    GdkRGBA secondary_colour;
} LsmGraph;

/**
 * Allocate a graph widget and its fixed-capacity sample histories.
 *
 * @param [in] has_secondary Whether a second series is displayed.
 * @param [in] percentage_scale Clamp the vertical scale to 0-100 percent.
 * @param [in] fixed_max Positive fixed maximum, or zero for dynamic scaling.
 * @param [in] minimum_width Minimum drawing-area width, or -1 for GTK's natural minimum.
 * @param [in] minimum_height Minimum drawing-area height, or -1 for GTK's natural minimum.
 * @return New graph owned by the caller, or NULL on allocation failure.
 */
LsmGraph *lsm_graph_new(gboolean has_secondary,
                        gboolean percentage_scale,
                        double fixed_max,
                        int minimum_width,
                        int minimum_height);
/**
 * Release graph-owned history and drawing state.
 *
 * @param [in,out] graph Graph returned by lsm_graph_new(), or NULL.
 */
void lsm_graph_free(LsmGraph *graph);
/**
 * Append primary and optional secondary samples in O(1) time.
 *
 * @param [in,out] graph Graph receiving the samples.
 * @param [in] primary Primary series value.
 * @param [in] secondary Secondary value when enabled.
 * @param [in] newer_on_right Whether newest samples appear on the right.
 */
void lsm_graph_push(LsmGraph *graph, double primary, double secondary,
                    gboolean newer_on_right);
/**
 * Queue a GTK redraw for the graph's drawing area.
 *
 * @param [in,out] graph Graph whose widget should be invalidated.
 */
void lsm_graph_queue_draw(LsmGraph *graph);
/**
 * Replace primary and secondary series colours from CSS colour strings.
 *
 * @param [in,out] graph Graph to configure.
 * @param [in] primary Primary CSS colour specification.
 * @param [in] secondary Secondary CSS colour specification, or NULL.
 */
void lsm_graph_set_colours(LsmGraph *graph, const char *primary, const char *secondary);
/**
 * Select compact side-pane rendering or full detail rendering.
 *
 * @param [in,out] graph Graph to configure.
 * @param [in] compact TRUE for compact rendering.
 */
void lsm_graph_set_compact(LsmGraph *graph, gboolean compact);
/**
 * Emphasise the horizontal 50-percent line in a full-size graph.
 *
 * This is useful for Task-Manager-style axis presentation where a midpoint
 * scale label is shown beside the graph. Compact sidebar graphs ignore it.
 *
 * @param [in,out] graph Graph to configure.
 * @param [in] emphasise TRUE to draw a stronger midpoint guide.
 */
void lsm_graph_set_midline_emphasis(LsmGraph *graph, gboolean emphasise);
/**
 * Configure quantised dynamic vertical scaling.
 *
 * @param [in,out] graph Graph to configure.
 * @param [in] step Scale grows in positive multiples of this value.
 * @param [in] minimum_max Lower bound for the displayed maximum.
 */
void lsm_graph_set_dynamic_scale(LsmGraph *graph, double step, double minimum_max);
/**
 * Return the vertical maximum currently used for presentation.
 *
 * @param [in] graph Graph to inspect.
 * @return Current positive maximum, or 0.0 for an invalid graph.
 */
double lsm_graph_get_maximum(const LsmGraph *graph);

#endif
