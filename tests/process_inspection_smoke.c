// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_inspection_smoke.c
 * @brief Deterministic fixture test for detailed process-inspection parsers.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "process_inspection.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static bool join(char *destination, size_t size, const char *left,
                 const char *right)
{
    const int written = snprintf(destination, size, "%s/%s", left, right);
    return written >= 0 && (size_t)written < size;
}

static bool make_tree(const char *path)
{
    char copy[PATH_MAX];
    if (!path || strlen(path) >= sizeof(copy)) return false;
    memcpy(copy, path, strlen(path) + 1U);
    for (char *cursor = copy + 1; *cursor; cursor++) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(copy, 0755) != 0 && errno != EEXIST) return false;
        *cursor = '/';
    }
    return mkdir(copy, 0755) == 0 || errno == EEXIST;
}

static bool write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    if (!file) return false;
    const bool okay = fputs(text, file) != EOF && fclose(file) == 0;
    return okay;
}

static void remove_tree(const char *path)
{
    struct stat status;
    if (lstat(path, &status) != 0) return;
    if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode)) {
        (void)unlink(path);
        return;
    }
    DIR *directory = opendir(path);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char child[PATH_MAX];
        if (join(child, sizeof(child), path, entry->d_name)) remove_tree(child);
    }
    closedir(directory);
    (void)rmdir(path);
}

int main(void)
{
    char template[] = "/tmp/lsm-process-inspection-XXXXXX";
    char *root = mkdtemp(template);
    if (!root) return 1;
    char path[PATH_MAX], target[PATH_MAX];
    if (!join(target, sizeof(target), root, "document.txt") ||
        !write_text(target, "fixture\n")) {
        remove_tree(root);
        return 2;
    }

    static const char *directories[] = {
        "123/fd", "123/task/123", "123/task/124", "456/fd", "456/task/456"
    };
    for (size_t index = 0U; index < sizeof(directories) / sizeof(directories[0]); index++) {
        if (!join(path, sizeof(path), root, directories[index]) || !make_tree(path)) {
            remove_tree(root);
            return 3;
        }
    }
    if (!join(path, sizeof(path), root, "123/fd/3") || symlink(target, path) != 0 ||
        !join(path, sizeof(path), root, "123/fd/4") || symlink("socket:[99]", path) != 0 ||
        !join(path, sizeof(path), root, "456/fd/7") || symlink(target, path) != 0) {
        remove_tree(root);
        return 4;
    }
    if (!join(path, sizeof(path), root, "123/maps") ||
        !write_text(path,
            "00400000-00452000 r-xp 00000000 08:01 123 /usr/bin/demo\n"
            "7f000000-7f001000 rw-p 00000000 00:00 0 [heap]\n") ||
        !join(path, sizeof(path), root, "123/stat") ||
        !write_text(path, "123 (demo process) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 424242 20\n") ||
        !join(path, sizeof(path), root, "123/comm") || !write_text(path, "demo\n") ||
        !join(path, sizeof(path), root, "456/comm") || !write_text(path, "viewer\n") ||
        !join(path, sizeof(path), root, "123/task/123/comm") || !write_text(path, "demo\n") ||
        !join(path, sizeof(path), root, "123/task/123/status") || !write_text(path, "State:\tR (running)\n") ||
        !join(path, sizeof(path), root, "123/task/124/comm") || !write_text(path, "worker\n") ||
        !join(path, sizeof(path), root, "123/task/124/status") || !write_text(path, "State:\tS (sleeping)\n")) {
        remove_tree(root);
        return 5;
    }
    if (setenv("LSM_PROCFS_ROOT", root, 1) != 0) {
        remove_tree(root);
        return 6;
    }

    if (!lsm_process_inspection_identity_matches(123, 424242U) ||
        lsm_process_inspection_identity_matches(123, 424243U)) {
        remove_tree(root);
        return 7;
    }

    LsmOpenFileInfo *files = NULL;
    const size_t file_count = lsm_process_inspection_open_files(123, &files);
    if (file_count != 2U || files[0].descriptor != 3 ||
        strcmp(files[0].kind, "File") != 0 ||
        strcmp(files[1].kind, "Socket") != 0) {
        lsm_process_inspection_free(files);
        remove_tree(root);
        return 7;
    }
    lsm_process_inspection_free(files);

    LsmMemoryMapInfo *maps = NULL;
    const size_t map_count = lsm_process_inspection_memory_maps(123, &maps);
    if (map_count != 2U || maps[0].start_address != 0x00400000ULL ||
        strcmp(maps[0].permissions, "r-xp") != 0 ||
        strcmp(maps[1].path, "[heap]") != 0) {
        lsm_process_inspection_free(maps);
        remove_tree(root);
        return 8;
    }
    lsm_process_inspection_free(maps);

    LsmThreadInfo *threads = NULL;
    const size_t thread_count = lsm_process_inspection_threads(123, &threads);
    if (thread_count != 2U || threads[0].tid != 123 || threads[1].tid != 124 ||
        strcmp(threads[1].name, "worker") != 0 ||
        strstr(threads[1].state, "sleeping") == NULL) {
        lsm_process_inspection_free(threads);
        remove_tree(root);
        return 9;
    }
    lsm_process_inspection_free(threads);

    LsmFileUserInfo *users = NULL;
    const size_t user_count = lsm_process_inspection_find_file_users(target, &users);
    if (user_count != 2U || users[0].pid != 123 || users[1].pid != 456) {
        lsm_process_inspection_free(users);
        remove_tree(root);
        return 10;
    }
    lsm_process_inspection_free(users);
    remove_tree(root);
    puts("Process inspection parsers and file-owner search passed.");
    return 0;
}
