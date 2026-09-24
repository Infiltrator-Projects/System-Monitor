// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_present_internal.h
 * @brief Private boundaries between Performance snapshot-presentation modules.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PERFORMANCE_PRESENT_INTERNAL_H
#define INFILTRATOR_SYSTEM_MONITOR_PERFORMANCE_PRESENT_INTERNAL_H

#include "app_internal.h"

#include <stdbool.h>

/**
 * Present CPU, memory, disk or network state from the retained snapshot.
 *
 * @param [in,out] app Application owning the current monitor snapshot and widgets.
 * @param [in,out] page Performance page to refresh.
 * @return true when @p page belongs to the core-resource family.
 */
bool performance_present_core_page(LsmApp *app, LsmDevicePage *page);

/**
 * Present Bluetooth, GPU, battery or NPU state from the retained snapshot.
 *
 * @param [in,out] app Application owning the current monitor snapshot and widgets.
 * @param [in,out] page Device-oriented Performance page to refresh.
 */
void performance_present_device_page(LsmApp *app, LsmDevicePage *page);

/**
 * Choose the best user-facing hardware name without promoting bus identifiers.
 *
 * @param [in] product Product/model text, or NULL.
 * @param [in] vendor Vendor text, or NULL.
 * @return Product when useful, otherwise vendor when useful, otherwise "N/A".
 */
const char *performance_present_preferred_hardware_name(const char *product,
                                                        const char *vendor);

/**
 * Apply the shared numbered-device title policy to a large heading label.
 *
 * @param [in,out] label GTK label receiving escaped large-title markup.
 * @param [in] kind Device family label such as "GPU" or "Ethernet".
 * @param [in] index Stable presentation index shown to the user.
 * @param [in] hardware_name Optional friendly product/vendor name.
 */
void performance_present_set_large_device_title(GtkWidget *label,
                                                const char *kind,
                                                size_t index,
                                                const char *hardware_name);

/**
 * Apply shared warning/fault presentation to one temperature metric.
 *
 * @param [in,out] widget GTK widget whose semantic state classes are updated.
 * @param [in] available Whether @p celsius is a valid measurement.
 * @param [in] celsius Current temperature in degrees Celsius.
 * @param [in] warning_threshold Threshold for warning presentation.
 * @param [in] fault_threshold Threshold for fault presentation.
 */
void performance_present_set_temperature_state(GtkWidget *widget,
                                               bool available,
                                               double celsius,
                                               double warning_threshold,
                                               double fault_threshold);

#endif
