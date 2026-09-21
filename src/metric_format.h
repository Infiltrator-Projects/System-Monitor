// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file metric_format.h
 * @brief System Monitor names mapped to Common metric-formatting policy.
 *
 * Common owns binary memory/storage presentation, decimal network units,
 * optional scalar formatting and the canonical two-decimal GHz presentation.
 * System Monitor retains only readable aliases for those shared contracts.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_METRIC_FORMAT_H
#define LINUX_SYSTEM_MONITOR_METRIC_FORMAT_H

#include <infiltratr/format.h>

#include <stdbool.h>
#include <stddef.h>

#define lsm_metric_format_memory_gb infiltratr_format_memory_gb
#define lsm_metric_format_disk_capacity infiltratr_format_disk_capacity
#define lsm_metric_format_network infiltratr_format_network
#define lsm_metric_format_network_pair infiltratr_format_network_pair
#define lsm_metric_format_link_speed_mbps infiltratr_format_link_speed_mbps
#define lsm_metric_format_percent infiltratr_format_percent
#define lsm_metric_format_mhz infiltratr_format_mhz
#define lsm_metric_format_ghz infiltratr_format_ghz
#define lsm_metric_format_celsius infiltratr_format_celsius
#define lsm_metric_format_watts infiltratr_format_watts

#endif
