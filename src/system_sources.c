// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system_sources.c
 * @brief Direct device, netlink and kernel-interface hardware collection.
 *
 * Rtnetlink, driver ioctls and device nodes are preferred where they expose the
 * required data. Stable sysfs/procfs ABIs remain bounded fallbacks. Friendly PCI
 * names use the project-bundled database; libudev/libsensors are not required.
 * Cached block metadata is delegated to storage_metadata.c, which supplies
 * filesystem and partition-role labels without mounting or privileged access.
 *
 * The context owns request and event sockets plus fixture-aware root paths.
 * Enumerators return bounded plain-C records and never retain caller buffers.
 * Network topology is event-invalidated, avoiding repeated address enumeration
 * on the fast cadence while preserving a periodic fallback on limited kernels.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "system_sources.h"

#include "common.h"
#include "mountinfo.h"
#include "pci_names.h"
#include "storage_metadata.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <linux/if_link.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <linux/wireless.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

struct LsmSystemSources {
    char sysfs_root[LSM_PATH_LEN];
    char procfs_root[LSM_PATH_LEN];
    char dev_root[LSM_PATH_LEN];
    char device_data_root[LSM_PATH_LEN];
    int network_request_fd;
    int network_event_fd;
    uint32_t network_sequence;
};

/* Fixture-aware path helpers keep production and synthetic backends identical. */
static void source_root(char *destination, size_t size,
                        const char *environment_name, const char *fallback)
{
    const char *value = getenv(environment_name);
    lsm_copy_string(destination, size, value && *value ? value : fallback);
    size_t length = strlen(destination);
    while (length > 1U && destination[length - 1U] == '/')
        destination[--length] = '\0';
}

static bool source_path(char *destination, size_t size,
                        const char *root, const char *suffix)
{
    return lsm_join_path(destination, size, root, suffix);
}

static bool child_path(char *destination, size_t size,
                       const char *base, const char *name,
                       const char *suffix)
{
    if (!destination || size == 0U || !base || !name || !suffix) return false;
    const int written = snprintf(destination, size, "%s/%s%s", base, name, suffix);
    if (written < 0 || (size_t)written >= size) {
        destination[0] = '\0';
        return false;
    }
    return true;
}

static bool block_device_numbers(LsmSystemSources *sources,
                                 const char *block_name,
                                 unsigned int *major_number,
                                 unsigned int *minor_number)
{
    if (!sources || !block_name || !*block_name ||
        !major_number || !minor_number) return false;
    char root[LSM_PATH_LEN];
    char path[LSM_PATH_LEN];
    char text[64] = "";
    if (!source_path(root, sizeof(root), sources->sysfs_root, "/class/block") ||
        !child_path(path, sizeof(path), root, block_name, "/dev") ||
        !lsm_read_text_file(path, text, sizeof(text)))
        return false;

    const char *cursor = text;
    uint64_t major_value = 0U;
    uint64_t minor_value = 0U;
    if (!lsm_parse_u64_token(&cursor, 10U, &major_value) || *cursor != ':')
        return false;
    cursor++;
    if (!lsm_parse_u64_token(&cursor, 10U, &minor_value) ||
        *cursor != '\0' || major_value > UINT_MAX || minor_value > UINT_MAX)
        return false;
    *major_number = (unsigned int)major_value;
    *minor_number = (unsigned int)minor_value;
    return true;
}

/** Resolve cached storage identity after translating the kernel block name. */
static bool filesystem_label_from_device_database(LsmSystemSources *sources,
                                                   const char *block_name,
                                                   char *filesystem,
                                                   size_t filesystem_size)
{
    if (!sources || !block_name || !filesystem || filesystem_size == 0U)
        return false;
    unsigned int major_number = 0U;
    unsigned int minor_number = 0U;
    if (!block_device_numbers(sources, block_name,
                              &major_number, &minor_number))
        return false;
    return lsm_storage_metadata_type_label(sources->device_data_root,
                                           major_number, minor_number,
                                           filesystem, filesystem_size);
}

static const char *path_basename(const char *path)
{
    if (!path) return "";
    const char *separator = strrchr(path, '/');
    return separator ? separator + 1 : path;
}

static bool read_symlink_basename(const char *path, char *destination, size_t size)
{
    if (!path || !destination || size == 0U) return false;
    char link[LSM_PATH_LEN];
    const ssize_t length = readlink(path, link, sizeof(link) - 1U);
    if (length <= 0) {
        destination[0] = '\0';
        return false;
    }
    link[length] = '\0';
    lsm_copy_string(destination, size, path_basename(link));
    return destination[0] != '\0';
}

static uint64_t direct_block_size(const LsmSystemSources *sources,
                                  const char *name)
{
    if (!sources || !name || !*name) return 0U;
    char path[LSM_PATH_LEN];
    const int written = snprintf(path, sizeof(path), "%s/%s",
                                 sources->dev_root, name);
    if (written < 0 || (size_t)written >= sizeof(path)) return 0U;
    const int descriptor = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0) return 0U;
    uint64_t bytes = 0U;
    const bool ok = ioctl(descriptor, BLKGETSIZE64, &bytes) == 0;
    close(descriptor);
    return ok ? bytes : 0U;
}

static bool ignored_block_name(const char *name)
{
    return !name || strncmp(name, "loop", 4) == 0 || strncmp(name, "ram", 3) == 0 ||
           strncmp(name, "zram", 4) == 0 || strncmp(name, "fd", 2) == 0 ||
           strncmp(name, "sr", 2) == 0 || strncmp(name, "dm-", 3) == 0;
}

static bool directory_entry(const char *path)
{
    struct stat information;
    return path && stat(path, &information) == 0 && S_ISDIR(information.st_mode);
}

static bool regular_attribute(const char *path)
{
    return path && access(path, R_OK) == 0;
}

static void combine_identity(char *destination, size_t size,
                             const char *vendor, const char *product,
                             const char *fallback)
{
    char clean_vendor[LSM_NAME_LEN] = "";
    char clean_product[LSM_NAME_LEN] = "";
    lsm_copy_string(clean_vendor, sizeof(clean_vendor), vendor);
    lsm_copy_string(clean_product, sizeof(clean_product), product);
    lsm_trim(clean_vendor);
    lsm_trim(clean_product);

    if (clean_vendor[0] && clean_product[0] &&
        strcasestr(clean_product, clean_vendor) != clean_product) {
        const int written = snprintf(destination, size, "%s %s",
                                     clean_vendor, clean_product);
        if (written >= 0 && (size_t)written < size) return;
    }
    if (clean_product[0]) lsm_copy_string(destination, size, clean_product);
    else if (clean_vendor[0]) lsm_copy_string(destination, size, clean_vendor);
    else lsm_copy_string(destination, size, fallback);
}

/**
 * Walk upward from a class device until a real bus device is found.
 *
 * PCI functions expose vendor/device. USB interfaces usually expose those
 * attributes one directory above the interface node. Platform and virtio
 * devices are retained as the final useful parent even without numeric IDs.
 */
static bool hardware_parent(const char *class_device,
                            char *destination, size_t destination_size)
{
    if (!class_device || !destination || destination_size == 0U) return false;
    char current[LSM_PATH_LEN];
    if (!lsm_realpath_copy(class_device, current, sizeof(current))) {
        destination[0] = '\0';
        return false;
    }

    char best[LSM_PATH_LEN] = "";
    for (unsigned depth = 0U; depth < 12U; depth++) {
        char attribute[LSM_PATH_LEN];
        bool recognised = false;
        if (lsm_join_path(attribute, sizeof(attribute), current, "/vendor") &&
            regular_attribute(attribute))
            recognised = true;
        if (!recognised && lsm_join_path(attribute, sizeof(attribute), current, "/idVendor") &&
            regular_attribute(attribute))
            recognised = true;
        if (!recognised && lsm_join_path(attribute, sizeof(attribute), current, "/driver") &&
            access(attribute, F_OK) == 0)
            lsm_copy_string(best, sizeof(best), current);
        if (recognised) {
            lsm_copy_string(destination, destination_size, current);
            return true;
        }

        char *separator = strrchr(current, '/');
        if (!separator || separator == current) break;
        *separator = '\0';
    }

    if (best[0]) {
        lsm_copy_string(destination, destination_size, best);
        return true;
    }
    destination[0] = '\0';
    return false;
}

static void read_identity_attribute(const char *base,
                                    const char *const *names, size_t name_count,
                                    char *destination, size_t destination_size)
{
    if (!base || !names || !destination || destination_size == 0U) return;
    destination[0] = '\0';
    for (size_t index = 0U; index < name_count; index++) {
        char path[LSM_PATH_LEN];
        char value[LSM_NAME_LEN] = "";
        if (!lsm_join_path(path, sizeof(path), base, names[index]) ||
            !lsm_read_text_file(path, value, sizeof(value)))
            continue;
        lsm_trim(value);
        if (!value[0]) continue;
        lsm_copy_string(destination, destination_size, value);
        return;
    }
}

static void native_device_identity(const char *hardware_path,
                                   char *product, size_t product_size,
                                   char *vendor, size_t vendor_size)
{
    lsm_copy_string(product, product_size, "N/A");
    lsm_copy_string(vendor, vendor_size, "N/A");
    if (!hardware_path || !*hardware_path) return;

    char path[LSM_PATH_LEN];
    char subsystem[64] = "";
    char driver[64] = "";
    if (lsm_join_path(path, sizeof(path), hardware_path, "/subsystem"))
        (void)read_symlink_basename(path, subsystem, sizeof(subsystem));
    if (lsm_join_path(path, sizeof(path), hardware_path, "/driver"))
        (void)read_symlink_basename(path, driver, sizeof(driver));

    char vendor_id[32] = "";
    char device_id[32] = "";
    if (lsm_join_path(path, sizeof(path), hardware_path, "/vendor"))
        (void)lsm_read_text_file(path, vendor_id, sizeof(vendor_id));
    if (lsm_join_path(path, sizeof(path), hardware_path, "/device"))
        (void)lsm_read_text_file(path, device_id, sizeof(device_id));

    const bool pci_device = strcmp(subsystem, "pci") == 0;
    char database_vendor[LSM_NAME_LEN] = "";
    char database_product[LSM_NAME_LEN] = "";
    if (pci_device && vendor_id[0] && device_id[0] &&
        lsm_pci_names_lookup(vendor_id, device_id,
                             database_vendor, sizeof(database_vendor),
                             database_product, sizeof(database_product))) {
        if (database_product[0])
            lsm_copy_string(product, product_size, database_product);
        if (database_vendor[0])
            lsm_copy_string(vendor, vendor_size, database_vendor);
    }

    static const char *const product_names[] = {
        "/product", "/model", "/name", "/device_name"
    };
    static const char *const vendor_names[] = {
        "/manufacturer", "/vendor_name"
    };
    char local_product[LSM_NAME_LEN] = "";
    char local_vendor[LSM_NAME_LEN] = "";
    read_identity_attribute(hardware_path, product_names,
                            LSM_ARRAY_LENGTH(product_names),
                            local_product, sizeof(local_product));
    read_identity_attribute(hardware_path, vendor_names,
                            LSM_ARRAY_LENGTH(vendor_names),
                            local_vendor, sizeof(local_vendor));
    if (strcmp(product, "N/A") == 0 && local_product[0])
        lsm_copy_string(product, product_size, local_product);
    if (strcmp(vendor, "N/A") == 0 && local_vendor[0])
        lsm_copy_string(vendor, vendor_size, local_vendor);

    /* VMBus devices expose numeric vendor/device attributes that are not PCI
     * IDs. Never label them as PCI hardware; use the native Hyper-V driver
     * identity when firmware/sysfs does not provide a friendly product name. */
    if (strcmp(product, "N/A") == 0) {
        if (strcmp(driver, "hv_netvsc") == 0)
            lsm_copy_string(product, product_size,
                            "Microsoft Hyper-V Network Adapter");
        else if (strcmp(driver, "hyperv_drm") == 0 ||
                 strcmp(driver, "hyperv_fb") == 0)
            lsm_copy_string(product, product_size,
                            "Microsoft Hyper-V Graphics Adapter");
        else if (strcmp(driver, "hv_storvsc") == 0)
            lsm_copy_string(product, product_size,
                            "Microsoft Hyper-V Storage Adapter");
    }
    if (strcmp(vendor, "N/A") == 0 &&
        (strncmp(driver, "hv_", 3) == 0 ||
         strncmp(driver, "hyperv_", 7) == 0))
        lsm_copy_string(vendor, vendor_size, "Microsoft Corporation");

    if (strcmp(product, "N/A") == 0 && pci_device &&
        vendor_id[0] && device_id[0]) {
        const char *vendor_text = strncmp(vendor_id, "0x", 2) == 0
            ? vendor_id + 2 : vendor_id;
        const char *device_text = strncmp(device_id, "0x", 2) == 0
            ? device_id + 2 : device_id;
        (void)snprintf(product, product_size, "PCI %.4s:%.4s",
                       vendor_text, device_text);
    }
    if (strcmp(vendor, "N/A") == 0 && pci_device && vendor_id[0])
        lsm_copy_string(vendor, vendor_size, vendor_id);
}



/* rtnetlink provides event-driven interface membership and 64-bit counters
 * without opening per-interface sysfs counter files every sample. */
static int route_socket(unsigned groups, bool nonblocking)
{
    int flags = SOCK_RAW | SOCK_CLOEXEC;
#ifdef SOCK_NONBLOCK
    if (nonblocking) flags |= SOCK_NONBLOCK;
#else
    (void)nonblocking;
#endif
    const int descriptor = socket(AF_NETLINK, flags, NETLINK_ROUTE);
    if (descriptor < 0) return -1;

#ifndef SOCK_NONBLOCK
    if (nonblocking) {
        const int current = fcntl(descriptor, F_GETFL, 0);
        if (current >= 0) (void)fcntl(descriptor, F_SETFL, current | O_NONBLOCK);
    }
#endif

    struct sockaddr_nl address;
    memset(&address, 0, sizeof(address));
    address.nl_family = AF_NETLINK;
    address.nl_groups = groups;
    if (bind(descriptor, (struct sockaddr *)(void *)&address, sizeof(address)) != 0) {
        close(descriptor);
        return -1;
    }
    if (!nonblocking) {
        const struct timeval timeout = {.tv_sec = 0, .tv_usec = 250000};
        (void)setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO,
                         &timeout, sizeof(timeout));
        (void)setsockopt(descriptor, SOL_SOCKET, SO_SNDTIMEO,
                         &timeout, sizeof(timeout));
    }
    return descriptor;
}

static bool wireless_interface(const char *name)
{
    if (!name || !*name) return false;
    const int descriptor = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (descriptor < 0) return false;
    struct iwreq request;
    memset(&request, 0, sizeof(request));
    lsm_copy_string(request.ifr_name, sizeof(request.ifr_name), name);
    const bool wireless = ioctl(descriptor, SIOCGIWNAME, &request) == 0;
    close(descriptor);
    return wireless;
}

typedef struct {
    char name[64];
    char mac[32];
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    unsigned char operstate;
    unsigned flags;
} LsmRouteLink;

static void format_mac(const unsigned char *bytes, size_t length,
                       char *destination, size_t destination_size)
{
    if (!bytes || length < 6U || !destination || destination_size == 0U) return;
    (void)snprintf(destination, destination_size,
                   "%02x:%02x:%02x:%02x:%02x:%02x",
                   bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]);
}


/* Netlink attributes are only RTA-aligned. Copy integer-bearing payloads into
 * naturally aligned locals before access so the code remains defined on strict
 * alignment architectures as well as x86. */
static void apply_route_link_attribute(LsmRouteLink *record,
                                       const struct rtattr *attribute)
{
    if (!record || !attribute) return;

    switch (attribute->rta_type) {
    case IFLA_IFNAME:
        lsm_copy_string(record->name, sizeof(record->name), RTA_DATA(attribute));
        break;
    case IFLA_ADDRESS:
        format_mac(RTA_DATA(attribute), RTA_PAYLOAD(attribute),
                   record->mac, sizeof(record->mac));
        break;
    case IFLA_OPERSTATE:
        if (RTA_PAYLOAD(attribute) >= sizeof(record->operstate))
            memcpy(&record->operstate, RTA_DATA(attribute),
                   sizeof(record->operstate));
        break;
    case IFLA_STATS64:
        if (RTA_PAYLOAD(attribute) >= sizeof(struct rtnl_link_stats64)) {
            struct rtnl_link_stats64 statistics;
            memcpy(&statistics, RTA_DATA(attribute), sizeof(statistics));
            record->rx_bytes = statistics.rx_bytes;
            record->tx_bytes = statistics.tx_bytes;
        }
        break;
    case IFLA_STATS:
        if (record->rx_bytes == 0U && record->tx_bytes == 0U &&
            RTA_PAYLOAD(attribute) >= sizeof(struct rtnl_link_stats)) {
            struct rtnl_link_stats statistics;
            memcpy(&statistics, RTA_DATA(attribute), sizeof(statistics));
            record->rx_bytes = statistics.rx_bytes;
            record->tx_bytes = statistics.tx_bytes;
        }
        break;
    default:
        break;
    }
}

static bool netlink_message_ok(const struct nlmsghdr *header, int remaining)
{
    if (!header || remaining < (int)sizeof(*header)) return false;
    return header->nlmsg_len >= sizeof(*header) &&
           header->nlmsg_len <= (unsigned int)remaining;
}

static size_t route_link_dump(LsmSystemSources *sources,
                              LsmRouteLink *records, size_t capacity)
{
    if (!sources || !records || capacity == 0U || sources->network_request_fd < 0)
        return 0U;

    struct {
        struct nlmsghdr header;
        struct ifinfomsg message;
    } request;
    memset(&request, 0, sizeof(request));
    request.header.nlmsg_len = NLMSG_LENGTH(sizeof(request.message));
    request.header.nlmsg_type = RTM_GETLINK;
    request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    request.header.nlmsg_seq = ++sources->network_sequence;
    request.message.ifi_family = AF_UNSPEC;

    if (send(sources->network_request_fd, &request, request.header.nlmsg_len, 0) < 0)
        return 0U;

    size_t count = 0U;
    bool complete = false;
    while (!complete) {
        unsigned char buffer[32768];
        const ssize_t received = recv(sources->network_request_fd, buffer, sizeof(buffer), 0);
        if (received < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (received == 0) break;

        int remaining = (int)received;
        for (struct nlmsghdr *header = (struct nlmsghdr *)(void *)buffer;
             netlink_message_ok(header, remaining);
             header = NLMSG_NEXT(header, remaining)) {
            if (header->nlmsg_seq != request.header.nlmsg_seq) continue;
            if (header->nlmsg_type == NLMSG_DONE) {
                complete = true;
                break;
            }
            if (header->nlmsg_type == NLMSG_ERROR) {
                complete = true;
                break;
            }
            if (header->nlmsg_type != RTM_NEWLINK || count >= capacity) continue;

            struct ifinfomsg *information = NLMSG_DATA(header);
            if ((information->ifi_flags & IFF_LOOPBACK) != 0U) continue;
            LsmRouteLink record;
            memset(&record, 0, sizeof(record));
            record.flags = information->ifi_flags;
            record.operstate = IF_OPER_UNKNOWN;

            int attribute_length = IFLA_PAYLOAD(header);
            for (struct rtattr *attribute = IFLA_RTA(information);
                 RTA_OK(attribute, attribute_length);
                 attribute = RTA_NEXT(attribute, attribute_length))
                apply_route_link_attribute(&record, attribute);
            if (!record.name[0]) continue;
            records[count++] = record;
        }
    }
    return count;
}

bool lsm_sources_init(LsmSystemSources **out)
{
    if (!out) return false;
    LsmSystemSources *sources = calloc(1U, sizeof(*sources));
    if (!sources) return false;
    sources->network_request_fd = -1;
    sources->network_event_fd = -1;
    source_root(sources->sysfs_root, sizeof(sources->sysfs_root),
                "LSM_SYSFS_ROOT", "/sys");
    source_root(sources->procfs_root, sizeof(sources->procfs_root),
                "LSM_PROCFS_ROOT", "/proc");
    source_root(sources->dev_root, sizeof(sources->dev_root),
                "LSM_DEV_ROOT", "/dev");
    source_root(sources->device_data_root, sizeof(sources->device_data_root),
                "LSM_UDEV_DATA_ROOT", "/run/udev/data");
    if (strcmp(sources->sysfs_root, "/sys") == 0) {
        sources->network_request_fd = route_socket(0U, false);
        sources->network_event_fd = route_socket(
            RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR, true);
    }
    *out = sources;
    return true;
}

void lsm_sources_destroy(LsmSystemSources *sources)
{
    if (!sources) return;
    if (sources->network_request_fd >= 0) close(sources->network_request_fd);
    if (sources->network_event_fd >= 0) close(sources->network_event_fd);
    free(sources);
}

static void read_block_characteristics(const char *root, const char *name,
                                       const char *device_path,
                                       LsmBlockDeviceRecord *record)
{
    if (!root || !name || !device_path || !record) return;
    char path[LSM_PATH_LEN];
    if (child_path(path, sizeof(path), root, name, "/queue/rotational") &&
        access(path, R_OK) == 0) {
        lsm_copy_string(record->media_type, sizeof(record->media_type),
                        lsm_read_u64_or_zero(path) == 0U ? "SSD" : "HDD");
    } else if (strncmp(name, "nvme", 4U) == 0 ||
               strncmp(name, "mmcblk", 6U) == 0) {
        lsm_copy_string(record->media_type, sizeof(record->media_type), "SSD");
    } else {
        lsm_copy_string(record->media_type, sizeof(record->media_type), "N/A");
    }

    char canonical[LSM_PATH_LEN] = "";
    (void)lsm_realpath_copy(device_path, canonical, sizeof(canonical));
    const char *connection = NULL;
    if (strncmp(name, "nvme", 4U) == 0 || strstr(canonical, "/nvme"))
        connection = "NVMe";
    else if (strncmp(name, "mmcblk", 6U) == 0 || strstr(canonical, "/mmc"))
        connection = "MMC";
    else if (strncmp(name, "vd", 2U) == 0 || strstr(canonical, "/virtio"))
        connection = "VirtIO";
    else if (strstr(canonical, "/usb"))
        connection = "USB";
    else if (strstr(canonical, "/ata"))
        connection = "SATA";

    if (!connection &&
        child_path(path, sizeof(path), root, name, "/device/protocol")) {
        char protocol[32] = "";
        if (lsm_read_text_file(path, protocol, sizeof(protocol)) && protocol[0])
            lsm_copy_string(record->connection_type,
                            sizeof(record->connection_type), protocol);
    }
    if (!record->connection_type[0])
        lsm_copy_string(record->connection_type,
                        sizeof(record->connection_type),
                        connection ? connection : "SCSI");
}

/* Block and mount inventory. Device-node ioctls are preferred for capacity;
 * sysfs remains the unprivileged identity and topology fallback. */
size_t lsm_sources_list_block_devices(LsmSystemSources *sources,
                                      LsmBlockDeviceRecord *records,
                                      size_t capacity)
{
    if (!sources || !records || capacity == 0U) return 0U;
    char root[LSM_PATH_LEN];
    if (!source_path(root, sizeof(root), sources->sysfs_root, "/block")) return 0U;
    DIR *directory = opendir(root);
    if (!directory) return 0U;

    size_t count = 0U;
    struct dirent *entry = NULL;
    while (count < capacity && (entry = readdir(directory))) {
        if (entry->d_name[0] == '.' || ignored_block_name(entry->d_name)) continue;
        char device_path[LSM_PATH_LEN];
        if (!child_path(device_path, sizeof(device_path), root, entry->d_name, "") ||
            !directory_entry(device_path))
            continue;

        LsmBlockDeviceRecord *record = &records[count++];
        memset(record, 0, sizeof(*record));
        lsm_copy_string(record->name, sizeof(record->name), entry->d_name);
        read_block_characteristics(root, entry->d_name, device_path, record);

        char path[LSM_PATH_LEN];
        record->size_bytes = direct_block_size(sources, entry->d_name);
        if (!record->size_bytes &&
            child_path(path, sizeof(path), root, entry->d_name, "/size"))
            record->size_bytes = lsm_u64_multiply_saturating(
                lsm_read_u64_or_zero(path), 512U);

        char vendor[LSM_NAME_LEN] = "";
        char model[LSM_NAME_LEN] = "";
        if (child_path(path, sizeof(path), root, entry->d_name, "/device/vendor"))
            (void)lsm_read_text_file(path, vendor, sizeof(vendor));
        if (child_path(path, sizeof(path), root, entry->d_name, "/device/model"))
            (void)lsm_read_text_file(path, model, sizeof(model));
        lsm_trim(model);

        /* MMC/SD cards expose the CID product name through device/name rather
         * than the SCSI-style device/model attribute used by SATA/NVMe paths.
         * Prefer that native kernel identity before falling back to mmcblkN. */
        if (!model[0] && strncmp(entry->d_name, "mmcblk", 6U) == 0 &&
            child_path(path, sizeof(path), root, entry->d_name, "/device/name")) {
            (void)lsm_read_text_file(path, model, sizeof(model));
            lsm_trim(model);
        }
        combine_identity(record->model, sizeof(record->model),
                         vendor, model, entry->d_name);
    }
    closedir(directory);
    return count;
}

static void resolve_block_identity(LsmSystemSources *sources,
                                   unsigned major_number, unsigned minor_number,
                                   char *block_name, size_t block_name_size,
                                   char *parent, size_t parent_size)
{
    if (block_name && block_name_size) block_name[0] = '\0';
    if (parent && parent_size) parent[0] = '\0';
    if (!sources || major_number == 0U) return;

    char relative[96];
    const int written = snprintf(relative, sizeof(relative),
                                 "/dev/block/%u:%u", major_number, minor_number);
    if (written < 0 || (size_t)written >= sizeof(relative)) return;
    char link[LSM_PATH_LEN];
    if (!source_path(link, sizeof(link), sources->sysfs_root, relative)) return;

    char canonical[LSM_PATH_LEN];
    if (!lsm_realpath_copy(link, canonical, sizeof(canonical))) return;
    const char *name = path_basename(canonical);
    if (block_name && block_name_size)
        lsm_copy_string(block_name, block_name_size, name);

    char partition_path[LSM_PATH_LEN];
    if (!lsm_join_path(partition_path, sizeof(partition_path), canonical, "/partition") ||
        access(partition_path, R_OK) != 0) {
        if (parent && parent_size) lsm_copy_string(parent, parent_size, name);
        return;
    }

    char parent_path[LSM_PATH_LEN];
    lsm_copy_string(parent_path, sizeof(parent_path), canonical);
    char *separator = strrchr(parent_path, '/');
    if (!separator) return;
    *separator = '\0';
    if (parent && parent_size)
        lsm_copy_string(parent, parent_size, path_basename(parent_path));
}

typedef struct {
    LsmSystemSources *sources;
    LsmMountRecord *records;
    size_t capacity;
    size_t count;
} LsmMountCollector;

static bool collect_mount(const LsmMountInfoEntry *entry, void *user_data)
{
    LsmMountCollector *collector = user_data;
    if (!collector || collector->count >= collector->capacity) return false;
    if (entry->major_number == 0U) return true;

    LsmMountRecord *record = &collector->records[collector->count];
    memset(record, 0, sizeof(*record));
    resolve_block_identity(collector->sources,
                           entry->major_number, entry->minor_number,
                           record->block_name, sizeof(record->block_name),
                           record->parent_disk, sizeof(record->parent_disk));
    if (!record->block_name[0] || !record->parent_disk[0]) return true;
    lsm_copy_string(record->source, sizeof(record->source), entry->source);
    lsm_copy_string(record->target, sizeof(record->target), entry->target);
    lsm_copy_string(record->filesystem, sizeof(record->filesystem), entry->filesystem);
    if (strcasecmp(entry->filesystem, "vfat") == 0 ||
        strcasecmp(entry->filesystem, "msdos") == 0) {
        char precise_filesystem[64] = "";
        if (filesystem_label_from_device_database(
                collector->sources, record->block_name,
                precise_filesystem, sizeof(precise_filesystem)) &&
            strncasecmp(precise_filesystem, "FAT", 3U) == 0)
            lsm_copy_string(record->filesystem, sizeof(record->filesystem),
                            precise_filesystem);
    }
    collector->count++;
    return collector->count < collector->capacity;
}

size_t lsm_sources_list_mounts(LsmSystemSources *sources,
                               LsmMountRecord *records,
                               size_t capacity)
{
    if (!sources || !records || capacity == 0U) return 0U;
    char path[LSM_PATH_LEN];
    if (!source_path(path, sizeof(path), sources->procfs_root, "/self/mountinfo"))
        return 0U;
    LsmMountCollector collector = {
        .sources = sources,
        .records = records,
        .capacity = capacity,
        .count = 0U
    };
    (void)lsm_mountinfo_visit_file(path, collect_mount, &collector);
    return collector.count;
}

static size_t append_partition_record(LsmPartitionRecord *records, size_t count,
                                      size_t capacity, const char *device,
                                      const char *mount_point, const char *filesystem,
                                      const char *parent_disk, uint64_t size_bytes,
                                      bool mounted)
{
    if (!records || count >= capacity || !device || !parent_disk) return count;
    LsmPartitionRecord *record = &records[count++];
    memset(record, 0, sizeof(*record));
    lsm_copy_string(record->device, sizeof(record->device), device);
    lsm_copy_string(record->mount_point, sizeof(record->mount_point),
                    mount_point ? mount_point : "");
    lsm_copy_string(record->filesystem, sizeof(record->filesystem),
                    filesystem && *filesystem ? filesystem : "partition");
    lsm_copy_string(record->parent_disk, sizeof(record->parent_disk), parent_disk);
    record->size_bytes = size_bytes;
    record->mounted = mounted;
    return count;
}

size_t lsm_sources_list_partitions(LsmSystemSources *sources,
                                   LsmPartitionRecord *records,
                                   size_t capacity)
{
    if (!sources || !records || capacity == 0U) return 0U;
    const size_t mount_capacity = 2048U;
    LsmMountRecord *mounts = calloc(mount_capacity, sizeof(*mounts));
    const size_t mount_count = mounts
        ? lsm_sources_list_mounts(sources, mounts, mount_capacity) : 0U;

    char root[LSM_PATH_LEN];
    if (!source_path(root, sizeof(root), sources->sysfs_root, "/class/block")) {
        free(mounts);
        return 0U;
    }
    DIR *directory = opendir(root);
    if (!directory) {
        free(mounts);
        return 0U;
    }

    size_t count = 0U;
    struct dirent *entry = NULL;
    while (count < capacity && (entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
        char canonical[LSM_PATH_LEN];
        char class_path[LSM_PATH_LEN];
        if (!child_path(class_path, sizeof(class_path), root, entry->d_name, "") ||
            !lsm_realpath_copy(class_path, canonical, sizeof(canonical)))
            continue;

        char partition_path[LSM_PATH_LEN];
        const bool is_partition =
            lsm_join_path(partition_path, sizeof(partition_path), canonical, "/partition") &&
            access(partition_path, R_OK) == 0;
        char parent_name[64] = "";
        if (is_partition) {
            char parent_path[LSM_PATH_LEN];
            lsm_copy_string(parent_path, sizeof(parent_path), canonical);
            char *separator = strrchr(parent_path, '/');
            if (!separator) continue;
            *separator = '\0';
            lsm_copy_string(parent_name, sizeof(parent_name), path_basename(parent_path));
        } else {
            lsm_copy_string(parent_name, sizeof(parent_name), entry->d_name);
        }
        if (ignored_block_name(parent_name)) continue;

        bool has_mount = false;
        for (size_t mount_index = 0U;
             mount_index < mount_count && count < capacity; mount_index++) {
            if (strcmp(mounts[mount_index].block_name, entry->d_name) != 0) continue;
            has_mount = true;
            char device[LSM_PATH_LEN];
            const int written = snprintf(device, sizeof(device), "%s/%s",
                                         sources->dev_root, entry->d_name);
            if (written < 0 || (size_t)written >= sizeof(device)) continue;
            char size_path[LSM_PATH_LEN];
            uint64_t size_bytes = direct_block_size(sources, entry->d_name);
            if (!size_bytes &&
                child_path(size_path, sizeof(size_path), root, entry->d_name, "/size"))
                size_bytes = lsm_u64_multiply_saturating(
                    lsm_read_u64_or_zero(size_path), 512U);
            count = append_partition_record(
                records, count, capacity, device, mounts[mount_index].target,
                mounts[mount_index].filesystem, parent_name, size_bytes, true);
        }

        if (!is_partition || has_mount) continue;
        char device[LSM_PATH_LEN];
        const int written = snprintf(device, sizeof(device), "%s/%s",
                                     sources->dev_root, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(device)) continue;
        char size_path[LSM_PATH_LEN];
        uint64_t size_bytes = direct_block_size(sources, entry->d_name);
        if (!size_bytes &&
            child_path(size_path, sizeof(size_path), root, entry->d_name, "/size"))
            size_bytes = lsm_u64_multiply_saturating(
                lsm_read_u64_or_zero(size_path), 512U);
        char filesystem[64] = "";
        (void)filesystem_label_from_device_database(
            sources, entry->d_name, filesystem, sizeof(filesystem));
        count = append_partition_record(records, count, capacity, device, "",
                                        filesystem, parent_name, size_bytes, false);
    }

    closedir(directory);
    free(mounts);
    return count;
}

/* Network fallback paths are used only when rtnetlink is unavailable or a
 * synthetic fixture deliberately supplies sysfs-only data. */
static size_t list_networks_sysfs(LsmSystemSources *sources,
                                  LsmNetworkRecord *records,
                                  size_t capacity)
{
    if (!sources || !records || capacity == 0U) return 0U;
    char root[LSM_PATH_LEN];
    if (!source_path(root, sizeof(root), sources->sysfs_root, "/class/net")) return 0U;
    DIR *directory = opendir(root);
    if (!directory) return 0U;

    size_t count = 0U;
    struct dirent *entry = NULL;
    while (count < capacity && (entry = readdir(directory))) {
        if (entry->d_name[0] == '.' || strcmp(entry->d_name, "lo") == 0) continue;
        char path[LSM_PATH_LEN];
        char state[32] = "";
        if (!child_path(path, sizeof(path), root, entry->d_name, "/operstate") ||
            !lsm_read_text_file(path, state, sizeof(state)) ||
            strcmp(state, "up") != 0)
            continue;

        LsmNetworkRecord *record = &records[count++];
        memset(record, 0, sizeof(*record));
        lsm_copy_string(record->name, sizeof(record->name), entry->d_name);
        if (child_path(path, sizeof(path), root, entry->d_name, "/address"))
            (void)lsm_read_text_file(path, record->mac, sizeof(record->mac));
        record->wireless = wireless_interface(record->name);

        char class_device[LSM_PATH_LEN];
        char hardware[LSM_PATH_LEN] = "";
        if (child_path(class_device, sizeof(class_device), root, entry->d_name, "/device"))
            (void)hardware_parent(class_device, hardware, sizeof(hardware));
        native_device_identity(hardware, record->product, sizeof(record->product),
                               record->vendor, sizeof(record->vendor));
    }
    closedir(directory);
    return count;
}


static size_t network_counters_sysfs(LsmSystemSources *sources,
                                      LsmNetworkCounterRecord *records,
                                      size_t capacity)
{
    if (!sources || !records || capacity == 0U) return 0U;
    char root[LSM_PATH_LEN];
    if (!source_path(root, sizeof(root), sources->sysfs_root, "/class/net")) return 0U;
    DIR *directory = opendir(root);
    if (!directory) return 0U;
    size_t count = 0U;
    struct dirent *entry = NULL;
    while (count < capacity && (entry = readdir(directory))) {
        if (entry->d_name[0] == '.' || strcmp(entry->d_name, "lo") == 0) continue;
        char path[LSM_PATH_LEN];
        char state[32] = "";
        if (!child_path(path, sizeof(path), root, entry->d_name, "/operstate") ||
            !lsm_read_text_file(path, state, sizeof(state)) || strcmp(state, "up") != 0)
            continue;
        lsm_copy_string(records[count].name, sizeof(records[count].name), entry->d_name);
        if (child_path(path, sizeof(path), root, entry->d_name, "/statistics/rx_bytes"))
            records[count].rx_bytes = lsm_read_u64_or_zero(path);
        if (child_path(path, sizeof(path), root, entry->d_name, "/statistics/tx_bytes"))
            records[count].tx_bytes = lsm_read_u64_or_zero(path);
        count++;
    }
    closedir(directory);
    return count;
}

size_t lsm_sources_list_networks(LsmSystemSources *sources,
                                 LsmNetworkRecord *records,
                                 size_t capacity)
{
    if (!sources || !records || capacity == 0U) return 0U;
    if (sources->network_request_fd < 0)
        return list_networks_sysfs(sources, records, capacity);

    LsmRouteLink links[LSM_MAX_NETS];
    memset(links, 0, sizeof(links));
    const size_t link_count = route_link_dump(sources, links, LSM_MAX_NETS);
    if (link_count == 0U) return list_networks_sysfs(sources, records, capacity);

    size_t count = 0U;
    char root[LSM_PATH_LEN];
    if (!source_path(root, sizeof(root), sources->sysfs_root, "/class/net"))
        return 0U;
    for (size_t index = 0U; index < link_count && count < capacity; index++) {
        if (links[index].operstate != IF_OPER_UP &&
            (links[index].flags & IFF_RUNNING) == 0U)
            continue;
        LsmNetworkRecord *record = &records[count++];
        memset(record, 0, sizeof(*record));
        lsm_copy_string(record->name, sizeof(record->name), links[index].name);
        lsm_copy_string(record->mac, sizeof(record->mac), links[index].mac);
        record->wireless = wireless_interface(record->name);

        char class_device[LSM_PATH_LEN];
        char hardware[LSM_PATH_LEN] = "";
        if (child_path(class_device, sizeof(class_device), root,
                       record->name, "/device"))
            (void)hardware_parent(class_device, hardware, sizeof(hardware));
        native_device_identity(hardware, record->product, sizeof(record->product),
                               record->vendor, sizeof(record->vendor));
    }
    return count;
}

size_t lsm_sources_read_network_counters(LsmSystemSources *sources,
                                         LsmNetworkCounterRecord *records,
                                         size_t capacity)
{
    if (!sources || !records || capacity == 0U) return 0U;
    LsmRouteLink links[LSM_MAX_NETS];
    memset(links, 0, sizeof(links));
    const size_t link_count = route_link_dump(sources, links, LSM_MAX_NETS);
    if (link_count == 0U) return network_counters_sysfs(sources, records, capacity);
    size_t count = 0U;
    for (size_t index = 0U; index < link_count && count < capacity; index++) {
        if (links[index].operstate != IF_OPER_UP &&
            (links[index].flags & IFF_RUNNING) == 0U)
            continue;
        lsm_copy_string(records[count].name, sizeof(records[count].name),
                        links[index].name);
        records[count].rx_bytes = links[index].rx_bytes;
        records[count].tx_bytes = links[index].tx_bytes;
        count++;
    }
    return count;
}

bool lsm_sources_network_topology_changed(LsmSystemSources *sources)
{
    if (!sources || sources->network_event_fd < 0 ||
        sources->network_request_fd < 0) return true;
    bool changed = false;
    for (;;) {
        unsigned char buffer[8192];
        const ssize_t received = recv(sources->network_event_fd, buffer, sizeof(buffer), 0);
        if (received < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            return true;
        }
        if (received == 0) break;
        int remaining = (int)received;
        for (struct nlmsghdr *header = (struct nlmsghdr *)(void *)buffer;
             netlink_message_ok(header, remaining);
             header = NLMSG_NEXT(header, remaining)) {
            switch (header->nlmsg_type) {
            case RTM_NEWLINK:
            case RTM_DELLINK:
            case RTM_NEWADDR:
            case RTM_DELADDR:
                changed = true;
                break;
            default:
                break;
            }
        }
    }
    return changed;
}

/* DRM inventory exposes stable kernel identities without vendor utilities. */
size_t lsm_sources_list_gpus(LsmSystemSources *sources,
                             LsmGpuRecord *records,
                             size_t capacity)
{
    if (!sources || !records || capacity == 0U) return 0U;
    char root[LSM_PATH_LEN];
    if (!source_path(root, sizeof(root), sources->sysfs_root, "/class/drm")) return 0U;
    DIR *directory = opendir(root);
    if (!directory) return 0U;

    size_t count = 0U;
    struct dirent *entry = NULL;
    while (count < capacity && (entry = readdir(directory))) {
        const char *card = entry->d_name;
        if (strncmp(card, "card", 4) != 0 || !isdigit((unsigned char)card[4]) ||
            strchr(card, '-'))
            continue;

        char class_device[LSM_PATH_LEN];
        char hardware[LSM_PATH_LEN] = "";
        if (!child_path(class_device, sizeof(class_device), root, card, "/device") ||
            !hardware_parent(class_device, hardware, sizeof(hardware)))
            continue;

        LsmGpuRecord *record = &records[count++];
        memset(record, 0, sizeof(*record));
        lsm_copy_string(record->card, sizeof(record->card), card);
        lsm_copy_string(record->device_syspath, sizeof(record->device_syspath), hardware);
        native_device_identity(hardware, record->product, sizeof(record->product),
                               record->vendor, sizeof(record->vendor));

        char driver_path[LSM_PATH_LEN];
        if (lsm_join_path(driver_path, sizeof(driver_path), hardware, "/driver"))
            (void)read_symlink_basename(driver_path, record->driver,
                                        sizeof(record->driver));
        if (!record->driver[0]) lsm_copy_string(record->driver, sizeof(record->driver), "unknown");
    }
    closedir(directory);
    return count;
}

/* Temperature selection scores labels and driver names to avoid reporting an
 * unrelated motherboard sensor as the CPU package temperature. */
static bool cpu_sensor_name(const char *name)
{
    if (!name) return false;
    return strcasestr(name, "coretemp") || strcasestr(name, "k10temp") ||
           strcasestr(name, "zenpower") || strcasestr(name, "cpu") ||
           strcasestr(name, "soc_thermal") || strcasestr(name, "x86_pkg_temp") ||
           strcasestr(name, "acpitz");
}

static int temperature_label_score(const char *label)
{
    if (!label || !*label) return 0;
    if (strcasestr(label, "package") || strcasestr(label, "tdie") ||
        strcasestr(label, "tctl") || strcasestr(label, "cpu") ||
        strcasestr(label, "composite"))
        return 100;
    if (strcasestr(label, "core")) return 30;
    return 10;
}

static bool valid_temperature(double celsius)
{
    return isfinite(celsius) && celsius >= -50.0 && celsius <= 200.0;
}

static double hwmon_cpu_temperature(const LsmSystemSources *sources)
{
    char root[LSM_PATH_LEN];
    if (!source_path(root, sizeof(root), sources->sysfs_root, "/class/hwmon")) return NAN;
    DIR *directory = opendir(root);
    if (!directory) return NAN;

    double best = NAN;
    int best_score = -1;
    struct dirent *entry = NULL;
    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') continue;
        char path[LSM_PATH_LEN];
        char chip[128] = "";
        if (!child_path(path, sizeof(path), root, entry->d_name, "/name")) continue;
        (void)lsm_read_text_file(path, chip, sizeof(chip));
        if (!cpu_sensor_name(chip)) continue;

        for (unsigned index = 1U; index <= 64U; index++) {
            char suffix[64];
            const int input_written = snprintf(suffix, sizeof(suffix), "/temp%u_input", index);
            if (input_written < 0 || (size_t)input_written >= sizeof(suffix) ||
                !child_path(path, sizeof(path), root, entry->d_name, suffix))
                continue;
            double milli = NAN;
            if (!lsm_read_double_file(path, &milli)) continue;
            const double celsius = milli / 1000.0;
            if (!valid_temperature(celsius)) continue;

            char label[128] = "";
            const int label_written = snprintf(suffix, sizeof(suffix), "/temp%u_label", index);
            if (label_written >= 0 && (size_t)label_written < sizeof(suffix) &&
                child_path(path, sizeof(path), root, entry->d_name, suffix))
                (void)lsm_read_text_file(path, label, sizeof(label));
            const int score = temperature_label_score(label);
            if (score > best_score || (score == best_score &&
                                       (!isfinite(best) || celsius > best))) {
                best = celsius;
                best_score = score;
            }
        }
    }
    closedir(directory);
    return best;
}

static double thermal_cpu_temperature(const LsmSystemSources *sources)
{
    char root[LSM_PATH_LEN];
    if (!source_path(root, sizeof(root), sources->sysfs_root, "/class/thermal")) return NAN;
    DIR *directory = opendir(root);
    if (!directory) return NAN;

    double best = NAN;
    struct dirent *entry = NULL;
    while ((entry = readdir(directory))) {
        if (strncmp(entry->d_name, "thermal_zone", 12) != 0) continue;
        char path[LSM_PATH_LEN];
        char type[128] = "";
        if (!child_path(path, sizeof(path), root, entry->d_name, "/type") ||
            !lsm_read_text_file(path, type, sizeof(type)) || !cpu_sensor_name(type))
            continue;
        if (!child_path(path, sizeof(path), root, entry->d_name, "/temp")) continue;
        double milli = NAN;
        if (!lsm_read_double_file(path, &milli)) continue;
        const double celsius = milli / 1000.0;
        if (valid_temperature(celsius) && (!isfinite(best) || celsius > best))
            best = celsius;
    }
    closedir(directory);
    return best;
}

double lsm_sources_read_cpu_temperature(LsmSystemSources *sources)
{
    if (!sources) return NAN;
    const double hwmon = hwmon_cpu_temperature(sources);
    return isfinite(hwmon) ? hwmon : thermal_cpu_temperature(sources);
}
