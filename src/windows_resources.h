// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file windows_resources.h
 * @brief Stable Win32 resource identifiers used by the native executable.
 *
 * Font resource IDs are independent of filenames. Build tooling obtains the
 * canonical verified faces through Common's typography metadata, while runtime
 * code refers only to these stable resource identities.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_WINDOWS_RESOURCES_H
#define INFILTRATOR_SYSTEM_MONITOR_WINDOWS_RESOURCES_H

#define LSM_WINDOWS_FONT_UI_REGULAR 301
#define LSM_WINDOWS_FONT_UI_BOLD 302
#define LSM_WINDOWS_FONT_BRAND_REGULAR 303
#define LSM_WINDOWS_FONT_RESOURCE_COUNT 3U

#endif
