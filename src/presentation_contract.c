// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file presentation_contract.c
 * @brief Platform-neutral System Monitor presentation contract data.
 *
 * This module owns product-level presentation facts that must remain identical
 * across native renderers. It deliberately contains no GTK, Win32 or operating-
 * system types.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "presentation_contract.h"

static const char *const tab_labels[LSM_TAB_COUNT] = {
    "Performance",
    "Processes",
    "App History",
    "Startup Apps",
    "Users",
    "Details",
    "Services",
    "File Systems"
};

static const char *const page_titles[LSM_PAGE_COUNT] = {
    "CPU",
    "Memory",
    "Disk",
    "Network",
    "Bluetooth",
    "GPU",
    "Battery",
    "NPU"
};

static const char *const stack_prefixes[LSM_PAGE_COUNT] = {
    "cpu",
    "memory",
    "disk",
    "network",
    "bluetooth",
    "gpu",
    "battery",
    "npu"
};

static const char *const page_colours[LSM_PAGE_COUNT] = {
    LSM_COLOUR_CPU,
    LSM_COLOUR_MEMORY,
    LSM_COLOUR_DISK,
    LSM_COLOUR_NETWORK,
    LSM_COLOUR_BLUETOOTH,
    LSM_COLOUR_GPU,
    LSM_COLOUR_BATTERY,
    LSM_COLOUR_NPU
};

static const LsmPresentationColour page_rgb[LSM_PAGE_COUNT] = {
    {0x00U, 0xadU, 0xefU},
    {0x5cU, 0x9eU, 0xfaU},
    {0x63U, 0x8dU, 0x1eU},
    {0xf5U, 0x62U, 0x8eU},
    {0x4bU, 0x9cU, 0xffU},
    {0xdeU, 0x68U, 0xf2U},
    {0xd1U, 0x9eU, 0x47U},
    {0x45U, 0xb7U, 0xa8U}
};

static const char *const cpu_metric_labels[LSM_CPU_METRIC_COUNT] = {
    "Utilisation",
    "Speed",
    "Processes",
    "Threads",
    "Handles",
    "Uptime",
    "Temperature",
    "Pressure (10 s)",
    "User",
    "Kernel"
};

static const char *const cpu_detail_labels[LSM_CPU_DETAIL_COUNT] = {
    "Cores:",
    "Logical processors:",
    "Base speed:",
    "Maximum speed:",
    "Virtualisation:",
    "L1 cache:",
    "L2 cache:",
    "L3 cache:",
    "Load average:",
    "Sockets:",
    "NUMA nodes:",
    "Interrupts/s:",
    "Context switches/s:"
};

static const char *const memory_metric_labels[LSM_MEMORY_METRIC_COUNT] = {
    "In use",
    "Available",
    "Committed",
    "Cached",
    "Buffers",
    "Swap",
    "Kernel reclaimable",
    "Kernel non-reclaimable",
    "Page tables",
    "Pressure (10 s)"
};

static const char *const memory_detail_labels[LSM_MEMORY_DETAIL_COUNT] = {
    "Speed:",
    "Slots used:",
    "Form factor:",
    "Hardware corrupted:",
    "Installed modules:"
};

static size_t checked_index(int value, size_t count)
{
    return value >= 0 && (size_t)value < count ? (size_t)value : 0U;
}

const char *lsm_tab_label(LsmTabIndex tab)
{
    return tab_labels[checked_index((int)tab, LSM_TAB_COUNT)];
}

const char *lsm_performance_page_title(LsmPageType type)
{
    return page_titles[checked_index((int)type, LSM_PAGE_COUNT)];
}

const char *lsm_performance_stack_prefix(LsmPageType type)
{
    return stack_prefixes[checked_index((int)type, LSM_PAGE_COUNT)];
}

const char *lsm_performance_colour_hex(LsmPageType type)
{
    return page_colours[checked_index((int)type, LSM_PAGE_COUNT)];
}

LsmPresentationColour lsm_performance_colour_rgb(LsmPageType type)
{
    return page_rgb[checked_index((int)type, LSM_PAGE_COUNT)];
}

const char *lsm_cpu_metric_label(LsmCpuMetricField field)
{
    return cpu_metric_labels[checked_index((int)field, LSM_CPU_METRIC_COUNT)];
}

const char *lsm_cpu_detail_label(LsmCpuDetailField field)
{
    return cpu_detail_labels[checked_index((int)field, LSM_CPU_DETAIL_COUNT)];
}

const char *lsm_memory_metric_label(LsmMemoryMetricField field)
{
    return memory_metric_labels[
        checked_index((int)field, LSM_MEMORY_METRIC_COUNT)];
}

const char *lsm_memory_detail_label(LsmMemoryDetailField field)
{
    return memory_detail_labels[
        checked_index((int)field, LSM_MEMORY_DETAIL_COUNT)];
}

LsmPresentationGridPosition
lsm_cpu_metric_position(LsmCpuMetricField field)
{
    const size_t index =
        checked_index((int)field, LSM_CPU_METRIC_COUNT);
    const LsmPresentationGridPosition position = {
        (uint8_t)(index % 2U),
        (uint8_t)(index / 2U)
    };
    return position;
}

LsmPresentationGridPosition
lsm_cpu_detail_position(LsmCpuDetailField field)
{
    const size_t index =
        checked_index((int)field, LSM_CPU_DETAIL_COUNT);
    const LsmPresentationGridPosition position = {
        (uint8_t)(index / 7U),
        (uint8_t)(index % 7U)
    };
    return position;
}

LsmPresentationGridPosition
lsm_memory_metric_position(LsmMemoryMetricField field)
{
    const size_t index =
        checked_index((int)field, LSM_MEMORY_METRIC_COUNT);
    const LsmPresentationGridPosition position = {
        (uint8_t)(index % 2U),
        (uint8_t)(index / 2U)
    };
    return position;
}

LsmPresentationGridPosition
lsm_memory_detail_position(LsmMemoryDetailField field)
{
    const size_t index =
        checked_index((int)field, LSM_MEMORY_DETAIL_COUNT);
    const LsmPresentationGridPosition position = {
        0U,
        (uint8_t)index
    };
    return position;
}

const char *lsm_cpu_graph_caption(void)
{
    return "% Utilisation";
}

const char *lsm_memory_graph_caption(void)
{
    return "Memory usage";
}

const char *lsm_memory_composition_caption(void)
{
    return "Memory composition";
}

const char *lsm_percent_scale_max_label(void)
{
    return "100%";
}
