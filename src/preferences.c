// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file preferences.c
 * @brief Small, forward-compatible preferences store and GTK editor.
 *
 * Preferences use a deliberately simple key=value file. The parser recognises
 * only project-owned keys, bounds every numeric value and keeps safe defaults
 * when input is malformed. Saving uses the application's durable atomic-file
 * provider, so a power loss cannot leave a partially written configuration.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "preferences.h"
#include "common.h"
#include "app_internal.h"
#include "app_runtime.h"

#include "atomic_file.h"
#include "details_page.h"
#include "filesystems.h"
#include "numeric_io.h"
#include "performance.h"
#include "processes_ui.h"

#include <infiltratr/config.h>
#include <infiltratr/core.h>
#include <infiltratr/design.h>
#include <infiltratr/format.h>

#include <stdio.h>
#include <string.h>

static gboolean parse_boolean(const char *value, gboolean fallback)
{
    bool parsed = false;
    return infiltratr_config_parse_bool(value, &parsed)
        ? (parsed ? TRUE : FALSE) : fallback;
}

static InfiltratrThemeMode validated_theme_mode(
    const char *value, InfiltratrThemeMode fallback)
{
    InfiltratrThemeMode parsed = fallback;
    return infiltratr_theme_mode_parse(value, &parsed) ? parsed : fallback;
}

static guint validated_interval(const char *value, guint fallback)
{
    int64_t parsed = 0;
    if (!infiltratr_parse_i64_range(value, 10U, 0, INT64_MAX, &parsed) ||
        (parsed != 500 && parsed != 1000 && parsed != 2000 && parsed != 5000))
        return fallback;
    return (guint)parsed;
}

static gint validated_integer(const char *value, gint minimum, gint maximum,
                              gint fallback)
{
    int64_t parsed = 0;
    if (!infiltratr_parse_i64_range(value, 10U, minimum, maximum, &parsed))
        return fallback;
    return (gint)parsed;
}

static double validated_double(const char *value, double minimum,
                               double maximum, double fallback)
{
    double parsed = 0.0;
    return numeric_io_parse_persisted_double_range(
        value, minimum, maximum, &parsed, NULL) ? parsed : fallback;
}

static gboolean valid_stack_name(const char *value)
{
    if (!value || !*value) return FALSE;
    for (const unsigned char *cursor = (const unsigned char *)value;
         *cursor; cursor++) {
        if ((*cursor < 'a' || *cursor > 'z') &&
            (*cursor < '0' || *cursor > '9') &&
            *cursor != '-' && *cursor != '_')
            return FALSE;
    }
    return TRUE;
}

void lsm_preferences_load(LsmApp *app)
{
    if (!app || !app->paths.preferences_path[0]) return;
    FILE *file = fopen(app->paths.preferences_path, "r");
    if (!file) return;
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char *key = NULL;
        char *value = NULL;
        if (infiltratr_config_parse_line(line, &key, &value) !=
            INFILTRATR_CONFIG_LINE_ENTRY)
            continue;
        if (strcmp(key, "update_interval_ms") == 0)
            app->runtime.update_interval_ms = validated_interval(
                value, app->runtime.update_interval_ms);
        else if (strcmp(key, "filesystem_update_interval_ms") == 0) {
            int64_t parsed = 0;
            if (infiltratr_parse_i64_range(
                    value, 10U, 1000, 100000, &parsed))
                app->runtime.filesystem_update_interval_ms = (guint)parsed;
        }
        else if (strcmp(key, "theme_mode") == 0)
            app->runtime.theme_mode = validated_theme_mode(
                value, app->runtime.theme_mode);
        else if (strcmp(key, "newer_on_right") == 0)
            app->runtime.newer_on_right = parse_boolean(
                value, app->runtime.newer_on_right);
        else if (strcmp(key, "network_use_bits") == 0)
            app->runtime.network_use_bits = parse_boolean(
                value, app->runtime.network_use_bits);
        else if (strcmp(key, "network_total_separate") == 0)
            app->runtime.network_total_separate = parse_boolean(
                value, app->runtime.network_total_separate);
        else if (strcmp(key, "network_total_use_bits") == 0)
            app->runtime.network_total_use_bits = parse_boolean(
                value, app->runtime.network_total_use_bits);
        else if (strcmp(key, "graph_smooth") == 0)
            app->runtime.graph_smooth = parse_boolean(
                value, app->runtime.graph_smooth);
        else if (strcmp(key, "cpu_stacked") == 0)
            app->runtime.cpu_stacked = parse_boolean(
                value, app->runtime.cpu_stacked);
        else if (strcmp(key, "memory_logarithmic") == 0)
            app->runtime.memory_logarithmic = parse_boolean(
                value, app->runtime.memory_logarithmic);
        else if (strcmp(key, "graph_data_points") == 0) {
            int64_t parsed = 0;
            if (infiltratr_parse_i64_range(
                    value, 10U, 30, LSM_HISTORY_LENGTH, &parsed))
                app->runtime.graph_data_points = (guint)parsed;
        }
        else if (strcmp(key, "process_cpu_per_core") == 0)
            app->runtime.process_cpu_per_core = parse_boolean(
                value, app->runtime.process_cpu_per_core);
        else if (strcmp(key, "confirm_process_actions") == 0)
            app->runtime.confirm_process_actions = parse_boolean(
                value, app->runtime.confirm_process_actions);
        else if (strcmp(key, "show_all_filesystems") == 0)
            app->runtime.show_all_filesystems = parse_boolean(
                value, app->runtime.show_all_filesystems);
        else if (strcmp(key, "process_heatmap") == 0)
            app->details.process_heatmap = parse_boolean(
                value, app->details.process_heatmap);
        else if (strcmp(key, "always_on_top") == 0)
            app->runtime.always_on_top = parse_boolean(
                value, app->runtime.always_on_top);
        else if (strcmp(key, "compact_summary") == 0)
            app->runtime.compact_summary = parse_boolean(
                value, app->runtime.compact_summary);
        else if (strcmp(key, "window_width") == 0)
            app->runtime.window_width = validated_integer(
                value, 320, 7680, app->runtime.window_width);
        else if (strcmp(key, "window_height") == 0)
            app->runtime.window_height = validated_integer(
                value, 240, 4320, app->runtime.window_height);
        else if (strcmp(key, "window_maximized") == 0)
            app->runtime.window_maximized = parse_boolean(
                value, app->runtime.window_maximized);
        else if (strcmp(key, "last_tab") == 0)
            app->runtime.last_tab = validated_integer(
                value, 0, LSM_TAB_COUNT - 1, app->runtime.last_tab);
        else if (strcmp(key, "performance_page") == 0 &&
                 valid_stack_name(value))
            lsm_copy_string(app->runtime.selected_performance_page, sizeof(app->runtime.selected_performance_page),
                            value);
        else if (lsm_string_starts_with(key, "page_scroll_") &&
                 key[12] >= '0' && key[12] <= '7' && key[13] == '\0') {
            const size_t index = (size_t)(key[12] - '0');
            if (index < LSM_TAB_COUNT)
                app->runtime.page_scroll[index] = validated_double(
                    value, 0.0, 1000000000.0,
                    app->runtime.page_scroll[index]);
        }
    }
    fclose(file);
}

static bool write_preferences(FILE *file, const void *user_data)
{
    const LsmApp *app = user_data;
    int result = fprintf(file,
        "# System Monitor graphical preferences\n"
        "update_interval_ms=%u\n"
        "filesystem_update_interval_ms=%u\n"
        "theme_mode=%s\n"
        "newer_on_right=%d\n"
        "network_use_bits=%d\n"
        "network_total_separate=%d\n"
        "network_total_use_bits=%d\n"
        "graph_smooth=%d\n"
        "cpu_stacked=%d\n"
        "memory_logarithmic=%d\n"
        "graph_data_points=%u\n"
        "process_cpu_per_core=%d\n"
        "confirm_process_actions=%d\n"
        "show_all_filesystems=%d\n"
        "process_heatmap=%d\n"
        "always_on_top=%d\n"
        "compact_summary=%d\n"
        "window_width=%d\n"
        "window_height=%d\n"
        "window_maximized=%d\n"
        "last_tab=%d\n",
        app->runtime.update_interval_ms,
        app->runtime.filesystem_update_interval_ms,
        infiltratr_theme_mode_key(app->runtime.theme_mode),
        app->runtime.newer_on_right ? 1 : 0,
        app->runtime.network_use_bits ? 1 : 0,
        app->runtime.network_total_separate ? 1 : 0,
        app->runtime.network_total_use_bits ? 1 : 0,
        app->runtime.graph_smooth ? 1 : 0,
        app->runtime.cpu_stacked ? 1 : 0,
        app->runtime.memory_logarithmic ? 1 : 0,
        app->runtime.graph_data_points,
        app->runtime.process_cpu_per_core ? 1 : 0,
        app->runtime.confirm_process_actions ? 1 : 0,
        app->runtime.show_all_filesystems ? 1 : 0,
        app->details.process_heatmap ? 1 : 0,
        app->runtime.always_on_top ? 1 : 0,
        app->runtime.compact_summary ? 1 : 0,
        app->runtime.window_width, app->runtime.window_height,
        app->runtime.window_maximized ? 1 : 0,
        app->runtime.last_tab);
    bool okay = result >= 0;
    for (size_t index = 0U; okay && index < LSM_TAB_COUNT; index++) {
        char scroll[64];
        if (!infiltratr_format_fixed_ascii(
                app->runtime.page_scroll[index], 3U, scroll,
                sizeof(scroll)) ||
            fprintf(file, "page_scroll_%zu=%s\n", index, scroll) < 0)
            okay = false;
    }
    if (okay && fprintf(file, "performance_page=%s\n",
            app->runtime.selected_performance_page[0]
                ? app->runtime.selected_performance_page : "cpu") < 0)
        okay = false;
    return okay && ferror(file) == 0;
}

void lsm_preferences_save(const LsmApp *app)
{
    if (!app || !app->paths.preferences_path[0]) return;
    if (lsm_mkdir_parents(app->paths.config_dir, 0700U) != 0) return;
    (void)lsm_atomic_file_write(app->paths.preferences_path,
                                LSM_ATOMIC_FILE_PRIVATE,
                                write_preferences, app);
}

static void attach_preference(GtkGrid *grid, int row, const char *name,
                              GtkWidget *control)
{
    GtkWidget *label = gtk_label_new(name);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_halign(control, GTK_ALIGN_START);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    gtk_grid_attach(grid, control, 1, row, 1, 1);
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
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Preferences", GTK_WINDOW(app->shell.window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, "Apply", GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 700, 680);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 16);
    GtkWidget *intro = gtk_label_new(
        "These settings affect only the graphical presentation. Hardware collection remains native and unchanged.");
    gtk_label_set_line_wrap(GTK_LABEL(intro), TRUE);
    gtk_widget_set_halign(intro, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(content), intro, FALSE, FALSE, 0);
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 28);
    gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 14);

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
                             interval_index(app->runtime.update_interval_ms));
    attach_preference(GTK_GRID(grid), 0, "Performance refresh speed", speed);

    GtkWidget *filesystem_speed = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filesystem_speed),
                                   "Fast — 1 second");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filesystem_speed),
                                   "Normal — 5 seconds");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(filesystem_speed),
                                   "Slow — 10 seconds");
    int filesystem_speed_index =
        app->runtime.filesystem_update_interval_ms <= 1000U ? 0 :
        app->runtime.filesystem_update_interval_ms <= 5000U ? 1 : 2;
    gtk_combo_box_set_active(GTK_COMBO_BOX(filesystem_speed),
                             filesystem_speed_index);
    attach_preference(GTK_GRID(grid), 1, "File-system refresh speed",
                      filesystem_speed);

    GtkWidget *network = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network),
                                   "Bytes per second — KB/s, MB/s");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network),
                                   "Bits per second — Kb/s, Mb/s");
    gtk_combo_box_set_active(GTK_COMBO_BOX(network),
                             app->runtime.network_use_bits ? 1 : 0);
    attach_preference(GTK_GRID(grid), 2, "Network units", network);

    GtkWidget *network_totals = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network_totals),
                                   "Same unit as network speed");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network_totals),
                                   "Bytes — KB, MB, GB");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network_totals),
                                   "Bits — Kb, Mb, Gb");
    gtk_combo_box_set_active(GTK_COMBO_BOX(network_totals),
        !app->runtime.network_total_separate ? 0 :
        app->runtime.network_total_use_bits ? 2 : 1);
    attach_preference(GTK_GRID(grid), 3, "Network totals", network_totals);

    GtkWidget *cpu_mode = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cpu_mode),
        "Total computer capacity — process maximum 100%");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cpu_mode),
        "Per-core capacity — multi-threaded processes may exceed 100%");
    gtk_combo_box_set_active(GTK_COMBO_BOX(cpu_mode),
                             app->runtime.process_cpu_per_core ? 1 : 0);
    attach_preference(GTK_GRID(grid), 4, "Process CPU scale", cpu_mode);

    GtkWidget *direction = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(direction),
                                   "New values on the right");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(direction),
                                   "New values on the left");
    gtk_combo_box_set_active(GTK_COMBO_BOX(direction),
                             app->runtime.newer_on_right ? 0 : 1);
    attach_preference(GTK_GRID(grid), 5, "Graph direction", direction);

    GtkWidget *history_points = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(history_points), "60 samples");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(history_points), "100 samples");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(history_points), "300 samples");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(history_points), "600 samples");
    int history_index = app->runtime.graph_data_points <= 60U ? 0 :
        app->runtime.graph_data_points <= 100U ? 1 :
        app->runtime.graph_data_points <= 300U ? 2 : 3;
    gtk_combo_box_set_active(GTK_COMBO_BOX(history_points), history_index);
    attach_preference(GTK_GRID(grid), 6, "Graph history", history_points);

    GtkWidget *smooth_graphs = gtk_check_button_new_with_label(
        "Draw performance history as smooth graphs");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(smooth_graphs),
                                 app->runtime.graph_smooth);
    gtk_grid_attach(GTK_GRID(grid), smooth_graphs, 0, 7, 2, 1);

    GtkWidget *stacked_cpu = gtk_check_button_new_with_label(
        "Show CPU history as a stacked user/kernel area");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(stacked_cpu),
                                 app->runtime.cpu_stacked);
    gtk_grid_attach(GTK_GRID(grid), stacked_cpu, 0, 8, 2, 1);

    GtkWidget *log_memory = gtk_check_button_new_with_label(
        "Show Memory history on a logarithmic scale");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(log_memory),
                                 app->runtime.memory_logarithmic);
    gtk_grid_attach(GTK_GRID(grid), log_memory, 0, 9, 2, 1);

    GtkWidget *confirm_process = gtk_check_button_new_with_label(
        "Confirm before ending or force-terminating processes");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(confirm_process),
                                 app->runtime.confirm_process_actions);
    gtk_grid_attach(GTK_GRID(grid), confirm_process, 0, 10, 2, 1);

    GtkWidget *show_all = gtk_check_button_new_with_label(
        "Show virtual and system filesystems by default");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_all),
                                 app->runtime.show_all_filesystems);
    gtk_grid_attach(GTK_GRID(grid), show_all, 0, 11, 2, 1);
    GtkWidget *heatmap = gtk_check_button_new_with_label(
        "Shade busy resource cells in Processes and Details");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(heatmap),
                                 app->details.process_heatmap);
    gtk_grid_attach(GTK_GRID(grid), heatmap, 0, 12, 2, 1);
    GtkWidget *always_on_top = gtk_check_button_new_with_label(
        "Keep the monitor above other windows");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(always_on_top),
                                 app->runtime.always_on_top);
    gtk_grid_attach(GTK_GRID(grid), always_on_top, 0, 13, 2, 1);
    GtkWidget *compact_summary = gtk_check_button_new_with_label(
        "Open in compact summary mode");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compact_summary),
                                 app->runtime.compact_summary);
    gtk_grid_attach(GTK_GRID(grid), compact_summary, 0, 14, 2, 1);
    GtkWidget *cadence_note = gtk_label_new(
        "Performance graphs can refresh every 0.5 seconds. Process and "
        "management lists refresh no faster than once per second.");
    gtk_label_set_line_wrap(GTK_LABEL(cadence_note), TRUE);
    gtk_widget_set_halign(cadence_note, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), cadence_note, 0, 15, 2, 1);

    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        app->runtime.update_interval_ms = interval_from_index(
            gtk_combo_box_get_active(GTK_COMBO_BOX(speed)));
        app->runtime.filesystem_update_interval_ms =
            gtk_combo_box_get_active(GTK_COMBO_BOX(filesystem_speed)) == 0
                ? 1000U
                : gtk_combo_box_get_active(GTK_COMBO_BOX(filesystem_speed)) == 1
                    ? 5000U : 10000U;
        app->runtime.network_use_bits =
            gtk_combo_box_get_active(GTK_COMBO_BOX(network)) == 1;
        const int network_totals_index =
            gtk_combo_box_get_active(GTK_COMBO_BOX(network_totals));
        app->runtime.network_total_separate = network_totals_index != 0;
        app->runtime.network_total_use_bits = network_totals_index == 2;
        static const guint graph_points[] = {60U, 100U, 300U, 600U};
        const int graph_index =
            gtk_combo_box_get_active(GTK_COMBO_BOX(history_points));
        app->runtime.graph_data_points =
            graph_index >= 0 && (size_t)graph_index < G_N_ELEMENTS(graph_points)
                ? graph_points[graph_index] : 100U;
        app->runtime.graph_smooth =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(smooth_graphs));
        app->runtime.cpu_stacked =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(stacked_cpu));
        app->runtime.memory_logarithmic =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(log_memory));
        app->runtime.process_cpu_per_core =
            gtk_combo_box_get_active(GTK_COMBO_BOX(cpu_mode)) == 1;
        app->runtime.newer_on_right =
            gtk_combo_box_get_active(GTK_COMBO_BOX(direction)) == 0;
        app->runtime.confirm_process_actions =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(confirm_process));
        app->runtime.show_all_filesystems =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_all));
        app->details.process_heatmap =
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(heatmap));
        app->runtime.always_on_top = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(always_on_top));
        app->runtime.compact_summary = gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(compact_summary));
        if (app->shell.always_on_top_menu_item)
            gtk_check_menu_item_set_active(
                GTK_CHECK_MENU_ITEM(app->shell.always_on_top_menu_item),
                app->runtime.always_on_top);
        if (app->shell.compact_summary_menu_item)
            gtk_check_menu_item_set_active(
                GTK_CHECK_MENU_ITEM(app->shell.compact_summary_menu_item),
                app->runtime.compact_summary);
        if (app->filesystem.filesystem_show_all)
            gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(app->filesystem.filesystem_show_all),
                app->runtime.show_all_filesystems);
        lsm_preferences_save(app);
        lsm_app_preferences_changed(app);
        lsm_performance_apply_graph_preferences(app);
        app->processes.processes_model_dirty = TRUE;
        app->details.details_model_dirty = TRUE;
        lsm_processes_present_snapshot(app);
        lsm_details_present_snapshot(app);
        lsm_performance_refresh(app);
        lsm_filesystems_refresh(app);
    }
    gtk_widget_destroy(dialog);
}
