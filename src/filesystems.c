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
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "filesystems.h"
#include "app_internal.h"

#include "common.h"
#include "filesystem_inventory.h"
#include "preferences.h"
#include "ui_helpers.h"

#include <stdio.h>
#include <string.h>

#define FILESYSTEM_COLUMNS 7

enum {
    FS_COL_SOURCE,
    FS_COL_TARGET,
    FS_COL_TYPE,
    FS_COL_TOTAL,
    FS_COL_USED,
    FS_COL_AVAILABLE,
    FS_COL_USE_PERCENT
};

typedef struct {
    LsmFilesystemInfo *items;
    size_t count;
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
    char total_text[64] = "N/A", used_text[64] = "N/A";
    char available_text[64] = "N/A", percent_text[32] = "N/A";
    if (item->capacity_available) {
        lsm_format_bytes(item->total_bytes, total_text, sizeof(total_text));
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
                       FS_COL_USED, used_text,
                       FS_COL_AVAILABLE, available_text,
                       FS_COL_USE_PERCENT, percent_text,
                       -1);
}

static void add_column(GtkWidget *tree, const char *title, int model_column,
                       gboolean expand)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        title, renderer, "text", model_column, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, model_column);
    gtk_tree_view_column_set_expand(column, expand);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);
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
    app->filesystem.filesystem_count_label = gtk_label_new("File systems: 0");
    gtk_box_pack_start(GTK_BOX(toolbar), app->filesystem.filesystem_search, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->filesystem.filesystem_show_all, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), toolbar, FALSE, FALSE, 0);

    app->filesystem.filesystem_store = gtk_list_store_new(FILESYSTEM_COLUMNS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    g_object_set_data(G_OBJECT(app->filesystem.filesystem_store),
                      "lsm-filesystem-app", app);
    app->filesystem.filesystem_tree = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(app->filesystem.filesystem_store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(app->filesystem.filesystem_tree), TRUE);
    add_column(app->filesystem.filesystem_tree, "Device or source", FS_COL_SOURCE, FALSE);
    add_column(app->filesystem.filesystem_tree, "Mount point", FS_COL_TARGET, TRUE);
    add_column(app->filesystem.filesystem_tree, "Type", FS_COL_TYPE, FALSE);
    add_column(app->filesystem.filesystem_tree, "Total", FS_COL_TOTAL, FALSE);
    add_column(app->filesystem.filesystem_tree, "Used", FS_COL_USED, FALSE);
    add_column(app->filesystem.filesystem_tree, "Available", FS_COL_AVAILABLE, FALSE);
    add_column(app->filesystem.filesystem_tree, "Use", FS_COL_USE_PERCENT, FALSE);
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
    app->filesystem.filesystem_count_label = NULL;
}

gboolean lsm_filesystems_update(gpointer user_data)
{
    LsmApp *app = user_data;
    if (!app->runtime.paused) lsm_filesystems_refresh(app);
    return G_SOURCE_CONTINUE;
}
