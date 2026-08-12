// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file storage_metadata.c
 * @brief Parser and classifier for cached block-device metadata.
 *
 * Linux device managers maintain small text records keyed by block major/minor
 * number. Reading those records is sufficient to identify filesystems on
 * unmounted partitions without mounting them, opening raw block devices or
 * linking against a hardware-enumeration library. Parsing remains bounded and
 * allocation-free; unfamiliar metadata is ignored rather than inferred.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "storage_metadata.h"

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#define LSM_STORAGE_METADATA_PATH_LEN 4096U

typedef struct {
    char filesystem[64];
    char version[64];
    char partition_type[64];
} StorageMetadata;

typedef struct {
    const char *guid;
    const char *label;
} PartitionRole;

static const PartitionRole gpt_partition_roles[] = {
    {"e3c9e316-0b5c-4db8-817d-f92df00215ae", "Microsoft Reserved"},
    {"21686148-6449-6e6f-744e-656564454649", "BIOS Boot"},
    {"c12a7328-f81f-11d2-ba4b-00a0c93ec93b", "EFI System"},
    {"de94bba4-06d1-4d40-a16a-bfd50179d6ac", "Windows Recovery"},
    {"ebd0a0a2-b9e5-4433-87c0-68b6b72699c7", "Microsoft Basic Data"},
    {"0657fd6d-a4ab-43c4-84e5-0933c84b4f4f", "Linux Swap"},
    {"a19d880f-05fc-4d3b-a006-743f0f84911e", "Linux RAID"},
    {"e6d6d379-f507-44c2-a23c-238f2a3df928", "Linux LVM"},
    {"ca7d7ccb-63ed-4c53-861c-1742536059cc", "Linux LUKS"},
    {"0fc63daf-8483-4772-8e79-3d69d8477de4", "Linux Filesystem"},
    {"7c3457ef-0000-11aa-aa11-00306543ecac", "Apple APFS"},
    {"48465300-0000-11aa-aa11-00306543ecac", "Apple HFS+"}
};

static void capture_property(char *destination, size_t destination_size,
                             const char *value)
{
    if (!destination || destination_size == 0U || !value) return;
    char clean[128];
    lsm_copy_string(clean, sizeof(clean), value);
    clean[strcspn(clean, "\r\n")] = '\0';
    if (clean[0]) lsm_copy_string(destination, destination_size, clean);
}

static bool read_metadata(const char *data_root,
                          unsigned int major_number,
                          unsigned int minor_number,
                          StorageMetadata *metadata)
{
    if (!data_root || !*data_root || !metadata) return false;
    memset(metadata, 0, sizeof(*metadata));

    char path[LSM_STORAGE_METADATA_PATH_LEN];
    const int written = snprintf(path, sizeof(path), "%s/b%u:%u",
                                 data_root, major_number, minor_number);
    if (written < 0 || (size_t)written >= sizeof(path)) return false;

    FILE *file = fopen(path, "r");
    if (!file) return false;

    static const char filesystem_prefix[] = "E:ID_FS_TYPE=";
    static const char version_prefix[] = "E:ID_FS_VERSION=";
    static const char partition_type_prefix[] = "E:ID_PART_ENTRY_TYPE=";
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, filesystem_prefix,
                    sizeof(filesystem_prefix) - 1U) == 0) {
            capture_property(metadata->filesystem,
                             sizeof(metadata->filesystem),
                             line + sizeof(filesystem_prefix) - 1U);
        } else if (strncmp(line, version_prefix,
                           sizeof(version_prefix) - 1U) == 0) {
            capture_property(metadata->version, sizeof(metadata->version),
                             line + sizeof(version_prefix) - 1U);
        } else if (strncmp(line, partition_type_prefix,
                           sizeof(partition_type_prefix) - 1U) == 0) {
            capture_property(metadata->partition_type,
                             sizeof(metadata->partition_type),
                             line + sizeof(partition_type_prefix) - 1U);
        }
    }
    const bool read_ok = !ferror(file);
    fclose(file);
    return read_ok;
}

static const char *fat_variant(const StorageMetadata *metadata)
{
    if (!metadata ||
        (strcasecmp(metadata->filesystem, "vfat") != 0 &&
         strcasecmp(metadata->filesystem, "msdos") != 0))
        return NULL;
    if (strcasecmp(metadata->version, "FAT12") == 0) return "FAT12";
    if (strcasecmp(metadata->version, "FAT16") == 0) return "FAT16";
    if (strcasecmp(metadata->version, "FAT32") == 0) return "FAT32";
    return NULL;
}

static const char *partition_role(const char *partition_type)
{
    if (!partition_type || !*partition_type) return NULL;
    for (size_t index = 0U;
         index < sizeof(gpt_partition_roles) / sizeof(gpt_partition_roles[0]);
         index++) {
        if (strcasecmp(partition_type, gpt_partition_roles[index].guid) == 0)
            return gpt_partition_roles[index].label;
    }
    return NULL;
}

bool lsm_storage_metadata_type_label(const char *data_root,
                                     unsigned int major_number,
                                     unsigned int minor_number,
                                     char *label, size_t label_size)
{
    if (!label || label_size == 0U) return false;
    label[0] = '\0';

    StorageMetadata metadata;
    if (!read_metadata(data_root, major_number, minor_number, &metadata))
        return false;

    if (metadata.filesystem[0]) {
        const char *variant = fat_variant(&metadata);
        lsm_copy_string(label, label_size,
                        variant ? variant : metadata.filesystem);
        return true;
    }

    const char *role = partition_role(metadata.partition_type);
    if (!role) return false;
    lsm_copy_string(label, label_size, role);
    return true;
}
