// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file task_manager_layout_smoke.c
 * @brief Lock the canonical Performance-first tab order.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
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
    puts("Canonical Performance-first tab order passed.");
    return 0;
}
