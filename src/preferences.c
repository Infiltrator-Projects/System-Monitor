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
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "preferences.h"

#include "atomic_file.h"
#include "filesystems.h"
#include "performance.h"
#include "details_page.h"
#include "processes_ui.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static gboolean parse_boolean(const char *value, gboolean fallback)
{
    if (!value) return fallback;
    if (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0)
        return TRUE;
    if (strcmp(value, "0") == 0 || strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "no") == 0)
        return FALSE;
    return fallback;
}

static guint validated_interval(const char *value, guint fallback)
{
    if (!value) return fallback;
    char *end = NULL;
    errno = 0;
    const unsigned long parsed = strtoul(value, &end, 10);
    if (errno || end == value || *end ||
        (parsed != 500UL && parsed != 1000UL && parsed != 2000UL &&
         parsed != 5000UL))
        return fallback;
    return (guint)parsed;
}

static gint validated_integer(const char *value, gint minimum, gint maximum,
                              gint fallback)
{
    if (!value) return fallback;
    char *end = NULL;
    errno = 0;
    const long parsed = strtol(value, &end, 10);
    if (errno || end == value || *end || parsed < minimum || parsed > maximum)
        return fallback;
    return (gint)parsed;
}

static double validated_double(const char *value, double minimum,
                               double maximum, double fallback)
{
    if (!value) return fallback;
    char *end = NULL;
    errno = 0;
    const double parsed = strtod(value, &end);
    if (errno || end == value || *end || !isfinite(parsed) ||
        parsed < minimum || parsed > maximum)
        return fallback;
    return parsed;
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
    gint tab_layout_version = 0;
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *equals = strchr(line, '=');
        if (!equals) continue;
        *equals++ = '\0';
        if (strcmp(line, "update_interval_ms") == 0)
            app->runtime.update_interval_ms = validated_interval(equals,
                                                         app->runtime.update_interval_ms);
        else if (strcmp(line, "newer_on_right") == 0)
            app->runtime.newer_on_right = parse_boolean(equals, app->runtime.newer_on_right);
        else if (strcmp(line, "network_use_bits") == 0)
            app->runtime.network_use_bits = parse_boolean(equals, app->runtime.network_use_bits);
        else if (strcmp(line, "process_cpu_per_core") == 0)
            app->runtime.process_cpu_per_core = parse_boolean(equals,
                                                      app->runtime.process_cpu_per_core);
        else if (strcmp(line, "show_all_filesystems") == 0)
            app->runtime.show_all_filesystems = parse_boolean(equals,
                                                      app->runtime.show_all_filesystems);
        else if (strcmp(line, "process_heatmap") == 0)
            app->details.process_heatmap = parse_boolean(equals, app->details.process_heatmap);
        else if (strcmp(line, "always_on_top") == 0)
            app->runtime.always_on_top = parse_boolean(equals, app->runtime.always_on_top);
        else if (strcmp(line, "compact_summary") == 0)
            app->runtime.compact_summary = parse_boolean(equals, app->runtime.compact_summary);
        else if (strcmp(line, "window_width") == 0)
            app->runtime.window_width = validated_integer(
                equals, 320, 7680, app->runtime.window_width);
        else if (strcmp(line, "window_height") == 0)
            app->runtime.window_height = validated_integer(
                equals, 240, 4320, app->runtime.window_height);
        else if (strcmp(line, "window_maximized") == 0)
            app->runtime.window_maximized = parse_boolean(
                equals, app->runtime.window_maximized);
        else if (strcmp(line, "last_tab") == 0)
            app->runtime.last_tab = validated_integer(
                equals, 0, LSM_TAB_COUNT - 1, app->runtime.last_tab);
        else if (strcmp(line, "tab_layout_version") == 0)
            tab_layout_version = validated_integer(
                equals, 0, LSM_TAB_LAYOUT_VERSION, 0);
        else if (strcmp(line, "performance_page") == 0 &&
                 valid_stack_name(equals))
            g_strlcpy(app->runtime.selected_performance_page, equals,
                      sizeof(app->runtime.selected_performance_page));
        else if (strncmp(line, "page_scroll_", 12U) == 0 &&
                 line[12] >= '0' && line[12] <= '7' && line[13] == '\0') {
            const size_t index = (size_t)(line[12] - '0');
            if (index < LSM_TAB_COUNT)
                app->runtime.page_scroll[index] = validated_double(
                    equals, 0.0, 1000000000.0, app->runtime.page_scroll[index]);
        }
    }
    fclose(file);
    app->runtime.last_tab = (gint)lsm_tab_index_migrate(
        app->runtime.last_tab, tab_layout_version);
}

static bool write_preferences(FILE *file, const void *user_data)
{
    const LsmApp *app = user_data;
    int result = fprintf(file,
        "# Linux System Monitor graphical preferences\n"
        "update_interval_ms=%u\n"
        "newer_on_right=%d\n"
        "network_use_bits=%d\n"
        "process_cpu_per_core=%d\n"
        "show_all_filesystems=%d\n"
        "process_heatmap=%d\n"
        "always_on_top=%d\n"
        "compact_summary=%d\n"
        "window_width=%d\n"
        "window_height=%d\n"
        "window_maximized=%d\n"
        "tab_layout_version=%d\n"
        "last_tab=%d\n",
        app->runtime.update_interval_ms, app->runtime.newer_on_right ? 1 : 0,
        app->runtime.network_use_bits ? 1 : 0,
        app->runtime.process_cpu_per_core ? 1 : 0,
        app->runtime.show_all_filesystems ? 1 : 0,
        app->details.process_heatmap ? 1 : 0,
        app->runtime.always_on_top ? 1 : 0,
        app->runtime.compact_summary ? 1 : 0,
        app->runtime.window_width, app->runtime.window_height,
        app->runtime.window_maximized ? 1 : 0, LSM_TAB_LAYOUT_VERSION, app->runtime.last_tab);
    bool okay = result >= 0;
    for (size_t index = 0U; okay && index < LSM_TAB_COUNT; index++)
        if (fprintf(file, "page_scroll_%zu=%.3f\n", index,
                    app->runtime.page_scroll[index]) < 0)
            okay = false;
    if (okay && fprintf(file, "performance_page=%s\n",
            app->runtime.selected_performance_page[0]
                ? app->runtime.selected_performance_page : "cpu") < 0)
        okay = false;
    return okay && ferror(file) == 0;
}

void lsm_preferences_save(const LsmApp *app)
{
    if (!app || !app->paths.preferences_path[0]) return;
    if (g_mkdir_with_parents(app->paths.config_dir, 0700) != 0) return;
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
    return index >= 0 && (size_t)index < G_N_ELEMENTS(intervals) ?
        intervals[index] : 1000U;
}

void lsm_preferences_show(LsmApp *app)
{
    if (!app) return;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Preferences",
        GTK_WINDOW(app->shell.window), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, "Apply", GTK_RESPONSE_ACCEPT, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 620, 430);
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
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(speed), "Fast — 0.5 seconds");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(speed), "Normal — 1 second");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(speed), "Low — 2 seconds");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(speed), "Very low — 5 seconds");
    gtk_combo_box_set_active(GTK_COMBO_BOX(speed), interval_index(app->runtime.update_interval_ms));
    attach_preference(GTK_GRID(grid), 0, "Performance refresh speed", speed);

    GtkWidget *network = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network), "Bytes per second — KB/s, MB/s");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(network), "Bits per second — Kb/s, Mb/s");
    gtk_combo_box_set_active(GTK_COMBO_BOX(network), app->runtime.network_use_bits ? 1 : 0);
    attach_preference(GTK_GRID(grid), 1, "Network units", network);

    GtkWidget *cpu_mode = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cpu_mode),
        "Total computer capacity — process maximum 100%");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(cpu_mode),
        "Per-core capacity — multi-threaded processes may exceed 100%");
    gtk_combo_box_set_active(GTK_COMBO_BOX(cpu_mode), app->runtime.process_cpu_per_core ? 1 : 0);
    attach_preference(GTK_GRID(grid), 2, "Process CPU scale", cpu_mode);

    GtkWidget *direction = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(direction), "New values on the right");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(direction), "New values on the left");
    gtk_combo_box_set_active(GTK_COMBO_BOX(direction), app->runtime.newer_on_right ? 0 : 1);
    attach_preference(GTK_GRID(grid), 3, "Graph direction", direction);

    GtkWidget *show_all = gtk_check_button_new_with_label(
        "Show virtual and system filesystems by default");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_all),
                                 app->runtime.show_all_filesystems);
    gtk_grid_attach(GTK_GRID(grid), show_all, 0, 4, 2, 1);
    GtkWidget *heatmap = gtk_check_button_new_with_label(
        "Shade busy resource cells in Processes and Details");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(heatmap), app->details.process_heatmap);
    gtk_grid_attach(GTK_GRID(grid), heatmap, 0, 5, 2, 1);
    GtkWidget *always_on_top = gtk_check_button_new_with_label(
        "Keep the monitor above other windows");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(always_on_top),
                                 app->runtime.always_on_top);
    gtk_grid_attach(GTK_GRID(grid), always_on_top, 0, 6, 2, 1);
    GtkWidget *compact_summary = gtk_check_button_new_with_label(
        "Open in compact summary mode");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compact_summary),
                                 app->runtime.compact_summary);
    gtk_grid_attach(GTK_GRID(grid), compact_summary, 0, 7, 2, 1);
    GtkWidget *cadence_note = gtk_label_new(
        "Performance graphs can refresh every 0.5 seconds. Process and "
        "management lists refresh no faster than once per second.");
    gtk_label_set_line_wrap(GTK_LABEL(cadence_note), TRUE);
    gtk_widget_set_halign(cadence_note, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), cadence_note, 0, 8, 2, 1);

    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        app->runtime.update_interval_ms = interval_from_index(
            gtk_combo_box_get_active(GTK_COMBO_BOX(speed)));
        app->runtime.network_use_bits = gtk_combo_box_get_active(GTK_COMBO_BOX(network)) == 1;
        app->runtime.process_cpu_per_core = gtk_combo_box_get_active(GTK_COMBO_BOX(cpu_mode)) == 1;
        app->runtime.newer_on_right = gtk_combo_box_get_active(GTK_COMBO_BOX(direction)) == 0;
        app->runtime.show_all_filesystems = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_all));
        app->details.process_heatmap = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(heatmap));
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
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->filesystem.filesystem_show_all),
                                         app->runtime.show_all_filesystems);
        lsm_preferences_save(app);
        lsm_app_preferences_changed(app);
        app->processes.processes_model_dirty = TRUE;
        app->details.details_model_dirty = TRUE;
        lsm_processes_present_snapshot(app);
        lsm_details_present_snapshot(app);
        lsm_performance_refresh(app);
        lsm_filesystems_refresh(app);
    }
    gtk_widget_destroy(dialog);
}
