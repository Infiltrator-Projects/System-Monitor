// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_config.h
 * @brief Compile-time identity, tab indices and UI timing policy.
 *
 * Centralising these constants prevents modules from silently drifting to
 * different names, tab numbers or refresh intervals.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_APP_CONFIG_H
#define LINUX_SYSTEM_MONITOR_APP_CONFIG_H

#include "sampling_policy.h"

#define LSM_PROGRAM_NAME "Linux System Monitor"
#define LSM_EXECUTABLE_NAME "linux-system-monitor"
#define LSM_APPLICATION_ID "io.github.theinfiltratr.LinuxSystemMonitor"
#define LSM_CONFIG_DIRECTORY "linux-system-monitor"
#define LSM_LOG_DIRECTORY "Linux-System-Monitor-logs"
#define LSM_LEGACY_CONFIG_DIRECTORY "sysmontask-c"
#define LSM_LEGACY_LOG_DIRECTORY "sysmontask_log"


#define LSM_DEFAULT_WINDOW_WIDTH 1280
#define LSM_DEFAULT_WINDOW_HEIGHT 800

typedef enum {
    LSM_TAB_PERFORMANCE = 0,
    LSM_TAB_PROCESSES,
    LSM_TAB_APP_HISTORY,
    LSM_TAB_STARTUP,
    LSM_TAB_USERS,
    LSM_TAB_DETAILS,
    LSM_TAB_SERVICES,
    LSM_TAB_FILESYSTEMS,
    LSM_TAB_COUNT
} LsmTabIndex;

#define LSM_TAB_LAYOUT_VERSION 3

/**
 * Translate a persisted tab index from an earlier layout.
 *
 * Layout 2 was the short-lived 1.7.33 Processes-first order. Older preference
 * files use the pre-Details order from 1.7.32 and earlier. Keeping the
 * translation here makes the migration deterministic and independently
 * testable.
 *
 * @param [in] index Persisted numeric tab index.
 * @param [in] layout_version Persisted tab-layout version, or zero if absent.
 * @return Matching index in the current Performance-first layout.
 */
static inline LsmTabIndex lsm_tab_index_migrate(int index, int layout_version)
{
    if (index < 0 || index >= LSM_TAB_COUNT)
        return LSM_TAB_PERFORMANCE;
    if (layout_version >= LSM_TAB_LAYOUT_VERSION)
        return (LsmTabIndex)index;
    if (layout_version == 2) {
        if (index == 0) return LSM_TAB_PROCESSES;
        if (index == 1) return LSM_TAB_PERFORMANCE;
        return (LsmTabIndex)index;
    }
    switch (index) {
        case 0: return LSM_TAB_PERFORMANCE;
        case 1: return LSM_TAB_PROCESSES;
        case 2: return LSM_TAB_APP_HISTORY;
        case 3: return LSM_TAB_FILESYSTEMS;
        case 4: return LSM_TAB_STARTUP;
        case 5: return LSM_TAB_SERVICES;
        case 6: return LSM_TAB_USERS;
        case 7:
        case LSM_TAB_COUNT:
            return LSM_TAB_PERFORMANCE;
    }
    return LSM_TAB_PERFORMANCE;
}

#endif
