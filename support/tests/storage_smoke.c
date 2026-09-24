// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file storage_smoke.c
 * @brief Consolidated storage regression smoke suite.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <stddef.h>
#include <stdio.h>

int smoke_case_mountinfo(void);
int smoke_case_storage_metadata(void);
int smoke_case_filesystem_inventory(void);
int smoke_case_bundled_pci(void);
int smoke_case_smbios_memory(void);
int smoke_case_system_sources(void);

/* ---- mountinfo ---- */
#define main smoke_case_mountinfo
#define write_fixture lsm_test_mountinfo_write_fixture
#define collect_mount lsm_test_mountinfo_collect_mount
/**
 * @file mountinfo_smoke.c
 * @brief Native mountinfo parser regression tests.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
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

#undef main
#undef collect_mount
#undef write_fixture

/* ---- storage_metadata ---- */
#define main smoke_case_storage_metadata
#define write_record lsm_test_storage_metadata_write_record
#define expect_label lsm_test_storage_metadata_expect_label
/**
 * @file storage_metadata_smoke.c
 * @brief Cached block metadata parsing and classification regression test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
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
                      "E:ID_PART_ENTRY_TYPE=00000000-0000-0000-0000-000000000000\n") ||
        !write_record(root, 8U, 8U,
                      "E:ID_FS_TYPE=ext4\rinjected\n")) {
        return 2;
    }

    if (!expect_label(root, 1U, "FAT12") ||
        !expect_label(root, 2U, "FAT16") ||
        !expect_label(root, 3U, "FAT32") ||
        !expect_label(root, 4U, "ntfs") ||
        !expect_label(root, 5U, "Microsoft Reserved") ||
        !expect_label(root, 6U, "ext4") ||
        !expect_label(root, 8U, "ext4"))
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

    for (unsigned int minor_number = 1U; minor_number <= 8U; minor_number++) {
        char path[512];
        const int written = snprintf(path, sizeof(path), "%s/b8:%u",
                                     root, minor_number);
        if (written >= 0 && (size_t)written < sizeof(path)) (void)unlink(path);
    }
    if (rmdir(root) != 0 && errno != ENOENT) return 7;

    puts("Cached storage metadata classification passed.");
    return 0;
}

#undef main
#undef expect_label
#undef write_record
#undef _POSIX_C_SOURCE

/* ---- filesystem_inventory ---- */
#define main smoke_case_filesystem_inventory
#define make_directory lsm_test_filesystem_inventory_make_directory
/**
 * @file filesystem_inventory_smoke.c
 * @brief Regression test for mount classification and capacity snapshots.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
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

#undef main
#undef make_directory
#undef _POSIX_C_SOURCE

/* ---- bundled_pci ---- */
#define main smoke_case_bundled_pci
/**
 * @file bundled_pci_smoke.c
 * @brief Bundled PCI name resolver regression test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "pci_names.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    char vendor[256] = "";
    char product[256] = "";
    if (!lsm_pci_names_lookup("8086", "7e40",
                              vendor, sizeof(vendor),
                              product, sizeof(product))) {
        fputs("PCI lookup returned no result.\n", stderr);
        return 1;
    }
    if (strcmp(vendor, "Intel Corporation") != 0 ||
        strcmp(product, "Meteor Lake PCH CNVi WiFi") != 0) {
        fprintf(stderr, "Unexpected PCI identity: %s / %s\n", vendor, product);
        return 2;
    }
    printf("%s — %s\n", vendor, product);
    return 0;
}

#undef main

/* ---- smbios_memory ---- */
#define main smoke_case_smbios_memory
#define write_le16 lsm_test_smbios_memory_write_le16
#define add_memory_device lsm_test_smbios_memory_add_memory_device
#define strings lsm_test_smbios_memory_strings
/**
 * @file smbios_memory_smoke.c
 * @brief Synthetic SMBIOS Type-17 parser regression test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#include "smbios_memory.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void write_le16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xffu);
    destination[1] = (uint8_t)(value >> 8);
}

static size_t add_memory_device(uint8_t *table, size_t offset,
                                uint16_t size_mb, uint8_t form_factor,
                                uint16_t rated_speed, uint16_t configured_speed,
                                bool include_details)
{
    uint8_t *record = table + offset;
    memset(record, 0, 0x24);
    record[0] = 17;
    record[1] = 0x22;
    write_le16(record + 0x0c, size_mb);
    record[0x0e] = form_factor;
    record[0x12] = 0x22; /* DDR5 */
    write_le16(record + 0x15, rated_speed);
    write_le16(record + 0x20, configured_speed);
    if (include_details) {
        static const char strings[] =
            "DIMM 0\0BANK 0\0Acme Memory\0SER123\0PART-99  \0";
        record[0x10] = 1;
        record[0x11] = 2;
        record[0x17] = 3;
        record[0x18] = 4;
        record[0x1a] = 5;
        memcpy(record + 0x22, strings, sizeof(strings));
        return offset + 0x22 + sizeof(strings);
    }
    /* The structure's empty string set is the terminating pair at 0x22. */
    return offset + 0x24;
}

int main(void)
{
    uint8_t table[256] = {0};
    size_t used = 0;
    used = add_memory_device(table, used, 8192, 0x0d, 4800, 5600, true);
    used = add_memory_device(table, used, 0, 0x0d, 4800, 4800, false);
    table[used] = 127;
    table[used + 1] = 4;
    used += 6; /* Four formatted bytes plus the empty string-set terminator. */

    char path[] = "/tmp/lsm-smbios-XXXXXX";
    const int descriptor = mkstemp(path);
    assert(descriptor >= 0);
    assert(write(descriptor, table, used) == (ssize_t)used);
    close(descriptor);

    LsmSmbiosMemoryInfo info;
    char error[256];
    assert(lsm_smbios_memory_read(path, &info, error, sizeof(error)));
    assert(info.slots_total == 2);
    assert(info.slots_used == 1);
    assert(info.speed_mhz == 5600);
    assert(strcmp(info.form_factor, "SODIMM") == 0);
    assert(info.module_count == 1U);
    assert(info.modules[0].size_bytes == 8192ULL * 1024ULL * 1024ULL);
    assert(info.modules[0].speed_mhz == 5600U);
    assert(strcmp(info.modules[0].locator, "DIMM 0") == 0);
    assert(strcmp(info.modules[0].bank_locator, "BANK 0") == 0);
    assert(strcmp(info.modules[0].manufacturer, "Acme Memory") == 0);
    assert(strcmp(info.modules[0].serial_number, "SER123") == 0);
    assert(strcmp(info.modules[0].part_number, "PART-99") == 0);
    assert(strcmp(info.modules[0].memory_type, "DDR5") == 0);

    /* Firmware text is external input. Invalid UTF-8 must not escape into GTK
     * presentation, while independent valid fields remain available. */
    const size_t manufacturer_offset =
        0x22U + sizeof("DIMM 0") + sizeof("BANK 0");
    assert(manufacturer_offset < used);
    table[manufacturer_offset] = 0xffU;
    const int invalid_utf8 = open(path, O_WRONLY | O_TRUNC);
    assert(invalid_utf8 >= 0);
    assert(write(invalid_utf8, table, used) == (ssize_t)used);
    close(invalid_utf8);
    assert(lsm_smbios_memory_read(path, &info, error, sizeof(error)));
    assert(strcmp(info.modules[0].manufacturer, "N/A") == 0);
    assert(strcmp(info.modules[0].locator, "DIMM 0") == 0);

    const int malformed = open(path, O_WRONLY | O_TRUNC);
    assert(malformed >= 0);
    const uint8_t truncated[] = {17U, 3U, 0U, 0U, 0U, 0U};
    assert(write(malformed, truncated, sizeof(truncated)) ==
           (ssize_t)sizeof(truncated));
    close(malformed);
    assert(!lsm_smbios_memory_read(path, &info, error, sizeof(error)));

    unlink(path);
    puts("SMBIOS memory parser smoke test passed.");
    return 0;
}

#undef main
#undef strings
#undef add_memory_device
#undef write_le16
#undef _POSIX_C_SOURCE

/* ---- system_sources ---- */
#define main smoke_case_system_sources
#define make_directories lsm_test_system_sources_make_directories
#define write_text lsm_test_system_sources_write_text
#define make_link lsm_test_system_sources_make_link
#define remove_tree lsm_test_system_sources_remove_tree
#define setup_fixture lsm_test_system_sources_setup_fixture
/**
 * @file system_sources_smoke.c
 * @brief Synthetic native procfs/sysfs discovery regression test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "system_sources.h"
#include "common.h"

#include <dirent.h>
#include <errno.h>
#include <math.h>
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
    const bool ok = fputs(text, file) >= 0 && fclose(file) == 0;
    return ok;
}

static bool make_link(const char *target, const char *path)
{
    char parent[LSM_PATH_LEN];
    if (!target || !path || strlen(path) >= sizeof(parent)) return false;
    strcpy(parent, path);
    char *separator = strrchr(parent, '/');
    if (!separator) return false;
    *separator = '\0';
    return make_directories(parent) && symlink(target, path) == 0;
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
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char child[LSM_PATH_LEN];
        const int written = snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(child)) continue;
        struct stat information;
        if (lstat(child, &information) != 0) continue;
        if (S_ISDIR(information.st_mode)) remove_tree(child);
        else (void)unlink(child);
    }
    closedir(directory);
    (void)rmdir(path);
}

static bool setup_fixture(char *root, size_t root_size)
{
    char template_path[] = "/tmp/lsm-sources-XXXXXX";
    char *created = mkdtemp(template_path);
    if (!created) return false;
    if (strlen(created) >= root_size) return false;
    strcpy(root, created);

    char path[LSM_PATH_LEN];
#define FIXTURE_FILE(suffix, value) \
    do { \
        if (!lsm_join_path(path, sizeof(path), root, suffix) || \
            !write_text(path, value)) return false; \
    } while (0)
#define FIXTURE_LINK(target, suffix) \
    do { \
        if (!lsm_join_path(path, sizeof(path), root, suffix) || \
            !make_link(target, path)) return false; \
    } while (0)

    FIXTURE_FILE("/sys/block/sda/size", "4096\n");
    FIXTURE_FILE("/sys/block/sda/diskseq", "101\n");
    FIXTURE_FILE("/sys/block/sda/device/vendor", "ATA\n");
    /* Invalid product text is skipped; the valid model remains the next
     * human-readable identity candidate. */
    FIXTURE_FILE("/sys/block/sda/device/product", "\xff\n");
    FIXTURE_FILE("/sys/block/sda/device/model", "Test Disk\n");
    FIXTURE_FILE("/sys/block/sda/device/protocol", "SATA\n");
    FIXTURE_FILE("/sys/block/sda/queue/rotational", "0\n");
    FIXTURE_FILE("/sys/block/sda/sda1/partition", "1\n");
    FIXTURE_FILE("/sys/block/sda/sda1/size", "2048\n");
    FIXTURE_FILE("/sys/block/sda/sda1/dev", "8:1\n");
    FIXTURE_FILE("/sys/block/sda/sda2/partition", "2\n");
    FIXTURE_FILE("/sys/block/sda/sda2/size", "1024\n");
    FIXTURE_FILE("/sys/block/sda/sda2/dev", "8:2\n");
    FIXTURE_FILE("/sys/block/sda/sda3/partition", "3\n");
    FIXTURE_FILE("/sys/block/sda/sda3/size", "256\n");
    FIXTURE_FILE("/sys/block/sda/sda3/dev", "8:3\n");
    FIXTURE_FILE("/sys/block/sda/sda4/partition", "4\n");
    FIXTURE_FILE("/sys/block/sda/sda4/size", "512\n");
    FIXTURE_FILE("/sys/block/sda/sda4/dev", "8:4\n");
    FIXTURE_FILE("/sys/block/sda/sda5/partition", "5\n");
    FIXTURE_FILE("/sys/block/sda/sda5/size", "768\n");
    FIXTURE_FILE("/sys/block/sda/sda5/dev", "8:5\n");
    FIXTURE_FILE("/sys/block/sda/sda6/partition", "6\n");
    FIXTURE_FILE("/sys/block/sda/sda6/size", "1024\n");
    FIXTURE_FILE("/sys/block/sda/sda6/dev", "8:6\n");
    FIXTURE_FILE("/sys/block/sda/sda7/partition", "7\n");
    FIXTURE_FILE("/sys/block/sda/sda7/size", "32\n");
    FIXTURE_FILE("/sys/block/sda/sda7/dev", "8:7\n");

    /* MMC/SD cards publish the CID product name as device/name, not model. */
    FIXTURE_FILE("/sys/block/mmcblk0/size", "8192\n");
    FIXTURE_FILE("/sys/block/mmcblk0/diskseq", "202\n");
    FIXTURE_FILE("/sys/block/mmcblk0/device/name", "SD128 CARD\n");
    FIXTURE_FILE("/sys/block/mmcblk0/queue/rotational", "0\n");

    FIXTURE_LINK("../../block/sda", "/sys/class/block/sda");
    FIXTURE_LINK("../../block/sda/sda1", "/sys/class/block/sda1");
    FIXTURE_LINK("../../block/sda/sda2", "/sys/class/block/sda2");
    FIXTURE_LINK("../../block/sda/sda3", "/sys/class/block/sda3");
    FIXTURE_LINK("../../block/sda/sda4", "/sys/class/block/sda4");
    FIXTURE_LINK("../../block/sda/sda5", "/sys/class/block/sda5");
    FIXTURE_LINK("../../block/sda/sda6", "/sys/class/block/sda6");
    FIXTURE_LINK("../../block/sda/sda7", "/sys/class/block/sda7");
    FIXTURE_LINK("../../block/sda/sda1", "/sys/dev/block/8:1");
    FIXTURE_LINK("../../block/sda/sda6", "/sys/dev/block/8:6");
    FIXTURE_FILE("/run/udev/data/b8:2",
                 "E:ID_FS_USAGE=filesystem\nE:ID_FS_TYPE=ntfs\n");
    FIXTURE_FILE("/run/udev/data/b8:3",
                 "E:ID_FS_VERSION=FAT12\nE:ID_FS_TYPE=vfat\n");
    FIXTURE_FILE("/run/udev/data/b8:4",
                 "E:ID_FS_TYPE=vfat\nE:ID_FS_VERSION=FAT16\n");
    FIXTURE_FILE("/run/udev/data/b8:5",
                 "E:ID_FS_USAGE=filesystem\nE:ID_FS_VERSION=FAT32\nE:ID_FS_TYPE=vfat\n");
    FIXTURE_FILE("/run/udev/data/b8:6",
                 "E:ID_FS_TYPE=vfat\nE:ID_FS_VERSION=FAT32\n");
    FIXTURE_FILE("/run/udev/data/b8:7",
                 "E:ID_PART_ENTRY_SCHEME=gpt\n"
                 "E:ID_PART_ENTRY_TYPE=e3c9e316-0b5c-4db8-817d-f92df00215ae\n");
    FIXTURE_FILE("/proc/self/mountinfo",
                 "36 25 8:1 / /mnt/test rw,relatime - ext4 /dev/sda1 rw\n"
                 "37 25 8:6 / /mnt/fat rw,relatime - vfat /dev/sda6 rw\n");

    FIXTURE_FILE("/sys/class/net/eth0/operstate", "up\n");
    FIXTURE_FILE("/sys/class/net/eth0/address", "00:11:22:33:44:55\n");
    FIXTURE_LINK("../../../devices/pci0000:00/0000:00:01.0",
                 "/sys/class/net/eth0/device");
    FIXTURE_FILE("/sys/devices/pci0000:00/0000:00:01.0/vendor", "0x8086\n");
    FIXTURE_FILE("/sys/devices/pci0000:00/0000:00:01.0/device", "0x7e40\n");
    FIXTURE_LINK("../../../../bus/pci",
                 "/sys/devices/pci0000:00/0000:00:01.0/subsystem");
    FIXTURE_LINK("../../../../bus/pci/drivers/testnet",
                 "/sys/devices/pci0000:00/0000:00:01.0/driver");

    FIXTURE_LINK("../../../devices/pci0000:00/0000:00:02.0",
                 "/sys/class/drm/card0/device");
    FIXTURE_FILE("/sys/devices/pci0000:00/0000:00:02.0/vendor", "0x1002\n");
    FIXTURE_FILE("/sys/devices/pci0000:00/0000:00:02.0/device", "0x164e\n");
    FIXTURE_LINK("../../../../bus/pci",
                 "/sys/devices/pci0000:00/0000:00:02.0/subsystem");
    FIXTURE_LINK("../../../../bus/pci/drivers/amdgpu",
                 "/sys/devices/pci0000:00/0000:00:02.0/driver");

    FIXTURE_FILE("/sys/class/net/eth1/operstate", "up\n");
    FIXTURE_FILE("/sys/class/net/eth1/address", "00:15:5d:00:00:01\n");
    FIXTURE_LINK("../../../devices/vmbus/net0",
                 "/sys/class/net/eth1/device");
    FIXTURE_FILE("/sys/devices/vmbus/net0/vendor", "0x1414\n");
    FIXTURE_FILE("/sys/devices/vmbus/net0/device", "0x0003\n");
    FIXTURE_LINK("../../../bus/vmbus",
                 "/sys/devices/vmbus/net0/subsystem");
    FIXTURE_LINK("../../../bus/vmbus/drivers/hv_netvsc",
                 "/sys/devices/vmbus/net0/driver");

    FIXTURE_LINK("../../../devices/vmbus/video0",
                 "/sys/class/drm/card1/device");
    FIXTURE_FILE("/sys/devices/vmbus/video0/vendor", "0x1414\n");
    FIXTURE_FILE("/sys/devices/vmbus/video0/device", "0x0006\n");
    FIXTURE_LINK("../../../bus/vmbus",
                 "/sys/devices/vmbus/video0/subsystem");
    FIXTURE_LINK("../../../bus/vmbus/drivers/hyperv_drm",
                 "/sys/devices/vmbus/video0/driver");

    FIXTURE_FILE("/sys/class/hwmon/hwmon0/name", "coretemp\n");
    FIXTURE_FILE("/sys/class/hwmon/hwmon0/temp1_input", "42000\n");
    FIXTURE_FILE("/sys/class/hwmon/hwmon0/temp1_label", "Package id 0\n");
    /* A generic ACPI zone can describe a board/chassis sensor. It must not
     * outrank an explicitly identified CPU package source merely because its
     * label contains CPU or its temperature is higher. */
    FIXTURE_FILE("/sys/class/hwmon/hwmon1/name", "acpitz\n");
    FIXTURE_FILE("/sys/class/hwmon/hwmon1/temp1_input", "99000\n");
    FIXTURE_FILE("/sys/class/hwmon/hwmon1/temp1_label", "CPU\n");
#undef FIXTURE_FILE
#undef FIXTURE_LINK
    return true;
}

int main(void)
{
    char root[LSM_PATH_LEN];
    if (!setup_fixture(root, sizeof(root))) return 1;

    char path[LSM_PATH_LEN];
    if (!lsm_join_path(path, sizeof(path), root, "/sys") ||
        setenv("LSM_SYSFS_ROOT", path, 1) != 0) return 2;
    if (!lsm_join_path(path, sizeof(path), root, "/proc") ||
        setenv("LSM_PROCFS_ROOT", path, 1) != 0) return 3;
    if (!lsm_join_path(path, sizeof(path), root, "/dev") ||
        setenv("LSM_DEV_ROOT", path, 1) != 0) return 4;
    if (!lsm_join_path(path, sizeof(path), root, "/run/udev/data") ||
        setenv("LSM_UDEV_DATA_ROOT", path, 1) != 0) return 5;

    LsmSystemSources *sources = NULL;
    if (!lsm_sources_init(&sources)) return 6;
    LsmBlockDeviceRecord disks[4] = {0};
    LsmMountRecord mounts[8] = {0};
    LsmPartitionRecord partitions[8] = {0};
    LsmNetworkRecord networks[4] = {0};
    LsmGpuRecord gpus[4] = {0};

    const size_t disk_count = lsm_sources_list_block_devices(sources, disks, 4);
    const size_t mount_count = lsm_sources_list_mounts(sources, mounts, 8);
    const size_t partition_count = lsm_sources_list_partitions(sources, partitions, 8);
    const size_t network_count = lsm_sources_list_networks(sources, networks, 4);
    const size_t gpu_count = lsm_sources_list_gpus(sources, gpus, 4);
    const double temperature = lsm_sources_read_cpu_temperature(sources);

    if (!lsm_join_path(path, sizeof(path), root, "/sys/block/sda/diskseq") ||
        !write_text(path, "303\n"))
        return 7;
    LsmBlockDeviceRecord replacement_disks[4] = {0};
    const size_t replacement_count =
        lsm_sources_list_block_devices(sources, replacement_disks, 4);
    bool replacement_identity_changed = false;
    for (size_t index = 0U; index < replacement_count; index++)
        if (strcmp(replacement_disks[index].name, "sda") == 0 &&
            strcmp(replacement_disks[index].instance_identity,
                   "diskseq:303") == 0)
            replacement_identity_changed = true;

    bool mounted_ext4 = false;
    bool mounted_fat32 = false;
    bool unmounted_ntfs = false;
    bool unmounted_fat12 = false;
    bool unmounted_fat16 = false;
    bool unmounted_fat32 = false;
    bool microsoft_reserved = false;
    for (size_t index = 0U; index < partition_count; index++) {
        if (partitions[index].mounted &&
            strcmp(partitions[index].mount_point, "/mnt/test") == 0 &&
            strcmp(partitions[index].filesystem, "ext4") == 0)
            mounted_ext4 = true;
        if (partitions[index].mounted &&
            strcmp(partitions[index].mount_point, "/mnt/fat") == 0 &&
            strcmp(partitions[index].filesystem, "FAT32") == 0)
            mounted_fat32 = true;
        if (!partitions[index].mounted &&
            strstr(partitions[index].device, "/sda2") &&
            strcmp(partitions[index].filesystem, "ntfs") == 0)
            unmounted_ntfs = true;
        if (!partitions[index].mounted && strstr(partitions[index].device, "/sda3") &&
            strcmp(partitions[index].filesystem, "FAT12") == 0)
            unmounted_fat12 = true;
        if (!partitions[index].mounted && strstr(partitions[index].device, "/sda4") &&
            strcmp(partitions[index].filesystem, "FAT16") == 0)
            unmounted_fat16 = true;
        if (!partitions[index].mounted && strstr(partitions[index].device, "/sda5") &&
            strcmp(partitions[index].filesystem, "FAT32") == 0)
            unmounted_fat32 = true;
        if (!partitions[index].mounted && strstr(partitions[index].device, "/sda7") &&
            strcmp(partitions[index].filesystem, "Microsoft Reserved") == 0)
            microsoft_reserved = true;
    }

    bool physical_network = false;
    bool hyperv_network = false;
    for (size_t index = 0U; index < network_count; index++) {
        if (strcmp(networks[index].name, "eth0") == 0 &&
            strcmp(networks[index].product, "N/A") != 0)
            physical_network = true;
        if (strcmp(networks[index].name, "eth1") == 0 &&
            strcmp(networks[index].product,
                   "Microsoft Hyper-V Network Adapter") == 0 &&
            strstr(networks[index].product, "PCI ") == NULL)
            hyperv_network = true;
    }
    bool physical_gpu = false;
    bool hyperv_gpu = false;
    for (size_t index = 0U; index < gpu_count; index++) {
        if (strcmp(gpus[index].driver, "amdgpu") == 0)
            physical_gpu = true;
        if (strcmp(gpus[index].driver, "hyperv_drm") == 0 &&
            strcmp(gpus[index].product,
                   "Microsoft Hyper-V Graphics Adapter") == 0 &&
            strstr(gpus[index].product, "PCI ") == NULL)
            hyperv_gpu = true;
    }

    bool sata_disk = false;
    bool mmc_disk = false;
    for (size_t index = 0U; index < disk_count; index++) {
        if (strcmp(disks[index].name, "sda") == 0 &&
            strcmp(disks[index].model, "ATA Test Disk") == 0 &&
            strcmp(disks[index].media_type, "SSD") == 0 &&
            strcmp(disks[index].connection_type, "SATA") == 0 &&
            strcmp(disks[index].instance_identity, "diskseq:101") == 0)
            sata_disk = true;
        if (strcmp(disks[index].name, "mmcblk0") == 0 &&
            strcmp(disks[index].model, "SD128 CARD") == 0 &&
            strcmp(disks[index].connection_type, "MMC") == 0 &&
            strcmp(disks[index].instance_identity, "diskseq:202") == 0)
            mmc_disk = true;
    }

    const bool ok =
        disk_count == 2 && sata_disk && mmc_disk &&
        replacement_identity_changed &&
        mount_count == 2 && strcmp(mounts[0].parent_disk, "sda") == 0 &&
        strcmp(mounts[0].filesystem, "ext4") == 0 &&
        partition_count == 7 && mounted_ext4 && mounted_fat32 && unmounted_ntfs &&
        unmounted_fat12 && unmounted_fat16 && unmounted_fat32 && microsoft_reserved &&
        network_count == 2 && physical_network && hyperv_network &&
        gpu_count == 2 && physical_gpu && hyperv_gpu &&
        isfinite(temperature) && fabs(temperature - 42.0) < 0.01;

    printf("native disks=%zu mounts=%zu partitions=%zu networks=%zu gpus=%zu temp=%.1f\n",
           disk_count, mount_count, partition_count, network_count, gpu_count,
           temperature);
    for (size_t index = 0U; index < network_count; index++)
        printf("network[%zu]=%s product=%s vendor=%s\n", index, networks[index].name,
               networks[index].product, networks[index].vendor);
    for (size_t index = 0U; index < gpu_count; index++)
        printf("gpu[%zu]=%s product=%s vendor=%s driver=%s\n", index, gpus[index].card,
               gpus[index].product, gpus[index].vendor, gpus[index].driver);
    lsm_sources_destroy(sources);
    remove_tree(root);
    return ok ? 0 : 7;
}

#undef main
#undef setup_fixture
#undef remove_tree
#undef make_link
#undef write_text
#undef make_directories
#undef _POSIX_C_SOURCE
#undef FIXTURE_FILE
#undef FIXTURE_LINK

typedef int (*LsmMergedSmokeCaseFunction)(void);
typedef struct { const char *name; LsmMergedSmokeCaseFunction function; } LsmMergedSmokeCase;

int main(void)
{
    static const LsmMergedSmokeCase cases[] = {
        {"mountinfo", smoke_case_mountinfo},
        {"storage_metadata", smoke_case_storage_metadata},
        {"filesystem_inventory", smoke_case_filesystem_inventory},
        {"bundled_pci", smoke_case_bundled_pci},
        {"smbios_memory", smoke_case_smbios_memory},
        {"system_sources", smoke_case_system_sources},
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0U; i < count; ++i) {
        const int status = cases[i].function();
        if (status != 0) {
            fprintf(stderr, "storage smoke suite: %s failed with status %d\n", cases[i].name, status);
            return status;
        }
    }
    printf("storage smoke suite passed (%zu cases).\n", count);
    return 0;
}
