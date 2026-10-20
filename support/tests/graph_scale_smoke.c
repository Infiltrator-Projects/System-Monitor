// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file graph_scale_smoke.c
 * @brief Exercise GTK graph scaling for recorded multi-core CPU percentages.
 *
 * A process can consume more than one logical core, while memory usage
 * remains bounded by 100%. Both traces share one axis: 100% is a floor,
 * not a maximum. This exercises the production renderer's scale contract.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "graph.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", \
                __FILE__, __LINE__, #condition); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

int main(int argc, char **argv)
{
    CHECK(gtk_init_check(&argc, &argv));

    LsmGraph *graph = lsm_graph_new(TRUE, FALSE, 0.0, -1, 390);
    CHECK(graph != NULL);
    lsm_graph_set_dynamic_scale(graph, 100.0, 100.0);
    CHECK(fabs(lsm_graph_get_maximum(graph) - 100.0) < 0.001);

    lsm_graph_push(graph, 85.0, 42.0, TRUE);
    CHECK(fabs(lsm_graph_get_maximum(graph) - 100.0) < 0.001);
    lsm_graph_push(graph, 245.0, 53.0, TRUE);
    CHECK(fabs(lsm_graph_get_maximum(graph) - 300.0) < 0.001);
    lsm_graph_push(graph, 401.0, 94.0, TRUE);
    CHECK(fabs(lsm_graph_get_maximum(graph) - 500.0) < 0.001);

    gtk_widget_destroy(graph->area);
    lsm_graph_free(graph);
    puts("Recorded-process graph scale preserves multi-core CPU percentages.");
    return EXIT_SUCCESS;
}
