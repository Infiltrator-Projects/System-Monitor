// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file application_catalog.c
 * @brief Native XDG desktop-file discovery for friendly process names/icons.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "application_catalog.h"

#include <gtk/gtk.h>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct LsmApplicationCatalog {
    GHashTable *by_executable;
    GPtrArray *entries;
};

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

static void normalise_identity(const char *input, char *output, size_t size)
{
    if (!output || size == 0U) return;
    output[0] = '\0';
    if (!input || !*input) return;
    const char *basename = strrchr(input, '/');
    basename = basename ? basename + 1 : input;
    size_t written = 0U;
    while (*basename && written + 1U < size) {
        const unsigned char value = (unsigned char)*basename++;
        output[written++] = (char)tolower(value);
    }
    output[written] = '\0';
}

static gboolean read_exec_token(const char **cursor, char *token, size_t size)
{
    if (!cursor || !*cursor || !token || size == 0U) return FALSE;
    const char *text = *cursor;
    while (*text && isspace((unsigned char)*text)) text++;
    if (!*text) {
        *cursor = text;
        token[0] = '\0';
        return FALSE;
    }

    const char quote = (*text == '\'' || *text == '"') ? *text++ : '\0';
    size_t length = 0U;
    while (*text) {
        if (quote ? *text == quote : isspace((unsigned char)*text)) {
            if (quote) text++;
            break;
        }
        if (*text == '\\' && text[1] != '\0') text++;
        if (length + 1U < size) token[length++] = *text;
        text++;
    }
    token[length] = '\0';
    *cursor = text;
    return length > 0U;
}

static gboolean assignment_token(const char *token)
{
    const char *equals = token ? strchr(token, '=') : NULL;
    if (!equals || equals == token) return FALSE;
    for (const char *cursor = token; cursor < equals; cursor++) {
        const unsigned char value = (unsigned char)*cursor;
        if (!isalnum(value) && value != '_') return FALSE;
    }
    return TRUE;
}

static void executable_from_command(const char *command, char *output,
                                    size_t size)
{
    output[0] = '\0';
    if (!command || !*command) return;
    const char *cursor = command;
    char token[LSM_PATH_LEN];
    gboolean environment_wrapper = FALSE;
    gboolean flatpak_wrapper = FALSE;
    while (read_exec_token(&cursor, token, sizeof(token))) {
        if (token[0] == '%') continue;
        char identity[LSM_NAME_LEN];
        normalise_identity(token, identity, sizeof(identity));
        if (!*identity) continue;
        if (!environment_wrapper &&
            (strcmp(identity, "env") == 0 ||
             strcmp(identity, "flatpak-spawn") == 0)) {
            environment_wrapper = TRUE;
            continue;
        }
        if (!environment_wrapper && strcmp(identity, "flatpak") == 0) {
            environment_wrapper = TRUE;
            flatpak_wrapper = TRUE;
            continue;
        }
        if (environment_wrapper &&
            (assignment_token(token) || token[0] == '-'))
            continue;
        if (flatpak_wrapper && strcmp(identity, "run") == 0) continue;
        g_strlcpy(output, identity, size);
        return;
    }
}

static void desktop_id_from_filename(const char *filename, char *id,
                                     size_t size)
{
    g_strlcpy(id, filename ? filename : "", size);
    char *suffix = strstr(id, ".desktop");
    if (suffix && suffix[8] == '\0') *suffix = '\0';
}

static void register_identity(LsmApplicationCatalog *catalog,
                              const char *identity,
                              LsmApplicationEntry *entry)
{
    char normalised[LSM_NAME_LEN];
    normalise_identity(identity, normalised, sizeof(normalised));
    if (!*normalised ||
        g_hash_table_lookup(catalog->by_executable, normalised))
        return;
    g_hash_table_insert(catalog->by_executable, g_strdup(normalised), entry);
}

static void register_desktop_id_aliases(LsmApplicationCatalog *catalog,
                                        const char *desktop_id,
                                        LsmApplicationEntry *entry)
{
    register_identity(catalog, desktop_id, entry);
    const char *dot = strrchr(desktop_id, '.');
    if (dot && dot[1]) register_identity(catalog, dot + 1, entry);
    const char *underscore = strrchr(desktop_id, '_');
    if (underscore && underscore[1])
        register_identity(catalog, underscore + 1, entry);
}

static void load_desktop_file(LsmApplicationCatalog *catalog,
                              const char *path, const char *filename)
{
    GKeyFile *file = g_key_file_new();
    GError *error = NULL;
    if (!g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, &error)) {
        if (error) g_error_free(error);
        g_key_file_free(file);
        return;
    }

    char *type = desktop_string(file, "Type");
    const gboolean application = !*type || strcmp(type, "Application") == 0;
    g_free(type);
    if (!application || desktop_boolean(file, "Hidden", FALSE)) {
        g_key_file_free(file);
        return;
    }

    char *name = desktop_string(file, "Name");
    char *icon = desktop_string(file, "Icon");
    char *command = desktop_string(file, "Exec");
    char *startup_class = desktop_string(file, "StartupWMClass");
    char executable[LSM_NAME_LEN];
    executable_from_command(command, executable, sizeof(executable));
    if (!*executable) {
        g_free(name);
        g_free(icon);
        g_free(command);
        g_free(startup_class);
        g_key_file_free(file);
        return;
    }

    LsmApplicationEntry *entry = calloc(1U, sizeof(*entry));
    if (entry) {
        desktop_id_from_filename(filename, entry->id, sizeof(entry->id));
        g_strlcpy(entry->executable, executable, sizeof(entry->executable));
        g_strlcpy(entry->name, *name ? name : entry->id,
                  sizeof(entry->name));
        g_strlcpy(entry->icon, *icon ? icon : "application-x-executable",
                  sizeof(entry->icon));
        g_ptr_array_add(catalog->entries, entry);
        register_identity(catalog, entry->executable, entry);
        register_desktop_id_aliases(catalog, entry->id, entry);
        register_identity(catalog, startup_class, entry);
    }

    g_free(name);
    g_free(icon);
    g_free(command);
    g_free(startup_class);
    g_key_file_free(file);
}

static void scan_application_directory(LsmApplicationCatalog *catalog,
                                       const char *directory)
{
    DIR *stream = opendir(directory);
    if (!stream) return;
    struct dirent *item = NULL;
    while ((item = readdir(stream))) {
        const size_t length = strlen(item->d_name);
        if (length <= 8U ||
            strcmp(item->d_name + length - 8U, ".desktop") != 0)
            continue;
        char path[LSM_PATH_LEN];
        const int written = snprintf(
            path, sizeof(path), "%s/%s", directory, item->d_name);
        if (written < 0 || (size_t)written >= sizeof(path)) continue;
        load_desktop_file(catalog, path, item->d_name);
    }
    closedir(stream);
}

LsmApplicationCatalog *lsm_application_catalog_create(void)
{
    LsmApplicationCatalog *catalog = calloc(1U, sizeof(*catalog));
    if (!catalog) return NULL;
    catalog->by_executable = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, NULL);
    catalog->entries = g_ptr_array_new_with_free_func(free);
    if (!catalog->by_executable || !catalog->entries) {
        lsm_application_catalog_destroy(catalog);
        return NULL;
    }

    char user_directory[LSM_PATH_LEN];
    const char *data_home = getenv("XDG_DATA_HOME");
    if (data_home && *data_home) {
        snprintf(user_directory, sizeof(user_directory),
                 "%s/applications", data_home);
    } else {
        snprintf(user_directory, sizeof(user_directory),
                 "%s/.local/share/applications", g_get_home_dir());
    }
    scan_application_directory(catalog, user_directory);

    const char *data_dirs = getenv("XDG_DATA_DIRS");
    if (!data_dirs || !*data_dirs)
        data_dirs = "/usr/local/share:/usr/share";
    char *copy = strdup(data_dirs);
    if (copy) {
        char *save = NULL;
        for (char *directory = strtok_r(copy, ":", &save);
             directory;
             directory = strtok_r(NULL, ":", &save)) {
            char path[LSM_PATH_LEN];
            const int written = snprintf(
                path, sizeof(path), "%s/applications", directory);
            if (written >= 0 && (size_t)written < sizeof(path))
                scan_application_directory(catalog, path);
        }
        free(copy);
    }
    return catalog;
}

void lsm_application_catalog_destroy(LsmApplicationCatalog *catalog)
{
    if (!catalog) return;
    if (catalog->by_executable)
        g_hash_table_destroy(catalog->by_executable);
    if (catalog->entries)
        g_ptr_array_free(catalog->entries, TRUE);
    free(catalog);
}

const LsmApplicationEntry *lsm_application_catalog_lookup(
    const LsmApplicationCatalog *catalog, const char *process_name,
    const char *command)
{
    if (!catalog || !catalog->by_executable) return NULL;
    char identity[LSM_NAME_LEN];
    normalise_identity(process_name, identity, sizeof(identity));
    const LsmApplicationEntry *entry =
        g_hash_table_lookup(catalog->by_executable, identity);
    if (entry) return entry;

    executable_from_command(command, identity, sizeof(identity));
    entry = g_hash_table_lookup(catalog->by_executable, identity);
    if (entry) return entry;

    const size_t length = strlen(identity);
    if (length > 4U && strcmp(identity + length - 4U, "-bin") == 0) {
        identity[length - 4U] = '\0';
        return g_hash_table_lookup(catalog->by_executable, identity);
    }
    return NULL;
}
