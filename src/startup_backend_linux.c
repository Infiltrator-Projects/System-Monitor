// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file startup_backend_linux.c
 * @brief Linux/XDG startup discovery and reversible per-user overrides.
 *
 * Entries are read directly from XDG autostart directories. System entries are
 * never edited in place: changes are represented by a user-owned override with
 * the same desktop-file ID. Exec metadata is never executed by this backend.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "startup_backend.h"

#include "atomic_file.h"
#include "common.h"

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static gboolean desktop_boolean(GKeyFile *file, const char *key,
                                gboolean fallback)
{
    GError *error = NULL;
    const gboolean value =
        g_key_file_get_boolean(file, "Desktop Entry", key, &error);
    if (error) {
        g_error_free(error);
        return fallback;
    }
    return value;
}

static char *desktop_string(GKeyFile *file, const char *key)
{
    GError *error = NULL;
    char *value = g_key_file_get_locale_string(
        file, "Desktop Entry", key, NULL, &error);
    if (error) {
        g_error_free(error);
        return g_strdup("");
    }
    return value ? value : g_strdup("");
}

static gboolean load_startup_entry(const char *path, const char *id,
                                   gboolean user_entry,
                                   LsmStartupEntry *entry)
{
    GKeyFile *file = g_key_file_new();
    GError *error = NULL;
    if (!g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, &error)) {
        if (error) g_error_free(error);
        g_key_file_free(file);
        return FALSE;
    }

    char *type = desktop_string(file, "Type");
    if (type[0] && strcmp(type, "Application") != 0) {
        g_free(type);
        g_key_file_free(file);
        return FALSE;
    }
    g_free(type);

    char *name = desktop_string(file, "Name");
    char *command = desktop_string(file, "Exec");
    char *description = desktop_string(file, "Comment");
    const gboolean hidden = desktop_boolean(file, "Hidden", FALSE);
    const gboolean gnome_enabled =
        desktop_boolean(file, "X-GNOME-Autostart-enabled", TRUE);

    memset(entry, 0, sizeof(*entry));
    lsm_copy_string(entry->id, sizeof(entry->id), id);
    lsm_copy_string(entry->source_identity, sizeof(entry->source_identity), path);
    lsm_copy_string(entry->origin_identity, sizeof(entry->origin_identity), path);
    lsm_copy_string(entry->name, sizeof(entry->name), name[0] ? name : id);
    lsm_copy_string(entry->command, sizeof(entry->command), command);
    lsm_copy_string(entry->description, sizeof(entry->description), description);
    entry->user_entry = user_entry != FALSE;
    entry->enabled = !hidden && gnome_enabled;

    g_free(name);
    g_free(command);
    g_free(description);
    g_key_file_free(file);
    return TRUE;
}

static ssize_t find_entry(LsmStartupEntry *entries, size_t count,
                          const char *id)
{
    for (size_t index = 0U; index < count; index++)
        if (strcmp(entries[index].id, id) == 0) return (ssize_t)index;
    return -1;
}

static gboolean append_entry(LsmStartupEntry **entries, size_t *count,
                             size_t *capacity,
                             const LsmStartupEntry *entry)
{
    if (!entries || !count || !capacity || !entry ||
        !lsm_array_reserve((void **)entries, capacity, sizeof(**entries),
                           *count + 1U, 32U))
        return FALSE;
    (*entries)[(*count)++] = *entry;
    return TRUE;
}

static void scan_directory(const char *directory, gboolean user_entry,
                           LsmStartupEntry **entries, size_t *count,
                           size_t *capacity)
{
    DIR *dir = opendir(directory);
    if (!dir) return;

    struct dirent *item = NULL;
    while ((item = readdir(dir))) {
        if (strlen(item->d_name) <= sizeof(".desktop") - 1U ||
            !lsm_string_ends_with(item->d_name, ".desktop"))
            continue;
        if (find_entry(*entries, *count, item->d_name) >= 0) continue;

        char path[LSM_PATH_LEN];
        if (!lsm_join_path(path, sizeof(path), directory, item->d_name))
            continue;
        LsmStartupEntry entry;
        if (load_startup_entry(path, item->d_name, user_entry, &entry))
            (void)append_entry(entries, count, capacity, &entry);
    }
    closedir(dir);
}

bool lsm_startup_backend_collect(LsmStartupEntry **out_entries,
                                 size_t *out_count)
{
    if (!out_entries || !out_count) return false;
    *out_entries = NULL;
    *out_count = 0U;

    LsmStartupEntry *entries = NULL;
    size_t count = 0U;
    size_t capacity = 0U;
    char user_directory[LSM_PATH_LEN];
    if (lsm_join_path(user_directory, sizeof(user_directory),
                      g_get_user_config_dir(), "autostart"))
        scan_directory(user_directory, TRUE, &entries, &count, &capacity);

    const char *xdg_dirs = getenv("XDG_CONFIG_DIRS");
    if (!xdg_dirs || !xdg_dirs[0]) xdg_dirs = "/etc/xdg";
    char *copy = strdup(xdg_dirs);
    if (copy) {
        char *save = NULL;
        for (char *dir = strtok_r(copy, ":", &save); dir;
             dir = strtok_r(NULL, ":", &save)) {
            char path[LSM_PATH_LEN];
            if (lsm_join_path(path, sizeof(path), dir, "autostart"))
                scan_directory(path, FALSE, &entries, &count, &capacity);
        }
        free(copy);
    }

    *out_entries = entries;
    *out_count = count;
    return true;
}

static gboolean write_startup_override(const char *source_path,
                                       const char *desktop_id,
                                       gboolean enable,
                                       GError **error)
{
    GKeyFile *file = g_key_file_new();
    if (!g_key_file_load_from_file(file, source_path, G_KEY_FILE_KEEP_COMMENTS,
                                   error)) {
        g_key_file_free(file);
        return FALSE;
    }
    g_key_file_set_boolean(file, "Desktop Entry", "Hidden", !enable);
    g_key_file_set_boolean(file, "Desktop Entry", "X-GNOME-Autostart-enabled",
                           enable);

    gsize length = 0U;
    char *data = g_key_file_to_data(file, &length, error);
    g_key_file_free(file);
    if (!data) return FALSE;

    char directory[LSM_PATH_LEN];
    if (!lsm_join_path(directory, sizeof(directory),
                       g_get_user_config_dir(), "autostart")) {
        g_set_error(error, G_FILE_ERROR,
                    g_file_error_from_errno(ENAMETOOLONG),
                    "Startup configuration path is too long");
        g_free(data);
        return FALSE;
    }
    if (g_mkdir_with_parents(directory, 0755) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Unable to create %s: %s", directory, g_strerror(errno));
        g_free(data);
        return FALSE;
    }

    char *target = g_build_filename(directory, desktop_id, NULL);
    const int failure = lsm_atomic_file_write_bytes(
        target, LSM_ATOMIC_FILE_USER_DOCUMENT, data, length);
    const gboolean result = failure == 0;
    if (!result)
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(failure),
                    "Unable to write %s: %s", target, g_strerror(failure));
    g_free(target);
    g_free(data);
    return result;
}

bool lsm_startup_backend_set_enabled(const char *id,
                                     const char *source_identity,
                                     const char *origin_identity,
                                     bool enabled,
                                     GError **error)
{
    if (!id || !id[0] || !source_identity || !source_identity[0])
        return false;
    const char *source =
        origin_identity && origin_identity[0] ? origin_identity : source_identity;
    return write_startup_override(source, id, enabled ? TRUE : FALSE, error);
}

char *lsm_startup_backend_location_uri(const char *source_identity)
{
    if (!source_identity || !source_identity[0]) return NULL;
    char *directory = g_path_get_dirname(source_identity);
    char *uri = g_filename_to_uri(directory, NULL, NULL);
    g_free(directory);
    return uri;
}

void lsm_startup_backend_free(LsmStartupEntry *entries)
{
    free(entries);
}
