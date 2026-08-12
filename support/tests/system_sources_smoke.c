// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system_sources_smoke.c
 * @brief Synthetic native procfs/sysfs discovery regression test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
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
    FIXTURE_FILE("/sys/block/sda/device/vendor", "ATA\n");
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
            strcmp(disks[index].connection_type, "SATA") == 0)
            sata_disk = true;
        if (strcmp(disks[index].name, "mmcblk0") == 0 &&
            strcmp(disks[index].model, "SD128 CARD") == 0 &&
            strcmp(disks[index].connection_type, "MMC") == 0)
            mmc_disk = true;
    }

    const bool ok =
        disk_count == 2 && sata_disk && mmc_disk &&
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
