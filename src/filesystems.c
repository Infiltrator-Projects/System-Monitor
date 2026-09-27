// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file filesystems.c
 * @brief Native mountinfo/statvfs presentation for the File Systems tab.
 *
 * The page distinguishes ordinary storage and network filesystems from kernel
 * implementation mounts. The default view therefore remains useful on desktop
 * systems, while an explicit GUI switch exposes every mount for diagnostics.
 * No libmount dependency or command-line filesystem utility is required.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "filesystems.h"
#include "app_internal.h"

#include "common.h"
#include "atomic_file.h"
#include <infiltratr/config.h>
#include "filesystem_inventory.h"
#include "preferences.h"
#include "ui_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILESYSTEM_COLUMNS 8

enum {
    FS_COL_SOURCE,
    FS_COL_TARGET,
    FS_COL_TYPE,
    FS_COL_TOTAL,
    FS_COL_FREE,
    FS_COL_AVAILABLE,
    FS_COL_USED,
    FS_COL_USE_PERCENT
};

typedef struct {
    const char *key;
    const char *title;
    gboolean default_visible;
    gboolean expand;
    int minimum_width;
} FilesystemColumnSpec;

static const FilesystemColumnSpec filesystem_column_specs[FILESYSTEM_COLUMNS] = {
    {"source", "Device or source", TRUE, FALSE, 140},
    {"target", "Mount point", TRUE, TRUE, 180},
    {"type", "Type", TRUE, FALSE, 85},
    {"total", "Total", TRUE, FALSE, 90},
    {"free", "Free", FALSE, FALSE, 90},
    {"available", "Available", TRUE, FALSE, 90},
    {"used", "Used", TRUE, FALSE, 90},
    {"use", "Use", TRUE, FALSE, 70}
};

/** Plain-data filesystem inventory returned from the background collector. */
typedef struct {
    LsmFilesystemInfo *items; /**< Owned records transferred to page state. */
    size_t count; /**< Number of valid records in @ref items. */
} FilesystemRefreshResult;

static void filesystem_refresh_result_free(gpointer data)
{
    FilesystemRefreshResult *result = data;
    if (!result) return;
    lsm_filesystem_inventory_free(result->items);
    g_free(result);
}

static bool filesystem_matches_search(const LsmFilesystemInfo *item,
                                      const char *search)
{
    return !search || !*search ||
           lsm_ui_text_matches(item->source, search) ||
           lsm_ui_text_matches(item->target, search) ||
           lsm_ui_text_matches(item->filesystem, search);
}

static void append_filesystem(LsmApp *app, const LsmFilesystemInfo *item)
{
    char total_text[64] = "N/A", free_text[64] = "N/A";
    char used_text[64] = "N/A", available_text[64] = "N/A";
    char percent_text[32] = "N/A";
    if (item->capacity_available) {
        lsm_format_bytes(item->total_bytes, total_text, sizeof(total_text));
        lsm_format_bytes(item->free_bytes, free_text, sizeof(free_text));
        lsm_format_bytes(item->used_bytes, used_text, sizeof(used_text));
        lsm_format_bytes(item->available_bytes, available_text,
                         sizeof(available_text));
        snprintf(percent_text, sizeof(percent_text), "%u%%", item->used_percent);
    }

    GtkTreeIter iterator;
    gtk_list_store_append(app->filesystem.filesystem_store, &iterator);
    gtk_list_store_set(app->filesystem.filesystem_store, &iterator,
                       FS_COL_SOURCE, item->source,
                       FS_COL_TARGET, item->target,
                       FS_COL_TYPE, item->filesystem,
                       FS_COL_TOTAL, total_text,
                       FS_COL_FREE, free_text,
                       FS_COL_AVAILABLE, available_text,
                       FS_COL_USED, used_text,
                       FS_COL_USE_PERCENT, percent_text,
                       -1);
}

static GtkTreeViewColumn *add_column(GtkWidget *tree,
                                     const FilesystemColumnSpec *spec,
                                     int model_column)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        spec->title, renderer, "text", model_column, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, model_column);
    gtk_tree_view_column_set_expand(column, spec->expand);
    gtk_tree_view_column_set_min_width(column, spec->minimum_width);
    gtk_tree_view_column_set_visible(column, spec->default_visible);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
    return column;
}

static gboolean filesystem_layout_boolean(const char *value, gboolean fallback)
{
    bool parsed = false;
    return infiltratr_config_parse_bool(value, &parsed)
        ? (parsed ? TRUE : FALSE) : fallback;
}

static int filesystem_layout_integer(const char *value, int minimum,
                                     int maximum, int fallback)
{
    int64_t parsed = 0;
    return lsm_parse_i64_range(value, 10U, minimum, maximum, &parsed)
        ? (int)parsed : fallback;
}

static void filesystem_columns_save(const LsmApp *app)
{
    if (!app || !app->filesystem.filesystem_tree ||
        lsm_mkdir_parents(app->paths.config_dir, 0700U) != 0)
        return;

    gint order[FILESYSTEM_COLUMNS];
    for (int i = 0; i < FILESYSTEM_COLUMNS; i++) order[i] = i;
    GList *columns = gtk_tree_view_get_columns(
        GTK_TREE_VIEW(app->filesystem.filesystem_tree));
    int position = 0;
    for (GList *item = columns; item; item = item->next, position++) {
        for (int i = 0; i < FILESYSTEM_COLUMNS; i++) {
            if (item->data == app->filesystem.filesystem_columns[i]) {
                order[i] = position;
                break;
            }
        }
    }
    g_list_free(columns);

    GString *text = g_string_new("layout_version=1\n");
    for (int i = 0; i < FILESYSTEM_COLUMNS; i++) {
        g_string_append_printf(text, "%s=%d\n",
            filesystem_column_specs[i].key,
            gtk_tree_view_column_get_visible(
                app->filesystem.filesystem_columns[i]) ? 1 : 0);
        g_string_append_printf(text, "width.%s=%d\n",
            filesystem_column_specs[i].key,
            gtk_tree_view_column_get_width(
                app->filesystem.filesystem_columns[i]));
        g_string_append_printf(text, "order.%s=%d\n",
            filesystem_column_specs[i].key, order[i]);
    }
    gint sort_column = -1;
    GtkSortType sort_order = GTK_SORT_ASCENDING;
    if (gtk_tree_sortable_get_sort_column_id(
            GTK_TREE_SORTABLE(app->filesystem.filesystem_store),
            &sort_column, &sort_order)) {
        g_string_append_printf(text, "sort_column=%d\nsort_order=%s\n",
            sort_column,
            sort_order == GTK_SORT_DESCENDING ? "descending" : "ascending");
    }
    const int failure = lsm_atomic_file_write_bytes(
        app->paths.filesystem_column_path, LSM_ATOMIC_FILE_PRIVATE,
        text->str, text->len);
    if (failure != 0)
        fprintf(stderr, "Unable to save filesystem column layout: %s\n",
                strerror(failure));
    g_string_free(text, TRUE);
}

static void filesystem_columns_load(LsmApp *app)
{
    gboolean visible[FILESYSTEM_COLUMNS];
    gint widths[FILESYSTEM_COLUMNS];
    gint order[FILESYSTEM_COLUMNS];
    gint sort_column = -1;
    GtkSortType sort_order = GTK_SORT_ASCENDING;
    for (int i = 0; i < FILESYSTEM_COLUMNS; i++) {
        visible[i] = filesystem_column_specs[i].default_visible;
        widths[i] = filesystem_column_specs[i].minimum_width;
        order[i] = i;
    }

    char *content = NULL;
    size_t length = 0U;
    if (lsm_read_text_file_alloc(app->paths.filesystem_column_path,
                                 &content, &length) == INFILTRATR_IO_OK) {
        char *save = NULL;
        for (char *line = strtok_r(content, "\n", &save); line;
             line = strtok_r(NULL, "\n", &save)) {
            char *key = NULL, *value = NULL;
            if (infiltratr_config_parse_line(line, &key, &value) !=
                INFILTRATR_CONFIG_LINE_ENTRY)
                continue;
            if (strcmp(key, "sort_column") == 0) {
                sort_column = filesystem_layout_integer(
                    value, 0, FILESYSTEM_COLUMNS - 1, sort_column);
                continue;
            }
            if (strcmp(key, "sort_order") == 0) {
                if (strcmp(value, "descending") == 0)
                    sort_order = GTK_SORT_DESCENDING;
                else if (strcmp(value, "ascending") == 0)
                    sort_order = GTK_SORT_ASCENDING;
                continue;
            }
            for (int i = 0; i < FILESYSTEM_COLUMNS; i++) {
                char width_key[64], order_key[64];
                snprintf(width_key, sizeof(width_key), "width.%s",
                         filesystem_column_specs[i].key);
                snprintf(order_key, sizeof(order_key), "order.%s",
                         filesystem_column_specs[i].key);
                if (strcmp(key, filesystem_column_specs[i].key) == 0) {
                    visible[i] = filesystem_layout_boolean(value, visible[i]);
                    break;
                }
                if (strcmp(key, width_key) == 0) {
                    widths[i] = filesystem_layout_integer(
                        value, 40, 2000, widths[i]);
                    break;
                }
                if (strcmp(key, order_key) == 0) {
                    order[i] = filesystem_layout_integer(
                        value, 0, FILESYSTEM_COLUMNS - 1, order[i]);
                    break;
                }
            }
        }
        free(content);
    }

    for (int i = 0; i < FILESYSTEM_COLUMNS; i++) {
        gtk_tree_view_column_set_visible(
            app->filesystem.filesystem_columns[i], visible[i]);
        gtk_tree_view_column_set_sizing(
            app->filesystem.filesystem_columns[i],
            GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_fixed_width(
            app->filesystem.filesystem_columns[i], widths[i]);
    }
    GtkTreeViewColumn *previous = NULL;
    for (int position = 0; position < FILESYSTEM_COLUMNS; position++) {
        for (int i = 0; i < FILESYSTEM_COLUMNS; i++) {
            if (order[i] != position) continue;
            gtk_tree_view_move_column_after(
                GTK_TREE_VIEW(app->filesystem.filesystem_tree),
                app->filesystem.filesystem_columns[i], previous);
            previous = app->filesystem.filesystem_columns[i];
            break;
        }
    }
    if (sort_column >= 0)
        gtk_tree_sortable_set_sort_column_id(
            GTK_TREE_SORTABLE(app->filesystem.filesystem_store),
            sort_column, sort_order);
}

static void filesystem_columns_dialog(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Select file-system columns", GTK_WINDOW(app->shell.window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, "Apply", GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    GtkWidget *checks[FILESYSTEM_COLUMNS];
    for (int i = 0; i < FILESYSTEM_COLUMNS; i++) {
        checks[i] = gtk_check_button_new_with_label(
            filesystem_column_specs[i].title);
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(checks[i]),
            gtk_tree_view_column_get_visible(
                app->filesystem.filesystem_columns[i]));
        gtk_grid_attach(GTK_GRID(grid), checks[i], 0, i, 1, 1);
    }
    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        for (int i = 0; i < FILESYSTEM_COLUMNS; i++)
            gtk_tree_view_column_set_visible(
                app->filesystem.filesystem_columns[i],
                gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checks[i])));
        filesystem_columns_save(app);
    }
    gtk_widget_destroy(dialog);
}

static void filesystem_row_activated(GtkTreeView *tree, GtkTreePath *path,
                                     GtkTreeViewColumn *column,
                                     gpointer user_data)
{
    (void)column;
    LsmApp *app = user_data;
    GtkTreeModel *model = gtk_tree_view_get_model(tree);
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter(model, &iter, path)) return;
    gchar *target = NULL;
    gtk_tree_model_get(model, &iter, FS_COL_TARGET, &target, -1);
    if (!target || !*target) {
        g_free(target);
        return;
    }

    GError *error = NULL;
    char *uri = g_filename_to_uri(target, NULL, &error);
    if (!uri || !gtk_show_uri_on_window(
                    GTK_WINDOW(app->shell.window), uri,
                    GDK_CURRENT_TIME, &error)) {
        lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                          "Unable to open mount point", "%s",
                          error ? error->message : "Unknown error");
    }
    if (error) g_error_free(error);
    g_free(uri);
    g_free(target);
}

static void present_filesystem_snapshot(LsmApp *app)
{
    if (!app || !app->filesystem.filesystem_store) return;
    gtk_list_store_clear(app->filesystem.filesystem_store);
    const char *search = app->filesystem.filesystem_search ?
        gtk_entry_get_text(GTK_ENTRY(app->filesystem.filesystem_search)) : "";
    size_t visible_count = 0U;
    for (size_t index = 0U;
         index < app->filesystem.filesystem_snapshot_count; index++) {
        const LsmFilesystemInfo *item =
            &app->filesystem.filesystem_snapshot[index];
        if (!app->runtime.show_all_filesystems && !item->normally_visible)
            continue;
        if (!filesystem_matches_search(item, search)) continue;
        append_filesystem(app, item);
        visible_count++;
    }
    lsm_ui_set_label_text(app->filesystem.filesystem_count_label,
                          "File systems: %zu", visible_count);
}

static void search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    present_filesystem_snapshot(user_data);
}

static void show_all_toggled(GtkToggleButton *button, gpointer user_data)
{
    LsmApp *app = user_data;
    app->runtime.show_all_filesystems = gtk_toggle_button_get_active(button);
    lsm_preferences_save(app);
    present_filesystem_snapshot(app);
}

static void filesystem_refresh_worker(GTask *task, gpointer source_object,
                                      gpointer task_data,
                                      GCancellable *cancellable)
{
    (void)source_object;
    (void)task_data;
    (void)cancellable;
    FilesystemRefreshResult *result = g_new0(FilesystemRefreshResult, 1U);
    result->count = lsm_filesystem_inventory_collect(&result->items);
    g_task_return_pointer(task, result, filesystem_refresh_result_free);
}

static void filesystem_refresh_complete(GObject *source_object,
                                        GAsyncResult *async_result,
                                        gpointer user_data)
{
    (void)user_data;
    FilesystemRefreshResult *result = g_task_propagate_pointer(
        G_TASK(async_result), NULL);
    LsmApp *app = source_object ?
        g_object_get_data(source_object, "lsm-filesystem-app") : NULL;
    if (!app) {
        filesystem_refresh_result_free(result);
        return;
    }

    app->filesystem.refresh_pending = FALSE;
    if (result) {
        lsm_filesystem_inventory_free(app->filesystem.filesystem_snapshot);
        app->filesystem.filesystem_snapshot = result->items;
        app->filesystem.filesystem_snapshot_count = result->count;
        result->items = NULL;
        filesystem_refresh_result_free(result);
        present_filesystem_snapshot(app);
    }

    if (app->filesystem.refresh_again && !app->runtime.shutting_down) {
        app->filesystem.refresh_again = FALSE;
        lsm_filesystems_refresh(app);
    }
}

void lsm_filesystems_build(LsmApp *app, GtkWidget *container)
{
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(outer), 8);
    gtk_container_add(GTK_CONTAINER(container), outer);
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    app->filesystem.filesystem_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->filesystem.filesystem_search),
                                   "Search device, mount point, or type");
    gtk_widget_set_hexpand(app->filesystem.filesystem_search, TRUE);
    app->filesystem.filesystem_show_all = gtk_check_button_new_with_label(
        "Show virtual and system filesystems");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->filesystem.filesystem_show_all),
                                 app->runtime.show_all_filesystems);
    app->filesystem.filesystem_columns_button =
        gtk_button_new_with_label("Columns…");
    app->filesystem.filesystem_count_label = gtk_label_new("File systems: 0");
    gtk_box_pack_start(GTK_BOX(toolbar), app->filesystem.filesystem_search, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->filesystem.filesystem_columns_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->filesystem.filesystem_show_all, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), toolbar, FALSE, FALSE, 0);

    app->filesystem.filesystem_store = gtk_list_store_new(FILESYSTEM_COLUMNS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    g_object_set_data(G_OBJECT(app->filesystem.filesystem_store),
                      "lsm-filesystem-app", app);
    app->filesystem.filesystem_tree = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(app->filesystem.filesystem_store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(app->filesystem.filesystem_tree), TRUE);
    for (int i = 0; i < FILESYSTEM_COLUMNS; i++)
        app->filesystem.filesystem_columns[i] = add_column(
            app->filesystem.filesystem_tree,
            &filesystem_column_specs[i], i);
    filesystem_columns_load(app);
    GtkWidget *scroller = gtk_scrolled_window_new(NULL, NULL);
    app->runtime.page_scrollers[LSM_TAB_FILESYSTEMS] = scroller;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroller, TRUE);
    gtk_container_add(GTK_CONTAINER(scroller), app->filesystem.filesystem_tree);
    gtk_box_pack_start(GTK_BOX(outer), scroller, TRUE, TRUE, 0);
    GtkWidget *note = gtk_label_new(
        "Capacity is read directly from each mounted filesystem. Unmounted partitions remain available on the Performance disk pages.");
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    gtk_label_set_line_wrap(GTK_LABEL(note), TRUE);
    gtk_box_pack_start(GTK_BOX(outer), note, FALSE, FALSE, 0);
    gtk_widget_set_halign(app->filesystem.filesystem_count_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->filesystem.filesystem_count_label, 4);
    gtk_box_pack_start(GTK_BOX(outer), app->filesystem.filesystem_count_label,
                       FALSE, FALSE, 0);
    g_signal_connect(app->filesystem.filesystem_search, "changed",
                     G_CALLBACK(search_changed), app);
    g_signal_connect(app->filesystem.filesystem_show_all, "toggled",
                     G_CALLBACK(show_all_toggled), app);
    g_signal_connect(app->filesystem.filesystem_columns_button, "clicked",
                     G_CALLBACK(filesystem_columns_dialog), app);
    g_signal_connect(app->filesystem.filesystem_tree, "row-activated",
                     G_CALLBACK(filesystem_row_activated), app);
}

void lsm_filesystems_refresh(LsmApp *app)
{
    if (!app || !app->filesystem.filesystem_store || app->runtime.shutting_down)
        return;
    if (app->filesystem.refresh_pending) {
        app->filesystem.refresh_again = TRUE;
        return;
    }

    app->filesystem.refresh_pending = TRUE;
    GTask *task = g_task_new(G_OBJECT(app->filesystem.filesystem_store), NULL,
                             filesystem_refresh_complete, NULL);
    g_task_run_in_thread(task, filesystem_refresh_worker);
    g_object_unref(task);
}

void lsm_filesystems_destroy(LsmApp *app)
{
    if (!app) return;
    if (app->filesystem.filesystem_store) {
        g_object_set_data(G_OBJECT(app->filesystem.filesystem_store),
                          "lsm-filesystem-app", NULL);
        g_object_unref(app->filesystem.filesystem_store);
    }
    lsm_filesystem_inventory_free(app->filesystem.filesystem_snapshot);
    app->filesystem.filesystem_snapshot = NULL;
    app->filesystem.filesystem_snapshot_count = 0U;
    app->filesystem.refresh_pending = FALSE;
    app->filesystem.refresh_again = FALSE;
    app->filesystem.filesystem_store = NULL;
    app->filesystem.filesystem_tree = NULL;
    app->filesystem.filesystem_search = NULL;
    app->filesystem.filesystem_show_all = NULL;
    app->filesystem.filesystem_columns_button = NULL;
    memset(app->filesystem.filesystem_columns, 0,
           sizeof(app->filesystem.filesystem_columns));
    app->filesystem.filesystem_count_label = NULL;
}

gboolean lsm_filesystems_update(gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app || app->runtime.paused) return G_SOURCE_CONTINUE;
    if (!app->shell.notebook ||
        gtk_notebook_get_current_page(GTK_NOTEBOOK(app->shell.notebook)) ==
            LSM_TAB_FILESYSTEMS)
        lsm_filesystems_refresh(app);
    return G_SOURCE_CONTINUE;
}
