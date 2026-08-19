// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file application_catalog_smoke.c
 * @brief Synthetic XDG desktop-application identity regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "application_catalog.h"
#include "app_internal.h"

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool make_directories(const char *path)
{
    char copy[LSM_PATH_LEN];
    if (!path || strlen(path) >= sizeof(copy)) return false;
    strcpy(copy, path);
    for (char *cursor = copy + 1; *cursor; cursor++) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(copy, 0700) != 0 && errno != EEXIST) return false;
        *cursor = '/';
    }
    return mkdir(copy, 0700) == 0 || errno == EEXIST;
}

static bool write_text(const char *path, const char *text)
{
    char parent[LSM_PATH_LEN];
    if (!path || strlen(path) >= sizeof(parent)) return false;
    strcpy(parent, path);
    char *separator = strrchr(parent, '/');
    if (!separator) return false;
    *separator = '\0';
    if (!make_directories(parent)) return false;
    FILE *file = fopen(path, "w");
    if (!file) return false;
    const bool okay = fputs(text, file) >= 0 && fclose(file) == 0;
    return okay;
}

static bool join_path(char *destination, size_t size, const char *directory,
                      const char *suffix)
{
    const size_t directory_length = strlen(directory);
    const size_t suffix_length = strlen(suffix);
    if (directory_length + suffix_length + 1U > size) return false;
    memcpy(destination, directory, directory_length);
    memcpy(destination + directory_length, suffix, suffix_length + 1U);
    return true;
}

static void remove_tree(const char *path)
{
    DIR *directory = opendir(path);
    if (!directory) {
        (void)unlink(path);
        return;
    }
    struct dirent *entry = NULL;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;
        char child[LSM_PATH_LEN];
        const int written = snprintf(
            child, sizeof(child), "%s/%s", path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(child)) continue;
        struct stat information;
        if (lstat(child, &information) != 0) continue;
        if (S_ISDIR(information.st_mode)) remove_tree(child);
        else (void)unlink(child);
    }
    closedir(directory);
    (void)rmdir(path);
}

int main(void)
{
    char fixture[] = "/tmp/lsm-app-catalog-XXXXXX";
    const char *root = mkdtemp(fixture);
    if (!root) return 1;
    char user[LSM_PATH_LEN];
    char system[LSM_PATH_LEN];
    char path[LSM_PATH_LEN];
    snprintf(user, sizeof(user), "%s/user", root);
    snprintf(system, sizeof(system), "%s/system", root);
    if (setenv("XDG_DATA_HOME", user, 1) != 0 ||
        setenv("XDG_DATA_DIRS", system, 1) != 0)
        return 2;

    if (!join_path(path, sizeof(path), user,
                   "/applications/browser.desktop") ||
        !write_text(path,
            "[Desktop Entry]\nType=Application\nName=Friendly Browser\n"
            "Icon=browser-icon\nExec=/usr/bin/firefox %u\n"
            "StartupWMClass=BrowserWindow\n"))
        return 3;
    if (!join_path(path, sizeof(path), system,
                   "/applications/browser.desktop") ||
        !write_text(path,
            "[Desktop Entry]\nType=Application\nName=System Browser\n"
            "Exec=/usr/bin/firefox\n"))
        return 4;
    if (!join_path(path, sizeof(path), system,
                   "/applications/tool.desktop") ||
        !write_text(path,
            "[Desktop Entry]\nType=Application\nName=Test Tool\n"
            "Icon=utilities-terminal\n"
            "Exec=env TOOL_MODE=1 /opt/tools/test-tool --open %f\n"))
        return 5;
    if (!join_path(path, sizeof(path), system,
                   "/applications/hidden.desktop") ||
        !write_text(path,
            "[Desktop Entry]\nType=Application\nName=Hidden Tool\n"
            "Hidden=true\nExec=hidden-tool\n"))
        return 6;
    if (!join_path(path, sizeof(path), system,
                   "/applications/org.example.Writer.desktop") ||
        !write_text(path,
            "[Desktop Entry]\nType=Application\nName=Flatpak Writer\n"
            "Icon=org.example.Writer\n"
            "Exec=flatpak run org.example.Writer %U\n"))
        return 7;

    LsmApplicationCatalog *catalog = lsm_application_catalog_create();
    if (!catalog) return 8;
    const LsmApplicationEntry *browser =
        lsm_application_catalog_lookup(catalog, "firefox", "");
    const LsmApplicationEntry *window =
        lsm_application_catalog_lookup(catalog, "BrowserWindow", "");
    const LsmApplicationEntry *tool =
        lsm_application_catalog_lookup(
            catalog, "unknown", "/opt/tools/test-tool --open file");
    const LsmApplicationEntry *hidden =
        lsm_application_catalog_lookup(catalog, "hidden-tool", "");
    const LsmApplicationEntry *flatpak =
        lsm_application_catalog_lookup(catalog, "writer", "");
    const bool okay =
        browser && strcmp(browser->name, "Friendly Browser") == 0 &&
        strcmp(browser->icon, "browser-icon") == 0 &&
        window == browser &&
        tool && strcmp(tool->name, "Test Tool") == 0 &&
        hidden == NULL &&
        flatpak && strcmp(flatpak->name, "Flatpak Writer") == 0;

    lsm_application_catalog_destroy(catalog);
    remove_tree(root);
    puts("XDG application catalogue identity and override policy passed.");
    return okay ? 0 : 9;
}
