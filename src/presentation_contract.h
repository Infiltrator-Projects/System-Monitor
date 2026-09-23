// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file presentation_contract.h
 * @brief Platform-neutral System Monitor presentation contract.
 *
 * Product hierarchy, Performance resource identity, canonical layout metrics
 * and field ordering live here so native front ends render one application
 * contract rather than maintaining independent Linux and Windows copies.
 *
 * This header deliberately contains no GTK, Win32 or operating-system types.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PRESENTATION_CONTRACT_H
#define INFILTRATOR_SYSTEM_MONITOR_PRESENTATION_CONTRACT_H

#include "app_config.h"

#include <stddef.h>
#include <stdint.h>

/** Performance resource/page identity shared by every native front end. */
typedef enum {
    LSM_PAGE_CPU = 0,
    LSM_PAGE_MEMORY,
    LSM_PAGE_DISK,
    LSM_PAGE_NETWORK,
    LSM_PAGE_BLUETOOTH,
    LSM_PAGE_GPU,
    LSM_PAGE_BATTERY,
    LSM_PAGE_NPU,
    LSM_PAGE_COUNT
} LsmPageType;

/** Canonical Performance geometry. Native toolkits map these logical pixels. */
enum {
    LSM_PRIMARY_GRAPH_MIN_HEIGHT = 120,
    LSM_SIDEBAR_WIDTH = 220,
    LSM_SIDE_BUTTON_WIDTH = 212,
    LSM_SIDE_BUTTON_HEIGHT = 68,
    LSM_SIDE_GRAPH_WIDTH = 64,
    LSM_SIDE_GRAPH_HEIGHT = 44,
    LSM_MEMORY_COMPOSITION_HEIGHT = 70
};

/** Canonical resource colours used by Linux graphs and native Windows drawing. */
#define LSM_COLOUR_CPU       "#00adef"
#define LSM_COLOUR_MEMORY    "#5c9efa"
#define LSM_COLOUR_DISK      "#638d1e"
#define LSM_COLOUR_NETWORK   "#f5628e"
#define LSM_COLOUR_BLUETOOTH "#4b9cff"
#define LSM_COLOUR_GPU       "#de68f2"
#define LSM_COLOUR_GPU_AUX   "#f0a0fa"
#define LSM_COLOUR_BATTERY   "#d19e47"
#define LSM_COLOUR_NPU       "#45b7a8"

/** Toolkit-neutral RGB triplet. */
typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} LsmPresentationColour;

/** Toolkit-neutral row/column placement for a field in a canonical grid. */
typedef struct {
    uint8_t column;
    uint8_t row;
} LsmPresentationGridPosition;

/** CPU headline metric order shared by all renderers. */
typedef enum {
    LSM_CPU_METRIC_UTILISATION = 0,
    LSM_CPU_METRIC_SPEED,
    LSM_CPU_METRIC_PROCESSES,
    LSM_CPU_METRIC_THREADS,
    LSM_CPU_METRIC_HANDLES,
    LSM_CPU_METRIC_UPTIME,
    LSM_CPU_METRIC_TEMPERATURE,
    LSM_CPU_METRIC_PRESSURE,
    LSM_CPU_METRIC_USER,
    LSM_CPU_METRIC_KERNEL,
    LSM_CPU_METRIC_COUNT
} LsmCpuMetricField;

/** CPU detail-field order shared by all renderers. */
typedef enum {
    LSM_CPU_DETAIL_CORES = 0,
    LSM_CPU_DETAIL_LOGICAL_PROCESSORS,
    LSM_CPU_DETAIL_BASE_SPEED,
    LSM_CPU_DETAIL_MAXIMUM_SPEED,
    LSM_CPU_DETAIL_VIRTUALISATION,
    LSM_CPU_DETAIL_CACHE_L1,
    LSM_CPU_DETAIL_CACHE_L2,
    LSM_CPU_DETAIL_CACHE_L3,
    LSM_CPU_DETAIL_LOAD_AVERAGE,
    LSM_CPU_DETAIL_SOCKETS,
    LSM_CPU_DETAIL_NUMA_NODES,
    LSM_CPU_DETAIL_INTERRUPTS,
    LSM_CPU_DETAIL_CONTEXT_SWITCHES,
    LSM_CPU_DETAIL_COUNT
} LsmCpuDetailField;

/** Memory headline metric order shared by all renderers. */
typedef enum {
    LSM_MEMORY_METRIC_IN_USE = 0,
    LSM_MEMORY_METRIC_AVAILABLE,
    LSM_MEMORY_METRIC_COMMITTED,
    LSM_MEMORY_METRIC_CACHED,
    LSM_MEMORY_METRIC_BUFFERS,
    LSM_MEMORY_METRIC_SWAP,
    LSM_MEMORY_METRIC_KERNEL_RECLAIMABLE,
    LSM_MEMORY_METRIC_KERNEL_NONRECLAIMABLE,
    LSM_MEMORY_METRIC_PAGE_TABLES,
    LSM_MEMORY_METRIC_PRESSURE,
    LSM_MEMORY_METRIC_COUNT
} LsmMemoryMetricField;

/** Memory hardware-detail order shared by all renderers. */
typedef enum {
    LSM_MEMORY_DETAIL_SPEED = 0,
    LSM_MEMORY_DETAIL_SLOTS_USED,
    LSM_MEMORY_DETAIL_FORM_FACTOR,
    LSM_MEMORY_DETAIL_HARDWARE_CORRUPTED,
    LSM_MEMORY_DETAIL_INSTALLED_MODULES,
    LSM_MEMORY_DETAIL_COUNT
} LsmMemoryDetailField;

/** Return the canonical label for one top-level application tab. */
const char *lsm_tab_label(LsmTabIndex tab);

/** Return the canonical default title for one Performance resource type. */
const char *lsm_performance_page_title(LsmPageType type);

/** Return the canonical stack-name prefix for one Performance resource type. */
const char *lsm_performance_stack_prefix(LsmPageType type);

/** Return the canonical hexadecimal graph/accent colour for a resource type. */
const char *lsm_performance_colour_hex(LsmPageType type);

/** Return the canonical RGB graph/accent colour for a resource type. */
LsmPresentationColour lsm_performance_colour_rgb(LsmPageType type);

/** Return one canonical CPU metric caption. */
const char *lsm_cpu_metric_label(LsmCpuMetricField field);

/** Return one canonical CPU detail caption. */
const char *lsm_cpu_detail_label(LsmCpuDetailField field);

/** Return one canonical memory metric caption. */
const char *lsm_memory_metric_label(LsmMemoryMetricField field);

/** Return one canonical memory detail caption. */
const char *lsm_memory_detail_label(LsmMemoryDetailField field);

/** Return the canonical two-column placement for one CPU headline metric. */
LsmPresentationGridPosition
lsm_cpu_metric_position(LsmCpuMetricField field);

/** Return the canonical two-group placement for one CPU detail field. */
LsmPresentationGridPosition
lsm_cpu_detail_position(LsmCpuDetailField field);

/** Return the canonical two-column placement for one memory headline metric. */
LsmPresentationGridPosition
lsm_memory_metric_position(LsmMemoryMetricField field);

/** Return the canonical single-column placement for one memory detail field. */
LsmPresentationGridPosition
lsm_memory_detail_position(LsmMemoryDetailField field);

/** Return the shared CPU primary-graph caption. */
const char *lsm_cpu_graph_caption(void);

/** Return the shared memory primary-graph caption. */
const char *lsm_memory_graph_caption(void);

/** Return the shared memory-composition caption. */
const char *lsm_memory_composition_caption(void);

/** Return the shared percentage-axis maximum label. */
const char *lsm_percent_scale_max_label(void);

#endif
