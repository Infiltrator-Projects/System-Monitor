// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_types.h
 * @brief Plain data structures exchanged by monitor and presentation modules.
 *
 * This header contains no collection logic and no GTK types.  Keeping the data
 * model separate lets native collectors and tests share the same ABI without
 * inheriting unrelated backend functions.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_MONITOR_TYPES_H
#define LINUX_SYSTEM_MONITOR_MONITOR_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "process_model.h"

/** Maximum supported logical processors. */
#define LSM_MAX_CPUS 512
/** Maximum physical block devices displayed. */
#define LSM_MAX_DISKS 64
/** Maximum partitions shown for one physical block device. */
#define LSM_MAX_PARTITIONS 64
/** Maximum active network interfaces displayed. */
#define LSM_MAX_NETS 64
/** Maximum graphics adapters displayed. */
#define LSM_MAX_GPUS 16
/** Maximum batteries or UPS battery devices displayed. */
#define LSM_MAX_BATTERIES 8
/** Maximum compute accelerators/NPU devices displayed. */
#define LSM_MAX_NPUS 16
/** Maximum local Bluetooth controllers displayed. */
#define LSM_MAX_BLUETOOTH 4
/** Maximum populated SMBIOS memory modules retained for presentation. */
#define LSM_MAX_MEMORY_MODULES 32
/** Standard short-name buffer size. */
#define LSM_NAME_LEN 128
/** Standard filesystem-path buffer size. */
#define LSM_PATH_LEN 512
/** Standard opaque platform-identity buffer size. */
#define LSM_IDENTITY_LEN 512

/** Current CPU identity, topology, scheduler metrics and calculated values. */
typedef struct {
    char model[LSM_NAME_LEN];
    unsigned logical_cores;
    unsigned physical_cores;
    bool virtualization;
    char cache_l1[64];
    char cache_l2[64];
    char cache_l3[64];
    double usage_percent;
    double user_percent;
    double kernel_percent;
    double frequency_ghz;
    double base_frequency_ghz;
    double max_frequency_ghz;
    double temperature_c;
    unsigned process_count;
    unsigned thread_count;
    uint64_t uptime_seconds;          /**< Seconds elapsed since the current boot. */
    uint64_t file_handle_count;       /**< System-wide allocated file handles. */
    double load_average_1;
    double load_average_5;
    double load_average_15;
    double interrupts_per_sec;
    double context_switches_per_sec;
    uint64_t interrupt_count;
    uint64_t context_switch_count;
    unsigned socket_count;
    unsigned numa_node_count;
    double core_usage[LSM_MAX_CPUS];
} LsmCpuInfo;

/** One populated physical-memory module reported by SMBIOS Type 17. */
typedef struct {
    char locator[64];
    char bank_locator[64];
    char manufacturer[64];
    char part_number[96];
    char serial_number[64];
    char memory_type[32];
    char form_factor[32];
    uint64_t size_bytes;
    unsigned speed_mhz;
} LsmMemoryModuleInfo;

/** Current physical-memory and swap statistics supplied by the active backend. */
typedef struct {
    uint64_t total_bytes;              /**< Installed physical memory. */
    uint64_t available_bytes;          /**< Kernel estimate available without swapping. */
    uint64_t used_bytes;               /**< Total minus available, matching psutil. */
    uint64_t free_bytes;               /**< Completely unused physical memory. */
    uint64_t buffers_bytes;            /**< Filesystem block-device buffers. */
    uint64_t cached_bytes;             /**< Cached plus reclaimable slab, matching psutil. */
    uint64_t swap_total_bytes;         /**< Configured swap capacity. */
    uint64_t swap_used_bytes;          /**< Swap capacity currently occupied. */
    uint64_t committed_bytes;          /**< Virtual memory committed by the kernel. */
    uint64_t commit_limit_bytes;        /**< Current kernel commit limit. */
    uint64_t kernel_reclaimable_bytes;  /**< Reclaimable kernel slab memory. */
    uint64_t kernel_nonreclaimable_bytes; /**< Non-reclaimable kernel slab memory. */
    uint64_t page_tables_bytes;         /**< Physical memory occupied by page tables. */
    uint64_t hardware_corrupted_bytes; /**< Memory marked unusable by the kernel. */
    unsigned speed_mhz;                /**< Lowest reported populated-module speed. */
    unsigned slots_used;               /**< Populated SMBIOS memory-device slots. */
    unsigned slots_total;              /**< Total SMBIOS memory-device slots. */
    char form_factor[64];               /**< First populated module form factor. */
    LsmMemoryModuleInfo modules[LSM_MAX_MEMORY_MODULES];
    size_t module_count;
    bool module_details_available;
    double usage_percent;              /**< Used physical memory as a percentage. */
} LsmMemoryInfo;

/** One mounted filesystem belonging to a physical block device. */
typedef struct {
    char device[LSM_PATH_LEN];          /**< Canonical or user-visible device node. */
    char mount_point[LSM_PATH_LEN];     /**< Mounted directory. */
    char filesystem[64];                /**< Filesystem type supplied by the active backend. */
    uint64_t total_bytes;               /**< Filesystem or partition capacity. */
    uint64_t used_bytes;                /**< Filesystem space currently occupied. */
    unsigned used_percent;              /**< Used space rounded to a percentage. */
    bool usage_known;                   /**< True when statvfs supplied used-space data. */
} LsmPartitionInfo;

/** Current counters, identity and mounted filesystems for one physical block device. */
typedef struct {
    char name[64];
    char model[LSM_NAME_LEN];
    char media_type[16];
    char connection_type[32];
    uint64_t size_bytes;
    double read_bytes_per_sec;
    double write_bytes_per_sec;
    double active_percent;
    double average_response_ms;
    double read_response_ms;
    double write_response_ms;
    double queue_length;
    uint64_t read_bytes_total;
    uint64_t write_bytes_total;
    unsigned in_progress_operations;
    bool system_disk;
    LsmPartitionInfo partitions[LSM_MAX_PARTITIONS];
    size_t partition_count;
} LsmDiskInfo;

/** Current counters, addresses and identity for one network interface. */
typedef struct {
    char name[64];
    char ipv4[64];
    char ipv6[128];
    char mac[32];
    char product[LSM_NAME_LEN];      /**< Human-readable adapter model supplied by the active backend. */
    char vendor[LSM_NAME_LEN];       /**< Human-readable adapter vendor supplied by the active backend. */
    uint64_t rx_bytes_total;
    uint64_t tx_bytes_total;
    double rx_bytes_per_sec;
    double tx_bytes_per_sec;
    bool wireless;                  /**< Interface exposes wireless link information. */
    char ssid[LSM_NAME_LEN];        /**< Current wireless network name. */
    char access_point[32];          /**< Access-point MAC address when available. */
    double signal_percent;          /**< Link quality normalised to 0-100 percent. */
    double signal_dbm;              /**< Received signal in dBm when reported. */
    double frequency_mhz;           /**< Current centre frequency. */
    double link_speed_mbps;         /**< Current negotiated transmit bitrate. */
    double utilisation_percent;     /**< Combined send/receive rate versus link speed. */
    bool utilisation_available;     /**< Link rate is known, so utilisation is meaningful. */
    char connection_state[32];      /**< Backend-supplied connection state, normalised for display. */
} LsmNetInfo;

/** Current state of one local Bluetooth controller. */
typedef struct {
    char name[64];                    /**< Kernel/BlueZ controller name, such as hci0. */
    char address[32];                 /**< Local controller Bluetooth address. */
    char adapter_name[LSM_NAME_LEN];  /**< BlueZ adapter name. */
    char alias[LSM_NAME_LEN];         /**< User-visible BlueZ alias. */
    char connected_devices[512];      /**< Comma-separated connected device names. */
    unsigned device_count;            /**< Devices known to BlueZ for this adapter. */
    unsigned connected_count;         /**< Currently connected devices. */
    unsigned paired_count;            /**< Paired devices. */
    unsigned trusted_count;           /**< Trusted devices. */
    bool powered;
    bool discoverable;
    bool pairable;
    bool discovering;
} LsmBluetoothInfo;

/** Current identity and optional metrics for one graphics adapter. */
typedef struct {
    char name[LSM_NAME_LEN];
    char display_identifier[64]; /**< Short backend-supplied adapter identifier. */
    char driver[64];
    char driver_version[96];
    char pci_location[32];
    char active_engine[64];
    double active_engine_percent;
    char platform_identity[LSM_IDENTITY_LEN]; /**< Opaque stable identity supplied by the active backend. */
    double utilization_percent;
    double memory_percent;
    uint64_t memory_used_bytes;
    uint64_t memory_total_bytes;
    double temperature_c;
    double core_clock_mhz;
    double memory_clock_mhz;
    double memory_busy_percent;
    double encoder_percent;
    double decoder_percent;
    double render_percent;
    double compute_percent;
    double video_percent;
    double video_enhance_percent;
    double copy_percent;
    double power_watts;
    double fan_percent;
    bool supported_metrics;
    bool utilization_available; /**< A sampled zero is valid when this is true. */
    bool memory_busy_available;
    bool encoder_available;
    bool decoder_available;
    bool render_available;
    bool compute_available;
    bool video_available;
    bool video_enhance_available;
    bool copy_available;
    bool temperature_available;
    bool core_clock_available;
    bool memory_clock_available;
    bool power_available;
    bool fan_available;
    bool engine_metrics_capable; /**< Backend can expose independent GPU-engine telemetry. */
    bool shared_system_memory;   /**< Adapter uses dynamic system RAM, not VRAM. */
    bool integrated_cooling;     /**< Cooling is system-managed without a GPU fan. */
    char metrics_source[96];     /**< Human-readable backend/capability description. */
} LsmGpuInfo;

/** One system or peripheral battery snapshot supplied by the active backend. */
typedef struct {
    char name[64];
    char model[LSM_NAME_LEN];
    char manufacturer[LSM_NAME_LEN];
    char technology[64];
    char status[64];
    char health[64];
    char scope[32];
    char capacity_level[32];
    char serial[64];
    char connection[64];
    char battery_source[96];
    char device_type[64];
    char modalias[128];
    double capacity_percent;
    double supplemental_capacity_percent;
    double energy_now_wh;
    double energy_full_wh;
    double energy_design_wh;
    double power_watts;
    double voltage_volts;
    double current_amps;
    double temperature_c;
    uint64_t seconds_remaining;
    unsigned cycle_count;
    bool has_supplemental_capacity;
    bool on_ac_power;
    bool is_peripheral;
    bool paired;
    bool trusted;
    bool services_resolved;
    bool bluetooth_details_available;
    bool present;
} LsmBatteryInfo;

/** One compute accelerator/NPU snapshot supplied by the active platform backend. */
typedef struct {
    char name[LSM_NAME_LEN];
    char display_identifier[64]; /**< Short backend-supplied display identifier. */
    char driver[64];
    char platform_identity[LSM_IDENTITY_LEN]; /**< Opaque stable backend identity. */
    char device_identifier[LSM_PATH_LEN]; /**< Optional backend-supplied device identifier. */
    double utilization_percent;
    double memory_percent;
    uint64_t memory_used_bytes;
    uint64_t memory_total_bytes;
    double temperature_c;
    double clock_mhz;
    double power_watts;
    bool utilization_available;
    bool memory_used_available;
    bool memory_total_available;
    bool temperature_available;
    bool clock_available;
    bool power_available;
    bool supported_metrics;
    char metrics_source[96];
} LsmNpuInfo;

/** Complete monitoring snapshot retained by the application. */
typedef struct {
    LsmCpuInfo cpu;
    LsmMemoryInfo memory;
    LsmDiskInfo disks[LSM_MAX_DISKS];
    size_t disk_count;
    uint64_t disk_generation;           /**< Incremented whenever block-device membership changes. */
    uint64_t topology_generation;       /**< Incremented whenever any displayed device membership changes. */
    LsmNetInfo nets[LSM_MAX_NETS];
    size_t net_count;
    LsmBluetoothInfo bluetooth[LSM_MAX_BLUETOOTH];
    size_t bluetooth_count;
    LsmGpuInfo gpus[LSM_MAX_GPUS];
    size_t gpu_count;
    LsmBatteryInfo batteries[LSM_MAX_BATTERIES];
    size_t battery_count;
    LsmNpuInfo npus[LSM_MAX_NPUS];
    size_t npu_count;
    void *backend_state; /**< Opaque platform collector state; presentation must not inspect it. */
} LsmMonitor;



#endif
