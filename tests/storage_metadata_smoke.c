// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file storage_metadata_smoke.c
 * @brief Cached block metadata parsing and classification regression test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "storage_metadata.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool write_record(const char *root, unsigned int major_number,
                         unsigned int minor_number, const char *contents)
{
    char path[512];
    const int written = snprintf(path, sizeof(path), "%s/b%u:%u",
                                 root, major_number, minor_number);
    if (written < 0 || (size_t)written >= sizeof(path)) return false;
    FILE *file = fopen(path, "w");
    if (!file) return false;
    const bool ok = fputs(contents, file) >= 0 && fclose(file) == 0;
    return ok;
}

static bool expect_label(const char *root, unsigned int minor_number,
                         const char *expected)
{
    char label[64] = "stale";
    return lsm_storage_metadata_type_label(root, 8U, minor_number,
                                           label, sizeof(label)) &&
           strcmp(label, expected) == 0;
}

int main(void)
{
    char root_template[] = "/tmp/lsm-storage-metadata-XXXXXX";
    char *root = mkdtemp(root_template);
    if (!root) return 1;

    if (!write_record(root, 8U, 1U,
                      "E:ID_FS_TYPE=vfat\nE:ID_FS_VERSION=FAT12\n") ||
        !write_record(root, 8U, 2U,
                      "E:ID_FS_VERSION=FAT16\nE:ID_FS_TYPE=msdos\n") ||
        !write_record(root, 8U, 3U,
                      "E:IGNORED=value\nE:ID_FS_VERSION=FAT32\n"
                      "E:ID_FS_TYPE=vfat\n") ||
        !write_record(root, 8U, 4U,
                      "E:ID_FS_TYPE=ntfs\nE:ID_FS_VERSION=FAT32\n") ||
        !write_record(root, 8U, 5U,
                      "E:ID_PART_ENTRY_TYPE=e3c9e316-0b5c-4db8-817d-f92df00215ae\n") ||
        !write_record(root, 8U, 6U,
                      "E:ID_PART_ENTRY_TYPE=e3c9e316-0b5c-4db8-817d-f92df00215ae\n"
                      "E:ID_FS_TYPE=ext4\n") ||
        !write_record(root, 8U, 7U,
                      "E:ID_PART_ENTRY_TYPE=00000000-0000-0000-0000-000000000000\n")) {
        return 2;
    }

    if (!expect_label(root, 1U, "FAT12") ||
        !expect_label(root, 2U, "FAT16") ||
        !expect_label(root, 3U, "FAT32") ||
        !expect_label(root, 4U, "ntfs") ||
        !expect_label(root, 5U, "Microsoft Reserved") ||
        !expect_label(root, 6U, "ext4"))
        return 3;

    char label[64] = "stale";
    if (lsm_storage_metadata_type_label(root, 8U, 7U,
                                        label, sizeof(label)) || label[0])
        return 4;
    label[0] = 'x';
    label[1] = '\0';
    if (lsm_storage_metadata_type_label(root, 8U, 99U,
                                        label, sizeof(label)) || label[0])
        return 5;
    if (lsm_storage_metadata_type_label(root, 8U, 1U, NULL, 0U)) return 6;

    for (unsigned int minor_number = 1U; minor_number <= 7U; minor_number++) {
        char path[512];
        const int written = snprintf(path, sizeof(path), "%s/b8:%u",
                                     root, minor_number);
        if (written >= 0 && (size_t)written < sizeof(path)) (void)unlink(path);
    }
    if (rmdir(root) != 0 && errno != ENOENT) return 7;

    puts("Cached storage metadata classification passed.");
    return 0;
}
