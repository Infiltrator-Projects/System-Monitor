// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file filesystem_inventory.c
 * @brief Toolkit-independent mountinfo and statvfs filesystem collector.
 *
 * Mount membership comes from /proc/self/mountinfo because it is namespace
 * aware and preserves the source and filesystem type required by the GUI.
 * Capacity is read independently for each mount so one stale network mount or
 * permission failure cannot invalidate the rest of the snapshot.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "filesystem_inventory.h"

#include "common.h"
#include "mountinfo.h"

#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

#define FILESYSTEM_INITIAL_CAPACITY 32U

typedef struct {
    LsmFilesystemInfo *items;
    size_t count;
    size_t capacity;
    bool allocation_failed;
} FilesystemCollector;

static bool filesystem_type_is_desktop_storage(const char *type)
{
    static const char *const types[] = {
        "ext2", "ext3", "ext4", "xfs", "btrfs", "f2fs", "jfs", "reiserfs",
        "vfat", "msdos", "exfat", "ntfs", "ntfs3", "fuseblk", "fuse",
        "zfs", "bcachefs", "udf", "iso9660", "nfs", "nfs4", "cifs", "smb3",
        "sshfs", "9p"
    };
    if (!type) return false;
    for (size_t index = 0U; index < LSM_ARRAY_LENGTH(types); index++)
        if (strcmp(type, types[index]) == 0) return true;
    return false;
}

static bool filesystem_default_visible(const LsmMountInfoEntry *entry)
{
    if (!entry) return false;
    if (strcmp(entry->target, "/") == 0) return true;
    if (strncmp(entry->source, "/dev/", 5U) == 0) return true;
    return filesystem_type_is_desktop_storage(entry->filesystem);
}

static bool collector_reserve(FilesystemCollector *collector)
{
    if (collector->count < collector->capacity) return true;
    const size_t next = collector->capacity ? collector->capacity * 2U :
        FILESYSTEM_INITIAL_CAPACITY;
    if (next < collector->capacity || next > SIZE_MAX / sizeof(*collector->items))
        return false;
    LsmFilesystemInfo *grown = realloc(collector->items,
                                       next * sizeof(*collector->items));
    if (!grown) return false;
    collector->items = grown;
    collector->capacity = next;
    return true;
}

static void collect_capacity(LsmFilesystemInfo *item)
{
    struct statvfs information;
    if (statvfs(item->target, &information) != 0) return;

    const uint64_t block_size = information.f_frsize ?
        (uint64_t)information.f_frsize : (uint64_t)information.f_bsize;
    const uint64_t total = lsm_u64_multiply_saturating(
        (uint64_t)information.f_blocks, block_size);
    const uint64_t free_all = lsm_u64_multiply_saturating(
        (uint64_t)information.f_bfree, block_size);
    const uint64_t available = lsm_u64_multiply_saturating(
        (uint64_t)information.f_bavail, block_size);

    item->capacity_available = true;
    item->total_bytes = total;
    item->available_bytes = available < total ? available : total;
    item->used_bytes = total >= free_all ? total - free_all : 0U;
    if (total != 0U) {
        item->used_percent = (unsigned)lsm_percent_u64(
            item->used_bytes, total);
    }
}

static bool append_mount(const LsmMountInfoEntry *entry, void *user_data)
{
    FilesystemCollector *collector = user_data;
    if (!collector_reserve(collector)) {
        collector->allocation_failed = true;
        return false;
    }

    LsmFilesystemInfo *item = &collector->items[collector->count++];
    memset(item, 0, sizeof(*item));
    lsm_copy_string(item->source, sizeof(item->source), entry->source);
    lsm_copy_string(item->target, sizeof(item->target), entry->target);
    lsm_copy_string(item->filesystem, sizeof(item->filesystem), entry->filesystem);
    item->normally_visible = filesystem_default_visible(entry);
    collect_capacity(item);
    return true;
}

static int compare_filesystems(const void *left_value, const void *right_value)
{
    const LsmFilesystemInfo *left = left_value;
    const LsmFilesystemInfo *right = right_value;
    const int target_order = strcmp(left->target, right->target);
    return target_order != 0 ? target_order : strcmp(left->source, right->source);
}

size_t lsm_filesystem_inventory_collect(LsmFilesystemInfo **out_items)
{
    if (!out_items) return 0U;
    *out_items = NULL;

    FilesystemCollector collector = {0};
    const char *root = getenv("LSM_PROCFS_ROOT");
    if (!root || !*root) root = "/proc";
    char path[LSM_PATH_LEN];
    if (!lsm_join_path(path, sizeof(path), root, "/self/mountinfo"))
        return 0U;
    (void)lsm_mountinfo_visit_file(path, append_mount, &collector);
    if (collector.allocation_failed) {
        free(collector.items);
        return 0U;
    }
    if (collector.count == 0U) {
        free(collector.items);
        return 0U;
    }

    qsort(collector.items, collector.count, sizeof(*collector.items),
          compare_filesystems);
    *out_items = collector.items;
    return collector.count;
}

void lsm_filesystem_inventory_free(LsmFilesystemInfo *items)
{
    free(items);
}
