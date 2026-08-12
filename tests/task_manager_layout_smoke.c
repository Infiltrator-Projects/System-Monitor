// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file task_manager_layout_smoke.c
 * @brief Lock the Performance-first tab order and preference migration.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_config.h"

#include <stdio.h>

int main(void)
{
    if (LSM_TAB_PERFORMANCE != 0) return 1;
    if (LSM_TAB_PROCESSES != 1) return 2;
    if (LSM_TAB_APP_HISTORY != 2) return 3;
    if (LSM_TAB_STARTUP != 3) return 4;
    if (LSM_TAB_USERS != 4) return 5;
    if (LSM_TAB_DETAILS != 5) return 6;
    if (LSM_TAB_SERVICES != 6) return 7;
    if (LSM_TAB_FILESYSTEMS != 7) return 8;
    if (LSM_TAB_COUNT != 8) return 9;
    if (lsm_tab_index_migrate(0, 2) != LSM_TAB_PROCESSES) return 10;
    if (lsm_tab_index_migrate(1, 2) != LSM_TAB_PERFORMANCE) return 11;
    if (lsm_tab_index_migrate(3, 2) != LSM_TAB_STARTUP) return 12;
    if (lsm_tab_index_migrate(0, 0) != LSM_TAB_PERFORMANCE) return 13;
    if (lsm_tab_index_migrate(1, 0) != LSM_TAB_PROCESSES) return 14;
    if (lsm_tab_index_migrate(3, 0) != LSM_TAB_FILESYSTEMS) return 15;
    if (lsm_tab_index_migrate(4, 0) != LSM_TAB_STARTUP) return 16;
    if (lsm_tab_index_migrate(5, 0) != LSM_TAB_SERVICES) return 17;
    if (lsm_tab_index_migrate(6, 0) != LSM_TAB_USERS) return 18;
    if (lsm_tab_index_migrate(7, LSM_TAB_LAYOUT_VERSION) !=
        LSM_TAB_FILESYSTEMS)
        return 19;
    if (lsm_tab_index_migrate(-1, LSM_TAB_LAYOUT_VERSION) !=
        LSM_TAB_PERFORMANCE)
        return 20;
    puts("Performance-first tab order and preference migration passed.");
    return 0;
}
