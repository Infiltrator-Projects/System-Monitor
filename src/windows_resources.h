// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_resources.h
 * @brief Stable Win32 resource identifiers and native UI capacities.
 *
 * Font resource IDs are independent of filenames. Build tooling obtains the
 * canonical verified faces through Common's typography metadata, while runtime
 * code refers only to these stable resource identities.
 *
 * The Performance hit-test inventory is sized from every resource class in the
 * shared monitor model rather than from whichever classes the current Win32
 * renderer happens to draw today. This prevents future Bluetooth, battery or
 * NPU presentation from silently outgrowing a separately maintained capacity.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_WINDOWS_RESOURCES_H
#define INFILTRATOR_SYSTEM_MONITOR_WINDOWS_RESOURCES_H

#include "monitor_types.h"

#define LSM_WINDOWS_FONT_UI_REGULAR 301
#define LSM_WINDOWS_FONT_UI_BOLD 302
#define LSM_WINDOWS_FONT_BRAND_REGULAR 303
#define LSM_WINDOWS_FONT_RESOURCE_COUNT 3U

#define LSM_WINDOWS_PERFORMANCE_ITEM_CAPACITY \
    (2U + LSM_MAX_DISKS + LSM_MAX_NETS + LSM_MAX_BLUETOOTH_DEVICES + \
     LSM_MAX_GPUS + LSM_MAX_BATTERIES + LSM_MAX_NPUS)

/* main_windows.c historically carried a narrower local definition. Keep this
 * header authoritative even while that translation unit includes us after its
 * legacy definition; the next structural split can remove the local macro. */
#ifdef LSM_WINDOWS_MAX_PERFORMANCE_ITEMS
#undef LSM_WINDOWS_MAX_PERFORMANCE_ITEMS
#endif
#define LSM_WINDOWS_MAX_PERFORMANCE_ITEMS \
    LSM_WINDOWS_PERFORMANCE_ITEM_CAPACITY

#endif
