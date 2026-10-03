// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_config.h
 * @brief Compile-time identity, tab indices and UI timing policy.
 *
 * Centralising these constants prevents modules from silently drifting to
 * different names, tab numbers or refresh intervals.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_APP_CONFIG_H
#define INFILTRATOR_SYSTEM_MONITOR_APP_CONFIG_H

#include "sampling_policy.h"

#define LSM_PROGRAM_NAME "System Monitor"
#define LSM_EXECUTABLE_NAME "system-monitor"
#define LSM_INSTALLED_EXECUTABLE_PATH "/usr/bin/system-monitor"
/* Stable compatibility key, not repository/package branding. Changing this
 * GApplication identity requires an explicit desktop-state migration plan. */
#define LSM_APPLICATION_ID "io.github.theinfiltratr.SystemMonitor"
#define LSM_CONFIG_DIRECTORY "system-monitor"
#define LSM_LOG_DIRECTORY "System-Monitor-logs"
#define LSM_DEFAULT_WINDOW_WIDTH 1280
#define LSM_DEFAULT_WINDOW_HEIGHT 800
#define LSM_COMPACT_LAYOUT_THRESHOLD 1100
#define LSM_COMPACT_LAYOUT_HYSTERESIS 64
#define LSM_MAIN_NAV_WIDTH 214
#define LSM_MAIN_NAV_COMPACT_WIDTH 64
#define LSM_MAIN_NAV_BUTTON_WIDTH 196
#define LSM_MAIN_NAV_BUTTON_COMPACT_WIDTH 48
#define LSM_PERFORMANCE_SIDEBAR_COMPACT_WIDTH 164
#define LSM_PERFORMANCE_SIDE_BUTTON_COMPACT_WIDTH 156

typedef enum {
    LSM_TAB_PERFORMANCE = 0,
    LSM_TAB_PROCESSES,
    LSM_TAB_APP_HISTORY,
    LSM_TAB_STARTUP,
    LSM_TAB_USERS,
    LSM_TAB_DETAILS,
    LSM_TAB_SERVICES,
    LSM_TAB_FILESYSTEMS,
    LSM_TAB_OVERVIEW,
    LSM_TAB_COUNT
} LsmTabIndex;

#endif
