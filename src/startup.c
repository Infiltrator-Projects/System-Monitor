// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file startup.c
 * @brief XDG Startup Applications discovery and enable/disable controls.
 *
 * Startup entries are read from the standard per-user and system XDG
 * autostart directories.  System entries are never edited in place: a user
 * override with the same desktop-file ID is written under
 * ~/.config/autostart instead.  This matches the XDG autostart override model
 * and makes every change reversible without administrator privileges.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "startup.h"
#include "app.h"
#include "app_config.h"
#include "atomic_file.h"
#include "ui_helpers.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
    STARTUP_COL_NAME,
    STARTUP_COL_STATUS,
    STARTUP_COL_SOURCE,
    STARTUP_COL_COMMAND,
    STARTUP_COL_DESCRIPTION,
    STARTUP_COL_ID,
    STARTUP_COL_PATH,
    STARTUP_COL_ORIGIN_PATH,
    STARTUP_COL_ENABLED,
    STARTUP_N_COLUMNS
};

typedef struct {
    char id[LSM_NAME_LEN];
    char path[LSM_PATH_LEN];
    char origin_path[LSM_PATH_LEN];
    char name[LSM_NAME_LEN];
    char command[1024];
    char description[512];
    gboolean user_entry;
    gboolean enabled;
} StartupEntry;

/* Desktop-file parsing follows XDG override semantics and never executes Exec. */
static gboolean desktop_boolean(GKeyFile *file, const char *key, gboolean fallback)
{
    GError *error = NULL;
    gboolean value = g_key_file_get_boolean(file, "Desktop Entry", key, &error);
    if (error) {
        g_error_free(error);
        return fallback;
    }
    return value;
}

static char *desktop_string(GKeyFile *file, const char *key)
{
    GError *error = NULL;
    char *value = g_key_file_get_locale_string(file, "Desktop Entry", key, NULL, &error);
    if (error) {
        g_error_free(error);
        return g_strdup("");
    }
    return value ? value : g_strdup("");
}

static gboolean load_startup_entry(const char *path, const char *id,
                                   gboolean user_entry, StartupEntry *entry)
{
    GKeyFile *file = g_key_file_new();
    GError *error = NULL;
    if (!g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, &error)) {
        if (error) g_error_free(error);
        g_key_file_free(file);
        return FALSE;
    }

    char *type = desktop_string(file, "Type");
    if (*type && strcmp(type, "Application") != 0) {
        g_free(type);
        g_key_file_free(file);
        return FALSE;
    }
    g_free(type);

    char *name = desktop_string(file, "Name");
    char *command = desktop_string(file, "Exec");
    char *description = desktop_string(file, "Comment");
    gboolean hidden = desktop_boolean(file, "Hidden", FALSE);
    gboolean gnome_enabled = desktop_boolean(file, "X-GNOME-Autostart-enabled", TRUE);

    memset(entry, 0, sizeof(*entry));
    g_strlcpy(entry->id, id, sizeof(entry->id));
    g_strlcpy(entry->path, path, sizeof(entry->path));
    g_strlcpy(entry->origin_path, path, sizeof(entry->origin_path));
    g_strlcpy(entry->name, *name ? name : id, sizeof(entry->name));
    g_strlcpy(entry->command, command, sizeof(entry->command));
    g_strlcpy(entry->description, description, sizeof(entry->description));
    entry->user_entry = user_entry;
    entry->enabled = !hidden && gnome_enabled;

    g_free(name);
    g_free(command);
    g_free(description);
    g_key_file_free(file);
    return TRUE;
}

static ssize_t find_entry(StartupEntry *entries, size_t count, const char *id)
{
    for (size_t i = 0; i < count; i++)
        if (strcmp(entries[i].id, id) == 0) return (ssize_t)i;
    return -1;
}

static gboolean append_entry(StartupEntry **entries, size_t *count, size_t *capacity,
                             const StartupEntry *entry)
{
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 32;
        StartupEntry *grown = realloc(*entries, next * sizeof(*grown));
        if (!grown) return FALSE;
        *entries = grown;
        *capacity = next;
    }
    (*entries)[(*count)++] = *entry;
    return TRUE;
}

/* User entries are scanned first.  A desktop file with the same basename in
 * ~/.config/autostart overrides the system entry, exactly as required by the
 * XDG autostart specification. */
static void scan_directory(const char *directory, gboolean user_entry,
                           StartupEntry **entries, size_t *count, size_t *capacity)
{
    DIR *dir = opendir(directory);
    if (!dir) return;

    struct dirent *item;
    while ((item = readdir(dir))) {
        size_t length = strlen(item->d_name);
        if (length < 9 || strcmp(item->d_name + length - 8, ".desktop") != 0) continue;
        if (find_entry(*entries, *count, item->d_name) >= 0) continue;

        char path[LSM_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", directory, item->d_name);
        StartupEntry entry;
        if (load_startup_entry(path, item->d_name, user_entry, &entry))
            append_entry(entries, count, capacity, &entry);
    }
    closedir(dir);
}

/* Later user entries replace system entries with the same desktop-file ID. */
static StartupEntry *collect_entries(size_t *out_count)
{
    StartupEntry *entries = NULL;
    size_t count = 0, capacity = 0;
    char user_directory[LSM_PATH_LEN];
    snprintf(user_directory, sizeof(user_directory), "%s/autostart", g_get_user_config_dir());
    scan_directory(user_directory, TRUE, &entries, &count, &capacity);

    const char *xdg_dirs = getenv("XDG_CONFIG_DIRS");
    if (!xdg_dirs || !*xdg_dirs) xdg_dirs = "/etc/xdg";
    char *copy = strdup(xdg_dirs);
    if (copy) {
        char *save = NULL;
        for (char *dir = strtok_r(copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
            char path[LSM_PATH_LEN];
            snprintf(path, sizeof(path), "%s/autostart", dir);
            scan_directory(path, FALSE, &entries, &count, &capacity);
        }
        free(copy);
    }
    *out_count = count;
    return entries;
}

static gboolean selected_startup(LsmApp *app, char **id, char **path,
                                 char **origin_path, gboolean *enabled)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->startup.startup_tree));
    GtkTreeModel *model = NULL;
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) return FALSE;
    gtk_tree_model_get(model, &iter,
                       STARTUP_COL_ID, id,
                       STARTUP_COL_PATH, path,
                       STARTUP_COL_ORIGIN_PATH, origin_path,
                       STARTUP_COL_ENABLED, enabled,
                       -1);
    return TRUE;
}

/* System entries are disabled through a user-owned Hidden=true override. */
static gboolean write_startup_override(const char *source_path, const char *desktop_id,
                                       gboolean enable, GError **error)
{
    GKeyFile *file = g_key_file_new();
    if (!g_key_file_load_from_file(file, source_path, G_KEY_FILE_KEEP_COMMENTS, error)) {
        g_key_file_free(file);
        return FALSE;
    }
    g_key_file_set_boolean(file, "Desktop Entry", "Hidden", !enable);
    g_key_file_set_boolean(file, "Desktop Entry", "X-GNOME-Autostart-enabled", enable);

    gsize length = 0;
    char *data = g_key_file_to_data(file, &length, error);
    g_key_file_free(file);
    if (!data) return FALSE;

    char directory[LSM_PATH_LEN];
    snprintf(directory, sizeof(directory), "%s/autostart", g_get_user_config_dir());
    if (g_mkdir_with_parents(directory, 0755) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Unable to create %s: %s", directory, g_strerror(errno));
        g_free(data);
        return FALSE;
    }

    char *target = g_build_filename(directory, desktop_id, NULL);
    const int failure = lsm_atomic_file_write_bytes(
        target, LSM_ATOMIC_FILE_USER_DOCUMENT, data, length);
    gboolean result = failure == 0;
    if (!result)
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(failure),
                    "Unable to write %s: %s", target, g_strerror(failure));
    g_free(target);
    g_free(data);
    return result;
}

static void startup_selection_changed(GtkTreeSelection *selection, gpointer user_data)
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
        gtk_button_set_label(GTK_BUTTON(app->startup.startup_toggle_button), enabled ? "Disable" : "Enable");
    }
}

static void startup_toggle(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *id = NULL, *path = NULL, *origin = NULL;
    gboolean enabled = FALSE;
    if (!selected_startup(app, &id, &path, &origin, &enabled)) return;

    GError *error = NULL;
    const char *source = (origin && *origin) ? origin : path;
    if (!write_startup_override(source, id, !enabled, &error)) {
        lsm_ui_show_error(GTK_WINDOW(app->shell.window), "Unable to change startup application", "%s",
                      error ? error->message : "The desktop file could not be written.");
        if (error) g_error_free(error);
    }
    g_free(id);
    g_free(path);
    g_free(origin);
    lsm_startup_refresh(app);
}

static void startup_open_folder(GtkButton *button, gpointer user_data)
{
    (void)button;
    LsmApp *app = user_data;
    char *id = NULL, *path = NULL, *origin = NULL;
    gboolean enabled = FALSE;
    if (!selected_startup(app, &id, &path, &origin, &enabled)) return;
    (void)enabled;
    char *directory = g_path_get_dirname(path);
    char *uri = g_filename_to_uri(directory, NULL, NULL);
    if (uri) gtk_show_uri_on_window(GTK_WINDOW(app->shell.window), uri, GDK_CURRENT_TIME, NULL);
    g_free(uri);
    g_free(directory);
    g_free(id);
    g_free(path);
    g_free(origin);
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
    if (app->startup.startup_search_timer) g_source_remove(app->startup.startup_search_timer);
    app->startup.startup_search_timer = g_timeout_add(LSM_SEARCH_DEBOUNCE_MS, startup_search_timeout, app);
}

static void startup_refresh_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    lsm_startup_refresh(user_data);
}

static GtkTreeViewColumn *startup_column(GtkTreeView *tree, const char *title, int model_column,
                                         gboolean expand, int minimum_width)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(title, renderer,
                                                                         "text", model_column, NULL);
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
    const char *search = app->startup.startup_search ? gtk_entry_get_text(GTK_ENTRY(app->startup.startup_search)) : "";

    size_t count = 0, visible = 0;
    StartupEntry *entries = collect_entries(&count);
    for (size_t i = 0; i < count; i++) {
        StartupEntry *entry = &entries[i];
        if (*search && !lsm_ui_text_matches(entry->name, search) &&
            !lsm_ui_text_matches(entry->command, search) &&
            !lsm_ui_text_matches(entry->description, search) &&
            !lsm_ui_text_matches(entry->id, search)) continue;

        GtkTreeIter iter;
        gtk_list_store_append(app->startup.startup_store, &iter);
        gtk_list_store_set(app->startup.startup_store, &iter,
                           STARTUP_COL_NAME, entry->name,
                           STARTUP_COL_STATUS, entry->enabled ? "Enabled" : "Disabled",
                           STARTUP_COL_SOURCE, entry->user_entry ? "User" : "System",
                           STARTUP_COL_COMMAND, entry->command,
                           STARTUP_COL_DESCRIPTION, entry->description,
                           STARTUP_COL_ID, entry->id,
                           STARTUP_COL_PATH, entry->path,
                           STARTUP_COL_ORIGIN_PATH, entry->origin_path,
                           STARTUP_COL_ENABLED, entry->enabled,
                           -1);
        visible++;
    }
    free(entries);
    lsm_ui_set_label_text(app->startup.startup_count_label, "%zu startup application%s",
                       visible, visible == 1 ? "" : "s");
    gtk_widget_set_sensitive(app->startup.startup_toggle_button, FALSE);
    gtk_widget_set_sensitive(app->startup.startup_open_button, FALSE);
}

void lsm_startup_build(LsmApp *app, GtkWidget *container)
{
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(toolbar), 8);
    app->startup.startup_search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->startup.startup_search), "Search startup applications");
    gtk_widget_set_hexpand(app->startup.startup_search, TRUE);
    g_signal_connect(app->startup.startup_search, "changed", G_CALLBACK(startup_search_changed), app);

    app->startup.startup_toggle_button = gtk_button_new_with_label("Enable");
    app->startup.startup_open_button = gtk_button_new_with_label("Open folder");
    GtkWidget *refresh = gtk_button_new_with_label("Refresh");
    gtk_widget_set_sensitive(app->startup.startup_toggle_button, FALSE);
    gtk_widget_set_sensitive(app->startup.startup_open_button, FALSE);
    g_signal_connect(app->startup.startup_toggle_button, "clicked", G_CALLBACK(startup_toggle), app);
    g_signal_connect(app->startup.startup_open_button, "clicked", G_CALLBACK(startup_open_folder), app);
    g_signal_connect(refresh, "clicked", G_CALLBACK(startup_refresh_clicked), app);

    gtk_box_pack_start(GTK_BOX(toolbar), app->startup.startup_search, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->startup.startup_toggle_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), app->startup.startup_open_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(toolbar), refresh, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(container), toolbar, FALSE, FALSE, 0);

    app->startup.startup_store = gtk_list_store_new(STARTUP_N_COLUMNS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    app->startup.startup_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->startup.startup_store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(app->startup.startup_tree), TRUE);
    startup_column(GTK_TREE_VIEW(app->startup.startup_tree), "Name", STARTUP_COL_NAME, FALSE, 180);
    startup_column(GTK_TREE_VIEW(app->startup.startup_tree), "Status", STARTUP_COL_STATUS, FALSE, 85);
    startup_column(GTK_TREE_VIEW(app->startup.startup_tree), "Source", STARTUP_COL_SOURCE, FALSE, 75);
    startup_column(GTK_TREE_VIEW(app->startup.startup_tree), "Command", STARTUP_COL_COMMAND, TRUE, 280);
    startup_column(GTK_TREE_VIEW(app->startup.startup_tree), "Description", STARTUP_COL_DESCRIPTION, TRUE, 220);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->startup.startup_tree));
    g_signal_connect(selection, "changed", G_CALLBACK(startup_selection_changed), app);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    app->runtime.page_scrollers[LSM_TAB_STARTUP] = scroll;
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), app->startup.startup_tree);
    gtk_box_pack_start(GTK_BOX(container), scroll, TRUE, TRUE, 0);

    app->startup.startup_count_label = gtk_label_new("");
    gtk_widget_set_halign(app->startup.startup_count_label, GTK_ALIGN_START);
    gtk_widget_set_margin_start(app->startup.startup_count_label, 8);
    gtk_widget_set_margin_bottom(app->startup.startup_count_label, 6);
    gtk_box_pack_start(GTK_BOX(container), app->startup.startup_count_label, FALSE, FALSE, 0);
    lsm_startup_refresh(app);
}

void lsm_startup_destroy(LsmApp *app)
{
    if (!app) return;
    if (app->startup.startup_store) g_object_unref(app->startup.startup_store);
    app->startup.startup_store = NULL;
}
