// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file preferences.c
 * @brief Small, forward-compatible preferences persistence store.
 *
 * Preferences use a deliberately simple key=value file. The parser recognises
 * only project-owned keys, bounds every numeric value and keeps safe defaults
 * when input is malformed. Saving uses the application's durable atomic-file
 * provider, so a power loss cannot leave a partially written configuration.
 * Accepted dialog changes are committed here so the editor does not need the
 * private application layout or cross-subsystem refresh knowledge.
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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

static gboolean preference_page_scroll_index(const char *key,
                                             size_t *index_out)
{
    static const char prefix[] = "page_scroll_";
    int64_t parsed = 0;
    if (!key || !index_out || !lsm_string_starts_with(key, prefix) ||
        !infiltratr_parse_i64_range(
            key + sizeof(prefix) - 1U, 10U, 0,
            (int64_t)LSM_TAB_COUNT - 1, &parsed))
        return FALSE;
    *index_out = (size_t)parsed;
    return TRUE;
}

void lsm_preferences_load(LsmApp *app)
{
    if (!app || !app->paths.preferences_path[0]) return;
    char *content = NULL;
    size_t length = 0U;
    if (lsm_read_text_file_alloc(
            app->paths.preferences_path, &content, &length) !=
            INFILTRATR_IO_OK)
        return;
    if (memchr(content, '\0', length) != NULL) {
        free(content);
        return;
    }

    char *save = NULL;
    for (char *line = strtok_r(content, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save)) {
        char *key = NULL;
        char *value = NULL;
        size_t page_scroll_index = 0U;
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
        else if (preference_page_scroll_index(key, &page_scroll_index))
            app->runtime.page_scroll[page_scroll_index] = validated_double(
                value, 0.0, 1000000000.0,
                app->runtime.page_scroll[page_scroll_index]);
    }
    free(content);
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
    const int directory_failure =
        lsm_mkdir_parents(app->paths.config_dir, 0700U);
    if (directory_failure != 0) {
        fprintf(stderr, "Unable to create System Monitor config directory: %s\n",
                strerror(directory_failure));
        return;
    }
    const int failure = lsm_atomic_file_write(
        app->paths.preferences_path, LSM_ATOMIC_FILE_PRIVATE,
        write_preferences, app);
    if (failure != 0)
        fprintf(stderr, "Unable to save System Monitor preferences: %s\n",
                strerror(failure));
}

bool lsm_preferences_dialog_values(
    const LsmApp *app, LsmPreferencesDialogValues *values)
{
    if (!app || !values) return false;
    *values = (LsmPreferencesDialogValues) {
        .update_interval_ms = app->runtime.update_interval_ms,
        .filesystem_update_interval_ms = app->runtime.filesystem_update_interval_ms,
        .network_use_bits = app->runtime.network_use_bits,
        .network_total_separate = app->runtime.network_total_separate,
        .network_total_use_bits = app->runtime.network_total_use_bits,
        .process_cpu_per_core = app->runtime.process_cpu_per_core,
        .newer_on_right = app->runtime.newer_on_right,
        .graph_data_points = app->runtime.graph_data_points,
        .graph_smooth = app->runtime.graph_smooth,
        .cpu_stacked = app->runtime.cpu_stacked,
        .memory_logarithmic = app->runtime.memory_logarithmic,
        .confirm_process_actions = app->runtime.confirm_process_actions,
        .show_all_filesystems = app->runtime.show_all_filesystems,
        .process_heatmap = app->details.process_heatmap,
        .always_on_top = app->runtime.always_on_top,
        .compact_summary = app->runtime.compact_summary
    };
    return true;
}

void lsm_preferences_commit_dialog_values(
    LsmApp *app, const LsmPreferencesDialogValues *values)
{
    if (!app || !values) return;

    app->runtime.update_interval_ms = values->update_interval_ms;
    app->runtime.filesystem_update_interval_ms =
        values->filesystem_update_interval_ms;
    app->runtime.network_use_bits = values->network_use_bits;
    app->runtime.network_total_separate = values->network_total_separate;
    app->runtime.network_total_use_bits = values->network_total_use_bits;
    app->runtime.process_cpu_per_core = values->process_cpu_per_core;
    app->runtime.newer_on_right = values->newer_on_right;
    app->runtime.graph_data_points = values->graph_data_points;
    app->runtime.graph_smooth = values->graph_smooth;
    app->runtime.cpu_stacked = values->cpu_stacked;
    app->runtime.memory_logarithmic = values->memory_logarithmic;
    app->runtime.confirm_process_actions = values->confirm_process_actions;
    app->runtime.show_all_filesystems = values->show_all_filesystems;
    app->details.process_heatmap = values->process_heatmap;
    app->runtime.always_on_top = values->always_on_top;
    app->runtime.compact_summary = values->compact_summary;

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
