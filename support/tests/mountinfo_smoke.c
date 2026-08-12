// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mountinfo_smoke.c
 * @brief Native mountinfo parser regression tests.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "mountinfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool write_fixture(const char *path)
{
    FILE *stream = fopen(path, "w");
    if (!stream) return false;
    const int written = fputs(
        "36 25 259:4 / /media/My\\040Drive rw,nosuid shared:7 - ext4 "
        "/dev/disk/by-label/My\\040Disk rw,errors=remount-ro\n"
        "41 36 259:4 /Documents /home/user/Documents rw - ext4 /dev/nvme0n1p4 rw\n"
        "52 25 0:42 / /run/user/1000/gvfs rw,nosuid,nodev - fuse.gvfsd-fuse "
        "gvfsd-fuse rw,user_id=1000\n"
        "this record is deliberately malformed\n",
        stream);
    const bool ok = written >= 0 && fclose(stream) == 0;
    if (!ok) (void)fclose(stream);
    return ok;
}

typedef struct {
    LsmMountInfoEntry entries[8];
    size_t count;
} MountCollector;

static bool collect_mount(const LsmMountInfoEntry *entry, void *user_data)
{
    MountCollector *collector = user_data;
    if (!collector || !entry || collector->count >= 8) return false;
    collector->entries[collector->count++] = *entry;
    return collector->count < 8;
}

int main(void)
{
    char path[] = "/tmp/lsm-mountinfo-XXXXXX";
    const int descriptor = mkstemp(path);
    if (descriptor < 0) return 1;
    close(descriptor);
    if (!write_fixture(path)) {
        unlink(path);
        return 2;
    }

    MountCollector collector = {0};
    const size_t visited = lsm_mountinfo_visit_file(path, collect_mount, &collector);
    unlink(path);
    if (visited != 3 || collector.count != 3) return 3;
    if (collector.entries[0].major_number != 259 ||
        collector.entries[0].minor_number != 4) return 4;
    if (strcmp(collector.entries[0].target, "/media/My Drive") != 0) return 5;
    if (strcmp(collector.entries[0].source, "/dev/disk/by-label/My Disk") != 0) return 6;
    if (strcmp(collector.entries[0].filesystem, "ext4") != 0) return 7;
    if (strcmp(collector.entries[1].target, "/home/user/Documents") != 0) return 8;
    if (collector.entries[2].major_number != 0 ||
        collector.entries[2].minor_number != 42) return 9;

    printf("Native mountinfo parser passed (%zu records).\n", visited);
    return 0;
}
