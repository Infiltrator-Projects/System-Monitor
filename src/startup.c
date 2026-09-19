// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file startup.c
 * @brief GTK presentation for startup applications.
 *
 * Native discovery and reversible startup-state changes are delegated through
 * startup_backend.h. This module owns only selection, filtering and widgets.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "startup.h"
#include "app_internal.h"
#include "startup_backend.h"
#include "ui_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    STARTUP_COL_NAME,
    STARTUP_COL_STATUS,
    STARTUP_COL_SOURCE,
    STARTUP_COL_COMMAND,
    STARTUP_COL_DESCRIPTION,
    STARTUP_COL_ID,
    STARTUP_COL_SOURCE_IDENTITY,
    STARTUP_COL_ORIGIN_IDENTITY,
    STARTUP_COL_ENABLED,
    STARTUP_N_COLUMNS
};

static gboolean selected_startup(LsmApp *app, char **id,
                                 char **source_identity,
                                 char **origin_identity,
                                 gboolean *enabled)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(app->startup.startup_tree));
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) return FALSE;
    gtk_tree_model_get(model, &iter,
                       STARTUP_COL_ID, id,
                       STARTUP_COL_SOURCE_IDENTITY, source_identity,
                       STARTUP_COL_ORIGIN_IDENTITY, origin_identity,
                       STARTUP_COL_ENABLED, enabled,
                       -1);
    return TRUE;
}

static void startup_selection_changed(GtkTreeSelection *selection,
                                      gpointer user_data)
{
    LsmApp *app = user_data;
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    gboolean selected = gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_widget_set_sensitive(app->startup.startup_toggle_button, selected);
    gtk_widget_set_sensitive(app->startup.startup_open_button, selected);
    if (selected) {
        gboolean enabled = FALSE;
        gtk_tree_model_get(model, &iter, STARTUP_COL_ENABLED, &enabled, -1);
        gtk_button_set_label(GTK_BUTTON(app->startup.startup_toggle_button),
                             enabled ? "Disable" : "Enable");
    }
}

static void startup_toggle(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *id = NULL, *source_identity = NULL, *origin_identity = NULL;
    gboolean enabled = FALSE;
    if (!selected_startup(app, &id, &source_identity,
                          &origin_identity, &enabled))
        return;

    GError *error = NULL;
    if (!lsm_startup_backend_set_enabled(
            id, source_identity, origin_identity, !enabled, &error)) {
        lsm_ui_show_error(GTK_WINDOW(app->shell.window),
                          "Unable to change startup application", "%s",
                          error ? error->message :
                          "The desktop file could not be written.");
        if (error) g_error_free(error);
    }
    g_free(id);
    g_free(source_identity);
    g_free(origin_identity);
    lsm_startup_refresh(app);
}

static void startup_open_folder(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *id = NULL, *source_identity = NULL, *origin_identity = NULL;
    gboolean enabled = FALSE;
    if (!selected_startup(app, &id, &source_identity,
                          &origin_identity, &enabled))
        return;
    (void)enabled;
    char *uri = lsm_startup_backend_location_uri(source_identity);
    if (uri)
        gtk_show_uri_on_window(GTK_WINDOW(app->shell.window), uri,
                               GDK_CURRENT_TIME, NULL);
    g_free(uri);
    g_free(id);
    g_free(source_identity);
    g_free(origin_identity);
}

/* Search and GTK construction. */
static gboolean startup_search_timeout(gpointer user_data)
{
    LsmApp *app = user_data;
    app->startup.startup_search_timer = 0;
    lsm_startup_refresh(app);
    return G_SOURCE_REMOVE;
}

static void startup_search_changed(GtkEditable *editable, gpointer user_data)
{
    (void)editable;
    LsmApp *app = user_data;
    if (app->startup.startup_search_timer)
        g_source_remove(app->startup.startup_search_timer);
    app->startup.startup_search_timer = g_timeout_add(
        LSM_SEARCH_DEBOUNCE_MS, startup_search_timeout, app);
}

static void startup_refresh_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    lsm_startup_refresh(user_data);
}

static GtkTreeViewColumn *startup_column(GtkTreeView *tree, const char *title,
                                         int model_column, gboolean expand,
                                         int minimum_width)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        title, renderer, "text", model_column, NULL);
    gtk_tree_view_column_set_sort_column_id(column, model_column);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_expand(column, expand);
    gtk_tree_view_column_set_min_width(column, minimum_width);
    gtk_tree_view_append_column(tree, column);
    return column;
}

void lsm_startup_refresh(LsmApp *app)
{
    if (!app || !app->startup.startup_store) return;
    gtk_list_store_clear(app->startup.startup_store);
    const char *search = app->startup.startup_search
        ? gtk_entry_get_text(GTK_ENTRY(app->startup.startup_search)) : "";

    size_t count = 0U, visible = 0U;
    LsmStartupEntry *entries = NULL;
    if (!lsm_startup_backend_collect(&entries, &count)) {
        lsm_ui_set_label_text(app->startup.startup_count_label,
                              "Startup inventory unavailable");
        return;
    }
    for (size_t i = 0U; i < count; i++) {
        LsmStartupEntry *entry = &entries[i];
        if (*search && !lsm_ui_text_matches(entry->name, search) &&
            !lsm_ui_text_matches(entry->command, search) &&
            !lsm_ui_text_matches(entry->description, search) &&
            !lsm_ui_text_matches(entry->id, search)) continue;

        GtkTreeIter iter;
        gtk_list_store_append(app->startup.startup_store, &iter);
        gtk_list_store_set(app->startup.startup_store, &iter,
                           STARTUP_COL_NAME, entry->name,
                           STARTUP_COL_STATUS,
                           entry->enabled ? "Enabled" : "Disabled",
                           STARTUP_COL_SOURCE,
                           entry->user_entry ? "User" : "System",
                           STARTUP_COL_COMMAND, entry->command,
                           STARTUP_COL_DESCRIPTION, entry->description,
                           STARTUP_COL_ID, entry->id,
                           STARTUP_COL_SOURCE_IDENTITY, entry->source_identity,
                           STARTUP_COL_ORIGIN_IDENTITY, entry->origin_identity,
                           STARTUP_COL_ENABLED, entry->enabled,
                           -1);
        visible++;
    }
    lsm_startup_backend_free(entries);
    lsm_ui_set_label_text(app->startup.startup_count_label,
                          "%zu startup application%s", visible,
                          visible == 1 ? "" : "s");
    gtk_widget_set_sensitive(app->startup.startup_toggle_button, FALSE);
    gtk_widget_set_sensitive(app->startup.startup_open_button, FALSE);
}

void lsm_startup_build(LsmApp *app, GtkWidget *container)
{
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 8);
    app->startup.startup_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->startup.startup_search),
                                   "Search startup applications");
    gtk_widget_set_hexpand(app->startup.startup_search, TRUE);
    g_signal_connect(app->startup.startup_search, "changed",
                     G_CALLBACK(startup_search_changed), app);

    app->startup.startup_toggle_button = gtk_button_new_with_label("Enable");
    app->startup.startup_open_button = gtk_button_new_with_label("Open folder");
    GtkWidget *refresh = gtk_button_new_with_label("Refresh");
    gtk_widget_set_sensitive(app->startup.startup_toggle_button, FALSE);
    gtk_widget_set_sensitive(app->startup.startup_open_button, FALSE);
    g_signal_connect(app->startup.startup_toggle_button, "clicked",
                     G_CALLBACK(startup_toggle), app);
    g_signal_connect(app->startup.startup_open_button, "clicked",
                     G_CALLBACK(startup_open_folder), app);
    g_signal_connect(refresh, "clicked", G_CALLBACK(startup_refresh_clicked),
                     app);

    gtk_box_pack_start(GTK_BOX(toolbar), app->startup.startup_search,
                       TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->startup.startup_toggle_button,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->startup.startup_open_button,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), refresh, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), toolbar, FALSE, FALSE, 0);

    app->startup.startup_store = gtk_list_store_new(STARTUP_N_COLUMNS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_BOOLEAN);
    app->startup.startup_tree = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(app->startup.startup_store));
    gtk_tree_view_set_headers_clickable(
        GTK_TREE_VIEW(app->startup.startup_tree), TRUE);
    startup_column(GTK_TREE_VIEW(app->startup.startup_tree), "Name",
                   STARTUP_COL_NAME, FALSE, 180);
    startup_column(GTK_TREE_VIEW(app->startup.startup_tree), "Status",
                   STARTUP_COL_STATUS, FALSE, 85);
    startup_column(GTK_TREE_VIEW(app->startup.startup_tree), "Source",
                   STARTUP_COL_SOURCE, FALSE, 75);
    startup_column(GTK_TREE_VIEW(app->startup.startup_tree), "Command",
                   STARTUP_COL_COMMAND, TRUE, 280);
    startup_column(GTK_TREE_VIEW(app->startup.startup_tree), "Description",
                   STARTUP_COL_DESCRIPTION, TRUE, 220);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(app->startup.startup_tree));
    g_signal_connect(selection, "changed",
                     G_CALLBACK(startup_selection_changed), app);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    app->runtime.page_scrollers[LSM_TAB_STARTUP] = scroll;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), app->startup.startup_tree);
    gtk_box_pack_start(GTK_BOX(container), scroll, TRUE, TRUE, 0);

    app->startup.startup_count_label = gtk_label_new("");
    gtk_widget_set_halign(app->startup.startup_count_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->startup.startup_count_label, 8);
    gtk_widget_set_margin_bottom(app->startup.startup_count_label, 6);
    gtk_box_pack_start(GTK_BOX(container), app->startup.startup_count_label,
                       FALSE, FALSE, 0);
    lsm_startup_refresh(app);
}

void lsm_startup_destroy(LsmApp *app)
{
    if (!app) return;
    if (app->startup.startup_store) g_object_unref(app->startup.startup_store);
    app->startup.startup_store = NULL;
}
