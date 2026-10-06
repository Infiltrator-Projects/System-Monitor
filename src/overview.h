// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file overview.h
 * @brief Linux GTK Overview page fed by completed monitor snapshots.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_OVERVIEW_H
#define INFILTRATOR_SYSTEM_MONITOR_OVERVIEW_H

#include <limits.h>
#include <gtk/gtk.h>

/*
 * Keep the strict compatibility build aware of GTK 3 APIs used by Overview
 * even when it uses the project's intentionally minimal declaration surface
 * instead of the distribution GTK development headers. Production GTK headers
 * provide the same prototypes and GLib normally supplies G_MAXINT.
 */
#ifndef G_MAXINT
#define G_MAXINT INT_MAX
#endif
void gtk_widget_set_margin_end(GtkWidget *widget, gint margin);
void gtk_widget_get_preferred_width(
    GtkWidget *widget, gint *minimum_width, gint *natural_width);
guint gtk_grid_get_column_spacing(GtkGrid *grid);
void gtk_container_child_set(
    GtkContainer *container, GtkWidget *child,
    const gchar *first_prop_name, ...);
void gtk_grid_set_column_homogeneous(GtkGrid *grid, gboolean homogeneous);

typedef struct LsmApp LsmApp;

/**
 * Construct the lazily-created Overview page.
 *
 * Retained history exists independently of this widget tree, so opening the
 * page later does not reset or fabricate earlier samples.
 *
 * @param [in,out] app Application that owns Overview state.
 * @param [in] container Empty notebook container receiving the page.
 */
void lsm_overview_build(LsmApp *app, GtkWidget *container);

/**
 * Retain the current monitor snapshot only when it is newly completed.
 *
 * This function is called by the always-running Performance sampling path,
 * irrespective of which notebook page is visible.
 *
 * @param [in,out] app Application whose bounded Overview history is updated.
 */
void lsm_overview_record_monitor_sample(LsmApp *app);

/**
 * Present the newest retained Overview values and process ranking.
 *
 * @param [in,out] app Application whose Overview widgets are refreshed.
 */
void lsm_overview_refresh(LsmApp *app);

/**
 * Release Overview-owned graphs and bounded history.
 *
 * @param [in,out] app Application being shut down.
 */
void lsm_overview_destroy(LsmApp *app);

#endif
