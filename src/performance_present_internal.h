// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_present_internal.h
 * @brief Private boundaries between Performance snapshot-presentation modules.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PERFORMANCE_PRESENT_INTERNAL_H
#define INFILTRATOR_SYSTEM_MONITOR_PERFORMANCE_PRESENT_INTERNAL_H

#include "app_internal.h"

#include <stdbool.h>

/** Present CPU, memory, disk or network state; return true when handled. */
bool performance_present_core_page(LsmApp *app, LsmDevicePage *page);

/** Present Bluetooth, GPU, battery or NPU state. */
void performance_present_device_page(LsmApp *app, LsmDevicePage *page);

/** Choose product, then vendor, while rejecting placeholder/bus-only names. */
const char *performance_present_preferred_hardware_name(const char *product,
                                                        const char *vendor);

/** Apply the shared numbered-device title policy to a large heading label. */
void performance_present_set_large_device_title(GtkWidget *label,
                                                const char *kind,
                                                size_t index,
                                                const char *hardware_name);

/** Apply the shared warning/fault presentation for a temperature metric. */
void performance_present_set_temperature_state(GtkWidget *widget,
                                               bool available,
                                               double celsius,
                                               double warning_threshold,
                                               double fault_threshold);

#endif
