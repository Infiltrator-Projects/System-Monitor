// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_model.c
 * @brief Platform-neutral process-model helpers.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_model.h"

const char *lsm_process_priority_name(LsmProcessPriority priority)
{
    switch (priority) {
        case LSM_PROCESS_PRIORITY_HIGH: return "High";
        case LSM_PROCESS_PRIORITY_ABOVE_NORMAL: return "Above normal";
        case LSM_PROCESS_PRIORITY_NORMAL: return "Normal";
        case LSM_PROCESS_PRIORITY_BELOW_NORMAL: return "Below normal";
        case LSM_PROCESS_PRIORITY_LOW: return "Low";
    }
    return "Normal";
}
