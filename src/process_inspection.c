// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_inspection.c
 * @brief Direct procfs implementation for the graphical Process Inspector.
 *
 * The parser deliberately tolerates process churn: a descriptor or task that
 * disappears between readdir(3) and readlink(2)/open(2) is skipped, while a
 * failure to open the containing procfs object aborts that category. No state
 * is retained between calls, so PID-reuse protection remains the responsibility
 * of the inspector window, which compares the process start-time identity.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "process_inspection.h"

#include "common.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static bool native_pid(LsmProcessId id, pid_t *pid)
{
    if (!pid || id == 0U || id > (LsmProcessId)INT_MAX) {
        errno = EINVAL;
        return false;
    }
    *pid = (pid_t)id;
    return true;
}

static const char *procfs_root(void)
{
    const char *root = getenv("LSM_PROCFS_ROOT");
    return root && *root ? root : "/proc";
}

static bool process_path(char *buffer, size_t size, pid_t pid,
                         const char *suffix)
{
    if (!buffer || size == 0U || pid < 1 || !suffix) return false;
    const int written = snprintf(buffer, size, "%s/%d/%s", procfs_root(), pid,
                                 suffix);
    return written >= 0 && (size_t)written < size;
}

static bool parse_start_ticks(const char *stat_text, uint64_t *start_ticks)
{
    if (!stat_text || !start_ticks) return false;
    const char *closing = strrchr(stat_text, ')');
    if (!closing || closing[1] != ' ') return false;

    const char *cursor = closing + 2;
    for (unsigned field = 3U; field <= 22U; field++) {
        while (*cursor && isspace((unsigned char)*cursor)) cursor++;
        if (!*cursor) return false;
        const char *begin = cursor;
        while (*cursor && !isspace((unsigned char)*cursor)) cursor++;
        if (field != 22U) continue;
        uint64_t value = 0U;
        const char *number = begin;
        if (!lsm_parse_u64_token(&number, 10U, &value) ||
            number != cursor || value == 0U)
            return false;
        *start_ticks = value;
        return true;
    }
    return false;
}

bool lsm_process_inspection_identity_matches(
    LsmProcessId process_id, LsmProcessInstanceId expected_instance_id)
{
    pid_t pid = 0;
    if (!native_pid(process_id, &pid) || expected_instance_id == 0U)
        return false;
    char path[PATH_MAX];
    if (!process_path(path, sizeof(path), pid, "stat")) return false;
    char text[4096];
    if (!lsm_read_text_file(path, text, sizeof(text))) return false;
    uint64_t current = 0U;
    return parse_start_ticks(text, &current) && current == expected_instance_id;
}

static bool numeric_name(const char *text)
{
    if (!text || !*text) return false;
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor; cursor++)
        if (!isdigit(*cursor)) return false;
    return true;
}

static const char *descriptor_kind(const char *target)
{
    if (!target || !*target) return "Unknown";
    if (strncmp(target, "socket:[", 8U) == 0) return "Socket";
    if (strncmp(target, "pipe:[", 6U) == 0) return "Pipe";
    if (strncmp(target, "anon_inode:", 11U) == 0) return "Anon inode";
    if (target[0] == '/') return "File";
    return "Kernel object";
}

static int compare_open_file(const void *left, const void *right)
{
    const LsmOpenFileInfo *a = left;
    const LsmOpenFileInfo *b = right;
    return (a->descriptor > b->descriptor) - (a->descriptor < b->descriptor);
}

size_t lsm_process_inspection_open_files(LsmProcessId process_id, LsmOpenFileInfo **out_items)
{
    pid_t pid = 0;
    if (!out_items || !native_pid(process_id, &pid)) {
        errno = EINVAL;
        return 0U;
    }
    *out_items = NULL;
    char directory_path[PATH_MAX];
    if (!process_path(directory_path, sizeof(directory_path), pid, "fd")) {
        errno = ENAMETOOLONG;
        return 0U;
    }
    DIR *directory = opendir(directory_path);
    if (!directory) return 0U;

    LsmOpenFileInfo *items = NULL;
    size_t count = 0U, capacity = 0U;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (!numeric_name(entry->d_name)) continue;
        char link_path[PATH_MAX];
        const int written = snprintf(link_path, sizeof(link_path), "%s/%s",
                                     directory_path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(link_path)) continue;
        char target[LSM_INSPECTION_TARGET_LEN];
        const ssize_t length = readlink(link_path, target, sizeof(target) - 1U);
        if (length < 0) continue;
        target[length] = '\0';
        if (!lsm_array_reserve((void **)&items, &capacity, sizeof(*items),
                               count + 1U, 32U))
            break;
        LsmOpenFileInfo *item = &items[count++];
        memset(item, 0, sizeof(*item));
        item->descriptor = atoi(entry->d_name);
        lsm_copy_string(item->kind, sizeof(item->kind), descriptor_kind(target));
        lsm_copy_string(item->target, sizeof(item->target), target);
    }
    closedir(directory);
    if (count > 1U) qsort(items, count, sizeof(*items), compare_open_file);
    *out_items = items;
    return count;
}

size_t lsm_process_inspection_memory_maps(LsmProcessId process_id, LsmMemoryMapInfo **out_items)
{
    pid_t pid = 0;
    if (!out_items || !native_pid(process_id, &pid)) {
        errno = EINVAL;
        return 0U;
    }
    *out_items = NULL;
    char path[PATH_MAX];
    if (!process_path(path, sizeof(path), pid, "maps")) {
        errno = ENAMETOOLONG;
        return 0U;
    }
    FILE *file = fopen(path, "r");
    if (!file) return 0U;

    LsmMemoryMapInfo *items = NULL;
    size_t count = 0U, capacity = 0U;
    char line[4096];
    while (fgets(line, sizeof(line), file)) {
        unsigned long long start = 0U, end = 0U, offset = 0U, inode = 0U;
        char permissions[8] = "", device[32] = "", pathname[LSM_INSPECTION_MAP_PATH_LEN] = "";
        int consumed = 0;
        const int fields = sscanf(line, "%llx-%llx %7s %llx %31s %llu %n",
                                  &start, &end, permissions, &offset, device,
                                  &inode, &consumed);
        if (fields < 6) continue;
        const char *tail = line + consumed;
        while (*tail == ' ' || *tail == '\t') tail++;
        size_t tail_length = strcspn(tail, "\r\n");
        if (tail_length >= sizeof(pathname)) tail_length = sizeof(pathname) - 1U;
        memcpy(pathname, tail, tail_length);
        pathname[tail_length] = '\0';
        if (!lsm_array_reserve((void **)&items, &capacity, sizeof(*items),
                               count + 1U, 32U))
            break;
        LsmMemoryMapInfo *item = &items[count++];
        memset(item, 0, sizeof(*item));
        item->start_address = (uint64_t)start;
        item->end_address = (uint64_t)end;
        item->file_offset = (uint64_t)offset;
        item->inode = (uint64_t)inode;
        lsm_copy_string(item->permissions, sizeof(item->permissions), permissions);
        lsm_copy_string(item->device, sizeof(item->device), device);
        lsm_copy_string(item->path, sizeof(item->path), pathname);
    }
    fclose(file);
    *out_items = items;
    return count;
}

static void read_thread_state(const char *path, char *state, size_t state_size)
{
    FILE *file = fopen(path, "r");
    if (!file) return;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "State:", 6U) != 0) continue;
        char *value = line + 6U;
        while (*value == ' ' || *value == '\t') value++;
        lsm_trim_line_end(value);
        lsm_copy_string(state, state_size, value);
        break;
    }
    fclose(file);
}

static int compare_thread(const void *left, const void *right)
{
    const LsmThreadInfo *a = left;
    const LsmThreadInfo *b = right;
    return (a->tid > b->tid) - (a->tid < b->tid);
}

size_t lsm_process_inspection_threads(LsmProcessId process_id, LsmThreadInfo **out_items)
{
    pid_t pid = 0;
    if (!out_items || !native_pid(process_id, &pid)) {
        errno = EINVAL;
        return 0U;
    }
    *out_items = NULL;
    char task_path[PATH_MAX];
    if (!process_path(task_path, sizeof(task_path), pid, "task")) {
        errno = ENAMETOOLONG;
        return 0U;
    }
    DIR *directory = opendir(task_path);
    if (!directory) return 0U;

    LsmThreadInfo *items = NULL;
    size_t count = 0U, capacity = 0U;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (!numeric_name(entry->d_name)) continue;
        uint64_t tid = 0U;
        if (!lsm_parse_u64(entry->d_name, 10U, &tid)) continue;
        if (!lsm_array_reserve((void **)&items, &capacity, sizeof(*items),
                               count + 1U, 32U))
            break;
        LsmThreadInfo *item = &items[count++];
        memset(item, 0, sizeof(*item));
        item->tid = (LsmProcessId)tid;
        char path[PATH_MAX], text[LSM_NAME_LEN];
        int written = snprintf(path, sizeof(path), "%s/%s/comm", task_path,
                               entry->d_name);
        if (written >= 0 && (size_t)written < sizeof(path) &&
            lsm_read_text_file(path, text, sizeof(text)))
            lsm_copy_string(item->name, sizeof(item->name), text);
        else
            snprintf(item->name, sizeof(item->name), "Thread %llu",
                     (unsigned long long)item->tid);
        written = snprintf(path, sizeof(path), "%s/%s/status", task_path,
                           entry->d_name);
        if (written >= 0 && (size_t)written < sizeof(path))
            read_thread_state(path, item->state, sizeof(item->state));
        if (!item->state[0]) lsm_copy_string(item->state, sizeof(item->state), "Unknown");
    }
    closedir(directory);
    if (count > 1U) qsort(items, count, sizeof(*items), compare_thread);
    *out_items = items;
    return count;
}

static void read_process_name(pid_t pid, char *name, size_t size)
{
    char path[PATH_MAX];
    if (!process_path(path, sizeof(path), pid, "comm") ||
        !lsm_read_text_file(path, name, size))
        snprintf(name, size, "PID %d", pid);
}

static bool canonical_descriptor_target(const char *target, char *resolved,
                                        size_t resolved_size)
{
    if (!target || target[0] != '/' || !resolved || resolved_size == 0U)
        return false;
    char candidate[LSM_INSPECTION_TARGET_LEN];
    lsm_copy_string(candidate, sizeof(candidate), target);
    char *deleted = strstr(candidate, " (deleted)");
    if (deleted && deleted[10] == '\0') *deleted = '\0';
    char real[PATH_MAX];
    if (!lsm_realpath_copy(candidate, real, sizeof(real))) return false;
    lsm_copy_string(resolved, resolved_size, real);
    return true;
}

static int compare_file_user(const void *left, const void *right)
{
    const LsmFileUserInfo *a = left;
    const LsmFileUserInfo *b = right;
    if (a->pid != b->pid) return (a->pid > b->pid) - (a->pid < b->pid);
    return (a->descriptor > b->descriptor) -
           (a->descriptor < b->descriptor);
}

size_t lsm_process_inspection_find_file_users(const char *path,
                                              LsmFileUserInfo **out_items)
{
    if (!path || !*path || !out_items) {
        errno = EINVAL;
        return 0U;
    }
    *out_items = NULL;
    char requested[PATH_MAX];
    if (!lsm_realpath_copy(path, requested, sizeof(requested))) return 0U;

    DIR *proc = opendir(procfs_root());
    if (!proc) return 0U;
    LsmFileUserInfo *items = NULL;
    size_t count = 0U, capacity = 0U;
    struct dirent *entry;
    while ((entry = readdir(proc))) {
        if (!numeric_name(entry->d_name)) continue;
        const pid_t pid = (pid_t)atoi(entry->d_name);
        LsmOpenFileInfo *files = NULL;
        const size_t file_count = lsm_process_inspection_open_files(
            (LsmProcessId)pid, &files);
        for (size_t index = 0U; index < file_count; index++) {
            char resolved[PATH_MAX];
            if (!canonical_descriptor_target(files[index].target, resolved,
                                             sizeof(resolved)) ||
                strcmp(resolved, requested) != 0)
                continue;
            if (!lsm_array_reserve((void **)&items, &capacity, sizeof(*items),
                                   count + 1U, 32U))
                break;
            LsmFileUserInfo *item = &items[count++];
            memset(item, 0, sizeof(*item));
            item->pid = (LsmProcessId)pid;
            item->descriptor = files[index].descriptor;
            read_process_name(pid, item->process_name, sizeof(item->process_name));
            lsm_copy_string(item->target, sizeof(item->target), files[index].target);
        }
        free(files);
    }
    closedir(proc);
    if (count > 1U) qsort(items, count, sizeof(*items), compare_file_user);
    *out_items = items;
    return count;
}

void lsm_process_inspection_free(void *items)
{
    free(items);
}
