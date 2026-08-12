// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file project_info.h
 * @brief Canonical application identity exposed through Infiltratr Common.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROJECT_INFO_H
#define LINUX_SYSTEM_MONITOR_PROJECT_INFO_H

#include <infiltratr/core.h>

/**
 * Return the immutable identity record for the running build.
 *
 * @return Process-lifetime project record owned by this module.
 */
const InfiltratrProjectInfo *lsm_project_info(void);

#endif
