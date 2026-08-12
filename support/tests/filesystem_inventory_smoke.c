// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file filesystem_inventory_smoke.c
 * @brief Regression test for mount classification and capacity snapshots.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "filesystem_inventory.h"
#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool make_directory(const char *path)
{
    return mkdir(path, 0700) == 0 || errno == EEXIST;
}

int main(void)
{
    char root[] = "/tmp/lsm-filesystems-XXXXXX";
    if (!mkdtemp(root)) return 1;

    char self[512], mount_a[512], mount_b[512], mountinfo[512];
    if (!lsm_join_path(self, sizeof(self), root, "/self") ||
        !lsm_join_path(mount_a, sizeof(mount_a), root, "/storage") ||
        !lsm_join_path(mount_b, sizeof(mount_b), root, "/kernel") ||
        !lsm_join_path(mountinfo, sizeof(mountinfo), self, "/mountinfo"))
        return 2;
    if (!make_directory(self) || !make_directory(mount_a) ||
        !make_directory(mount_b)) return 3;

    FILE *stream = fopen(mountinfo, "w");
    if (!stream) return 4;
    fprintf(stream,
        "31 20 8:1 / %s rw,relatime - ext4 /dev/sda1 rw\n"
        "32 20 0:5 / %s rw,nosuid,nodev - proc proc rw\n",
        mount_a, mount_b);
    if (fclose(stream) != 0) return 5;
    if (setenv("LSM_PROCFS_ROOT", root, 1) != 0) return 6;

    LsmFilesystemInfo *items = NULL;
    const size_t count = lsm_filesystem_inventory_collect(&items);
    if (count != 2U || !items) return 7;

    bool found_storage = false, found_kernel = false;
    for (size_t index = 0U; index < count; index++) {
        if (strcmp(items[index].filesystem, "ext4") == 0) {
            found_storage = items[index].normally_visible &&
                items[index].capacity_available &&
                items[index].used_percent <= 100U;
        } else if (strcmp(items[index].filesystem, "proc") == 0) {
            found_kernel = !items[index].normally_visible;
        }
    }
    lsm_filesystem_inventory_free(items);
    unlink(mountinfo);
    rmdir(mount_a);
    rmdir(mount_b);
    rmdir(self);
    rmdir(root);
    if (!found_storage || !found_kernel) return 8;
    puts("Filesystem inventory classification and capacity sampling passed.");
    return 0;
}
