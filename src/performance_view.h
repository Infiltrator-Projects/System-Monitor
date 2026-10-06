// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_view.h
 * @brief Platform-neutral formatted Performance view models.
 *
 * Native collectors publish plain monitor data. This layer converts that data
 * into one canonical user-facing representation before GTK or Win32 renders it,
 * preventing operating-system front ends from independently deciding whether a
 * value is available or how it is formatted.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PERFORMANCE_VIEW_H
#define INFILTRATOR_SYSTEM_MONITOR_PERFORMANCE_VIEW_H

#include "monitor_types.h"
#include "presentation_contract.h"

#define LSM_PERFORMANCE_VIEW_VALUE_LEN 128
#define LSM_PERFORMANCE_VIEW_RAIL_LEN 512
#define LSM_MEMORY_MODULE_VIEW_LEN 8192
#define LSM_DEVICE_PERFORMANCE_METRIC_COUNT 12
#define LSM_DEVICE_PERFORMANCE_LABEL_LEN 64
#define LSM_SUMMARY_VIEW_VALUE_LEN 128

/** Canonical formatted cross-tab summary consumed by every native renderer. */
typedef struct {
    char values[LSM_SUMMARY_COUNT][LSM_SUMMARY_VIEW_VALUE_LEN];
} LsmSummaryPerformanceView;

/** Canonical formatted CPU presentation consumed by every native renderer. */
typedef struct {
    char subtitle[LSM_NAME_LEN];
    char rail_value[LSM_PERFORMANCE_VIEW_RAIL_LEN];
    char metrics[LSM_CPU_METRIC_COUNT][LSM_PERFORMANCE_VIEW_VALUE_LEN];
    char details[LSM_CPU_DETAIL_COUNT][LSM_PERFORMANCE_VIEW_VALUE_LEN];
} LsmCpuPerformanceView;

/** Canonical formatted memory presentation consumed by every native renderer. */
typedef struct {
    char subtitle[LSM_PERFORMANCE_VIEW_VALUE_LEN];
    char rail_value[LSM_PERFORMANCE_VIEW_RAIL_LEN];
    char metrics[LSM_MEMORY_METRIC_COUNT][LSM_PERFORMANCE_VIEW_VALUE_LEN];
    char details[LSM_MEMORY_DETAIL_COUNT][LSM_MEMORY_MODULE_VIEW_LEN];
} LsmMemoryPerformanceView;

/**
 * Canonical formatted device presentation used by native renderers for
 * storage, network and graphics resources.
 *
 * Labels live here with their values so native renderers do not create a
 * second operating-system-specific interpretation of the same telemetry.
 */
typedef struct {
    char title[LSM_PERFORMANCE_VIEW_VALUE_LEN];
    char subtitle[LSM_PERFORMANCE_VIEW_RAIL_LEN];
    char rail_value[LSM_PERFORMANCE_VIEW_RAIL_LEN];
    char metric_labels[LSM_DEVICE_PERFORMANCE_METRIC_COUNT]
                      [LSM_DEVICE_PERFORMANCE_LABEL_LEN];
    char metric_values[LSM_DEVICE_PERFORMANCE_METRIC_COUNT]
                      [LSM_PERFORMANCE_VIEW_VALUE_LEN];
    size_t metric_count;
} LsmDevicePerformanceView;

/** Stable indices for values produced by lsm_disk_performance_view(). */
typedef enum {
    LSM_DISK_VIEW_READ_SPEED = 0,
    LSM_DISK_VIEW_WRITE_SPEED,
    LSM_DISK_VIEW_ACTIVE_TIME,
    LSM_DISK_VIEW_AVERAGE_RESPONSE,
    LSM_DISK_VIEW_QUEUE_LENGTH,
    LSM_DISK_VIEW_CAPACITY,
    LSM_DISK_VIEW_MEDIA_TYPE,
    LSM_DISK_VIEW_CONNECTION,
    LSM_DISK_VIEW_SYSTEM_DISK
} LsmDiskPerformanceMetric;

/** Stable indices for values produced by lsm_network_performance_view(). */
typedef enum {
    LSM_NETWORK_VIEW_RECEIVE = 0,
    LSM_NETWORK_VIEW_SEND,
    LSM_NETWORK_VIEW_LINK_SPEED,
    LSM_NETWORK_VIEW_UTILISATION,
    LSM_NETWORK_VIEW_IPV4,
    LSM_NETWORK_VIEW_IPV6,
    LSM_NETWORK_VIEW_MAC,
    LSM_NETWORK_VIEW_STATE,
    LSM_NETWORK_VIEW_ADAPTER
} LsmNetworkPerformanceMetric;

/** Stable indices for values produced by lsm_gpu_performance_view(). */
typedef enum {
    LSM_GPU_VIEW_PRODUCT = 0,
    LSM_GPU_VIEW_UTILISATION,
    LSM_GPU_VIEW_TEMPERATURE,
    LSM_GPU_VIEW_MEMORY,
    LSM_GPU_VIEW_DRIVER,
    LSM_GPU_VIEW_DRIVER_VERSION,
    LSM_GPU_VIEW_ACTIVE_ENGINE,
    LSM_GPU_VIEW_TELEMETRY
} LsmGpuPerformanceMetric;

/**
 * Project one monitor snapshot into the canonical cross-tab summary.
 *
 * @param monitor Current platform-neutral monitoring snapshot, or NULL.
 * @param network_use_bits Present network throughput in bits when true.
 * @param view Caller-owned output fully initialised by the function.
 */
void lsm_summary_performance_view(const LsmMonitor *monitor,
                                  bool network_use_bits,
                                  LsmSummaryPerformanceView *view);

/**
 * Project one monitor snapshot into the canonical CPU presentation.
 *
 * @param monitor Current platform-neutral monitoring snapshot.
 * @param view Caller-owned output that is fully initialised by the function.
 */
void lsm_cpu_performance_view(const LsmMonitor *monitor,
                              LsmCpuPerformanceView *view);

/**
 * Project one monitor snapshot into the canonical memory presentation.
 *
 * @param monitor Current platform-neutral monitoring snapshot.
 * @param view Caller-owned output that is fully initialised by the function.
 */
void lsm_memory_performance_view(const LsmMonitor *monitor,
                                 LsmMemoryPerformanceView *view);

/**
 * Project one disk snapshot into the canonical device presentation.
 *
 * @param disk Disk snapshot, or NULL when no disk is available.
 * @param index Zero-based presentation index.
 * @param view Caller-owned output fully initialised by the function.
 */
void lsm_disk_performance_view(const LsmDiskInfo *disk, size_t index,
                               LsmDevicePerformanceView *view);

/**
 * Project one network snapshot into the canonical device presentation.
 *
 * @param net Network snapshot, or NULL when no interface is available.
 * @param index Zero-based presentation index.
 * @param use_bits Present throughput in bits when true, bytes when false.
 * @param view Caller-owned output fully initialised by the function.
 */
void lsm_network_performance_view(const LsmNetInfo *net, size_t index,
                                  bool use_bits,
                                  LsmDevicePerformanceView *view);

/**
 * Project one graphics snapshot into the canonical device presentation.
 *
 * @param gpu Graphics snapshot, or NULL when no adapter is available.
 * @param index Zero-based presentation index.
 * @param view Caller-owned output fully initialised by the function.
 */
void lsm_gpu_performance_view(const LsmGpuInfo *gpu, size_t index,
                              LsmDevicePerformanceView *view);

#endif
