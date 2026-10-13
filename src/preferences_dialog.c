// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file preferences_dialog.c
 * @brief GTK editor for System Monitor preferences.
 *
 * Persistence and application-state projection remain owned by preferences.c.
 * This module owns only user interaction and submits one accepted value set
 * through the preferences contract without depending on private LsmApp layout.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "preferences.h"

#include "app_presentation_context.h"
#include "ui_helpers.h"

#include <infiltratr/design.h>

static void attach_preference(GtkGrid *grid, int row, const char *name,
                              GtkWidget *control)
{
    GtkWidget *label = gtk_label_new(name);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_halign(control, GTK_ALIGN_FILL);
    gtk_widget_set_size_request(control, 280, 40);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    gtk_grid_attach(grid, control, 1, row, 1, 1);
}

static GtkWidget *preference_page(GtkNotebook *notebook, const char *title)
{
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 24);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 20);
    gtk_container_add(GTK_CONTAINER(scroll), grid);
    gtk_notebook_append_page(notebook, scroll, gtk_label_new(title));
    return grid;
}

static void attach_toggle(GtkGrid *grid, int row, GtkWidget *toggle)
{
    gtk_widget_set_hexpand(toggle, TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(toggle),
                                "lsm-preference-toggle");
    gtk_grid_attach(grid, toggle, 0, row, 2, 1);
}

static int interval_index(guint interval)
{
    switch (interval) {
        case 500U: return 0;
        case 1000U: return 1;
        case 2000U: return 2;
        case 5000U: return 3;
        default: return 1;
    }
}

static guint interval_from_index(int index)
{
    static const guint intervals[] = {500U, 1000U, 2000U, 5000U};
    return index >= 0 && (size_t)index < G_N_ELEMENTS(intervals)
        ? intervals[index] : 1000U;
}

void lsm_preferences_show(LsmApp *app)
{
    if (!app) return;

    LsmPreferencesDialogValues current;
    if (!lsm_preferences_dialog_values(app, &current)) return;
    GtkWidget *parent = lsm_app_presentation_window(app);
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Preferences", parent ? GTK_WINDOW(parent) : NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, "Apply", GTK_RESPONSE_ACCEPT, NULL);
    gtk_style_context_add_class(gtk_widget_get_style_context(dialog),
                                "lsm-preferences");
    lsm_ui_set_workarea_default_size(GTK_WINDOW(dialog), 700, 570);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    GtkWidget *apply = gtk_dialog_get_widget_for_response(
        GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
    gtk_style_context_add_class(gtk_widget_get_style_context(apply),
                                "suggested-action");
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 20);
    gtk_box_set_spacing(GTK_BOX(content), 16);
    GtkWidget *heading = gtk_label_new("Preferences");
    gtk_label_set_xalign(GTK_LABEL(heading), 0.0F);
    gtk_style_context_add_class(gtk_widget_get_style_context(heading),
                                "lsm-performance-title");
    gtk_style_context_add_class(gtk_widget_get_style_context(heading),
                                "lsm-preferences-title");
    gtk_box_pack_start(GTK_BOX(content), heading, FALSE, FALSE, 0);
    GtkWidget *intro = gtk_label_new(
        "Choose how updates, graphs and resource lists are displayed.");
    gtk_label_set_line_wrap(GTK_LABEL(intro), TRUE);
    gtk_label_set_xalign(GTK_LABEL(intro), 0.0F);
    gtk_style_context_add_class(gtk_widget_get_style_context(intro),
                                "lsm-preferences-note");
    gtk_box_pack_start(GTK_BOX(content), intro, FALSE, FALSE, 0);
    GtkWidget *notebook = gtk_notebook_new();
    gtk_widget_set_hexpand(notebook, TRUE);
    gtk_widget_set_vexpand(notebook, TRUE);
    gtk_box_pack_start(GTK_BOX(content), notebook, TRUE, TRUE, 0);
    GtkWidget *updates = preference_page(GTK_NOTEBOOK(notebook), "Updates & units");
    GtkWidget *graphs = preference_page(GTK_NOTEBOOK(notebook), "Graphs");
    GtkWidget *behaviour = preference_page(GTK_NOTEBOOK(notebook), "Behaviour");

    GtkWidget *speed = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(speed),
                                   "Fast — 0.5 seconds");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(speed),
                                   "Normal — 1 second");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(speed),
                                   "Low — 2 seconds");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(speed),
                                   "Very low — 5 seconds");
    gtk_combo_box_set_active(GTK_COMBO_BOX(speed),
                             interval_index(current.update_interval_ms));
    attach_preference(GTK_GRID(updates), 0, "Performance refresh speed", speed);

    GtkWidget *filesystem_speed = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filesystem_speed),
                                   "Fast — 1 second");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filesystem_speed),
                                   "Normal — 5 seconds");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filesystem_speed),
                                   "Slow — 10 seconds");
    int filesystem_speed_index =
        current.filesystem_update_interval_ms <= 1000U ? 0 :
        current.filesystem_update_interval_ms <= 5000U ? 1 : 2;
    gtk_combo_box_set_active(GTK_COMBO_BOX(filesystem_speed),
                             filesystem_speed_index);
    attach_preference(GTK_GRID(updates), 1, "File-system refresh speed",
                      filesystem_speed);

    GtkWidget *network = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network),
                                   "Bytes per second — KB/s, MB/s");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network),
                                   "Bits per second — Kb/s, Mb/s");
    gtk_combo_box_set_active(GTK_COMBO_BOX(network),
                             current.network_use_bits ? 1 : 0);
    attach_preference(GTK_GRID(updates), 2, "Network units", network);

    GtkWidget *network_totals = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network_totals),
                                   "Same unit as network speed");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network_totals),
                                   "Bytes — KB, MB, GB");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network_totals),
                                   "Bits — Kb, Mb, Gb");
    gtk_combo_box_set_active(GTK_COMBO_BOX(network_totals),
        !current.network_total_separate ? 0 :
        current.network_total_use_bits ? 2 : 1);
    attach_preference(GTK_GRID(updates), 3, "Network totals", network_totals);

    GtkWidget *cpu_mode = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cpu_mode),
        "Whole computer — maximum 100%");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cpu_mode),
        "Per core — may exceed 100%");
    gtk_combo_box_set_active(GTK_COMBO_BOX(cpu_mode),
                             current.process_cpu_per_core ? 1 : 0);
    attach_preference(GTK_GRID(updates), 4, "Process CPU scale", cpu_mode);

    GtkWidget *direction = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(direction),
                                   "New values on the right");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(direction),
                                   "New values on the left");
    gtk_combo_box_set_active(GTK_COMBO_BOX(direction),
                             current.newer_on_right ? 0 : 1);
    attach_preference(GTK_GRID(graphs), 0, "Graph direction", direction);

    GtkWidget *history_points = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(history_points), "60 samples");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(history_points), "100 samples");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(history_points), "300 samples");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(history_points), "600 samples");
    int history_index = current.graph_data_points <= 60U ? 0 :
        current.graph_data_points <= 100U ? 1 :
        current.graph_data_points <= 300U ? 2 : 3;
    gtk_combo_box_set_active(GTK_COMBO_BOX(history_points), history_index);
    attach_preference(GTK_GRID(graphs), 1, "Graph history", history_points);

    GtkWidget *smooth_graphs = gtk_check_button_new_with_label(
        "Draw performance history as smooth graphs");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(smooth_graphs),
                                 current.graph_smooth);
    attach_toggle(GTK_GRID(graphs), 2, smooth_graphs);

    GtkWidget *stacked_cpu = gtk_check_button_new_with_label(
        "Show CPU history as a stacked user/kernel area");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stacked_cpu),
                                 current.cpu_stacked);
    attach_toggle(GTK_GRID(graphs), 3, stacked_cpu);

    GtkWidget *log_memory = gtk_check_button_new_with_label(
        "Show Memory history on a logarithmic scale");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(log_memory),
                                 current.memory_logarithmic);
    attach_toggle(GTK_GRID(graphs), 4, log_memory);

    GtkWidget *confirm_process = gtk_check_button_new_with_label(
        "Confirm before ending or force-terminating processes");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(confirm_process),
                                 current.confirm_process_actions);
    attach_toggle(GTK_GRID(behaviour), 0, confirm_process);

    GtkWidget *show_all = gtk_check_button_new_with_label(
        "Show virtual and system filesystems by default");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_all),
                                 current.show_all_filesystems);
    attach_toggle(GTK_GRID(behaviour), 1, show_all);
    GtkWidget *heatmap = gtk_check_button_new_with_label(
        "Shade busy resource cells in Processes and Details");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(heatmap),
                                 current.process_heatmap);
    attach_toggle(GTK_GRID(behaviour), 2, heatmap);
    GtkWidget *always_on_top = gtk_check_button_new_with_label(
        "Keep the monitor above other windows");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(always_on_top),
                                 current.always_on_top);
    attach_toggle(GTK_GRID(behaviour), 3, always_on_top);
    GtkWidget *compact_summary = gtk_check_button_new_with_label(
        "Open in compact summary mode");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compact_summary),
                                 current.compact_summary);
    attach_toggle(GTK_GRID(behaviour), 4, compact_summary);
    GtkWidget *cadence_note = gtk_label_new(
        "Graphs support 0.5-second updates. Process and management lists "
        "update at most once per second.");
    gtk_label_set_line_wrap(GTK_LABEL(cadence_note), TRUE);
    gtk_widget_set_halign(cadence_note, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(cadence_note),
                                "lsm-preferences-note");
    gtk_widget_set_margin_top(cadence_note, 8);
    gtk_grid_attach(GTK_GRID(updates), cadence_note, 0, 5, 2, 1);

    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        LsmPreferencesDialogValues accepted = current;
        accepted.update_interval_ms = interval_from_index(
            gtk_combo_box_get_active(GTK_COMBO_BOX(speed)));
        accepted.filesystem_update_interval_ms =
            gtk_combo_box_get_active(GTK_COMBO_BOX(filesystem_speed)) == 0
                ? 1000U
                : gtk_combo_box_get_active(GTK_COMBO_BOX(filesystem_speed)) == 1
                    ? 5000U : 10000U;
        accepted.network_use_bits =
            gtk_combo_box_get_active(GTK_COMBO_BOX(network)) == 1;
        const int network_totals_index =
            gtk_combo_box_get_active(GTK_COMBO_BOX(network_totals));
        accepted.network_total_separate = network_totals_index != 0;
        accepted.network_total_use_bits = network_totals_index == 2;
        static const guint graph_points[] = {60U, 100U, 300U, 600U};
        const int graph_index =
            gtk_combo_box_get_active(GTK_COMBO_BOX(history_points));
        accepted.graph_data_points =
            graph_index >= 0 && (size_t)graph_index < G_N_ELEMENTS(graph_points)
                ? graph_points[graph_index] : 100U;
        accepted.graph_smooth =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(smooth_graphs));
        accepted.cpu_stacked =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(stacked_cpu));
        accepted.memory_logarithmic =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(log_memory));
        accepted.process_cpu_per_core =
            gtk_combo_box_get_active(GTK_COMBO_BOX(cpu_mode)) == 1;
        accepted.newer_on_right =
            gtk_combo_box_get_active(GTK_COMBO_BOX(direction)) == 0;
        accepted.confirm_process_actions =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(confirm_process));
        accepted.show_all_filesystems =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_all));
        accepted.process_heatmap =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(heatmap));
        accepted.always_on_top = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(always_on_top));
        accepted.compact_summary = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(compact_summary));
        lsm_preferences_commit_dialog_values(app, &accepted);
    }
    gtk_widget_destroy(dialog);
}
