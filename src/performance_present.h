// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_present.h
 * @brief Snapshot-to-widget presentation for Performance device pages.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PERFORMANCE_PRESENT_H
#define LINUX_SYSTEM_MONITOR_PERFORMANCE_PRESENT_H

#include "app_internal.h"

/**
 * Project one immutable monitor snapshot onto an existing device page.
 *
 * Presentation never performs hardware I/O. Availability flags determine
 * whether a numeric zero is displayed as a valid sample or as unavailable.
 *
 * @param [in,out] app Application containing the current retained snapshot.
 * @param [in,out] page Page whose labels, graphs and details are updated.
 */
void lsm_performance_present_page(LsmApp *app, LsmDevicePage *page);

#endif
