// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system_sources.h
 * @brief Native Linux hardware-discovery boundary.
 *
 * The collector exposes compact records built from direct device ioctls,
 * rtnetlink and bounded kernel-interface fallbacks. No external enumeration or
 * sensor-library type escapes into the application data model.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_SYSTEM_SOURCES_H
#define LINUX_SYSTEM_MONITOR_SYSTEM_SOURCES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "monitor_types.h"

/** Opaque native-source context. */
typedef struct LsmSystemSources LsmSystemSources;

/** One physical block device discovered through block-class sysfs. */
typedef struct {
    char name[64];             /**< Stable kernel name, such as nvme0n1. */
    char model[LSM_NAME_LEN];  /**< Human-readable vendor/model text. */
    char media_type[16];       /**< SSD or HDD when the queue reports it. */
    char connection_type[32];  /**< Native storage transport when identifiable. */
    uint64_t size_bytes;       /**< Raw device capacity. */
} LsmBlockDeviceRecord;

/** One mounted filesystem reported by the native mountinfo parser. */
typedef struct {
    char source[LSM_PATH_LEN];      /**< Mount source as reported by the kernel. */
    char target[LSM_PATH_LEN];      /**< Mount point. */
    char filesystem[64];            /**< Filesystem type. */
    char block_name[64];            /**< Kernel partition name when resolvable. */
    char parent_disk[64];           /**< Owning block disk name. */
} LsmMountRecord;

/** One partition, mounted or unmounted, belonging to a physical disk. */
typedef struct {
    char device[LSM_PATH_LEN];      /**< User-visible /dev node. */
    char mount_point[LSM_PATH_LEN]; /**< Empty when unmounted. */
    char filesystem[64];            /**< Filesystem/type identifier when known. */
    char parent_disk[64];           /**< Owning physical disk name. */
    uint64_t size_bytes;            /**< Partition capacity. */
    bool mounted;                   /**< True when a mount record matched. */
} LsmPartitionRecord;

/** One active non-loopback network interface. */
typedef struct {
    char name[64];                  /**< Kernel interface name. */
    char mac[32];                   /**< Link-layer address. */
    char product[LSM_NAME_LEN];     /**< Friendly adapter model. */
    char vendor[LSM_NAME_LEN];      /**< Friendly adapter vendor. */
    bool wireless;                  /**< Wireless driver accepted SIOCGIWNAME. */
} LsmNetworkRecord;

/** One rtnetlink interface-counter sample. */
typedef struct {
    char name[64];
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} LsmNetworkCounterRecord;

/** One DRM graphics adapter and its backing hardware identity. */
typedef struct {
    char card[64];                  /**< DRM card name, such as card0. */
    char product[LSM_NAME_LEN];     /**< Friendly adapter model. */
    char vendor[LSM_NAME_LEN];      /**< Friendly adapter vendor. */
    char driver[64];                /**< Bound kernel driver. */
    char device_syspath[LSM_PATH_LEN]; /**< Canonical backing-device path. */
} LsmGpuRecord;

/**
 * Create the native Linux source context and its rtnetlink sockets.
 *
 * @param [out] sources Receives the newly allocated context.
 * @return true on success; false leaves @p sources set to NULL.
 */
bool lsm_sources_init(LsmSystemSources **sources);
/**
 * Close source sockets and release the native-source context.
 *
 * @param [in,out] sources Context to release, or NULL.
 */
void lsm_sources_destroy(LsmSystemSources *sources);

/**
 * Enumerate physical non-loop block devices into a bounded array.
 *
 * @param [in] sources Native-source context.
 * @param [out] records Destination array.
 * @param [in] capacity Number of records available.
 * @return Number of records written, never greater than @p capacity.
 */
size_t lsm_sources_list_block_devices(LsmSystemSources *sources,
                                      LsmBlockDeviceRecord *records,
                                      size_t capacity);
/**
 * Enumerate current mount records through the internal mountinfo parser.
 *
 * @param [in] sources Native-source context.
 * @param [out] records Destination array.
 * @param [in] capacity Number of records available.
 * @return Number of records written.
 */
size_t lsm_sources_list_mounts(LsmSystemSources *sources,
                               LsmMountRecord *records,
                               size_t capacity);
/**
 * Enumerate mounted and unmounted child partitions for physical disks.
 *
 * @param [in] sources Native-source context.
 * @param [out] records Destination array.
 * @param [in] capacity Number of records available.
 * @return Number of records written.
 */
size_t lsm_sources_list_partitions(LsmSystemSources *sources,
                                   LsmPartitionRecord *records,
                                   size_t capacity);
/**
 * Enumerate active network interfaces and resolve hardware identity.
 *
 * @param [in] sources Native-source context.
 * @param [out] records Destination array.
 * @param [in] capacity Number of records available.
 * @return Number of records written.
 */
size_t lsm_sources_list_networks(LsmSystemSources *sources,
                                 LsmNetworkRecord *records,
                                 size_t capacity);
/**
 * Read one rtnetlink counter snapshot for active interfaces.
 *
 * @param [in] sources Native-source context.
 * @param [out] records Destination array.
 * @param [in] capacity Number of records available.
 * @return Number of counter records written.
 */
size_t lsm_sources_read_network_counters(LsmSystemSources *sources,
                                         LsmNetworkCounterRecord *records,
                                         size_t capacity);
/**
 * Drain queued link/address events and report whether topology changed.
 *
 * @param [in,out] sources Native-source context containing the event socket.
 * @return true when at least one relevant link or address event was observed.
 */
bool lsm_sources_network_topology_changed(LsmSystemSources *sources);

/**
 * Enumerate DRM graphics adapters and canonical backing-device identities.
 *
 * @param [in] sources Native-source context.
 * @param [out] records Destination array.
 * @param [in] capacity Number of records available.
 * @return Number of adapters written.
 */
size_t lsm_sources_list_gpus(LsmSystemSources *sources,
                             LsmGpuRecord *records,
                             size_t capacity);

/**
 * Read the best available CPU/package temperature through cached native paths.
 *
 * @param [in,out] sources Native-source context containing sensor discovery.
 * @return Temperature in degrees Celsius, or NAN when unavailable.
 */
double lsm_sources_read_cpu_temperature(LsmSystemSources *sources);

#endif
