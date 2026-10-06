// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_navigation.h
 * @brief Cross-page navigation for process presentations.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_NAVIGATION_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_NAVIGATION_H

typedef struct LsmApp LsmApp;

/**
 * Navigate from the grouped Processes presentation to technical Details.
 * @param app Application containing the current process selection and pages.
 */
void lsm_processes_go_to_details(LsmApp *app);

#endif
