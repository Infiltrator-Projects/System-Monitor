// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file compiler.h
 * @brief Portable compiler annotations used by performance-critical code.
 *
 * The project deliberately keeps compiler-specific features behind this file.
 * Every annotation has a standards-compliant fallback, so a performance hint
 * can never become a portability requirement. Branch-probability hints are
 * reserved for objectively exceptional paths (invalid arguments, allocation
 * failure and disappearing procfs/sysfs objects); ordinary control flow is
 * intentionally left to the optimiser's profile-independent heuristics.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_COMPILER_H
#define LINUX_SYSTEM_MONITOR_COMPILER_H

#include <infiltratr/compiler.h>

#define LSM_LIKELY(expression) INFILTRATR_LIKELY(expression)
#define LSM_UNLIKELY(expression) INFILTRATR_UNLIKELY(expression)
#define LSM_COLD INFILTRATR_COLD
#define LSM_PRINTF_FORMAT(format_index, first_argument) \
    INFILTRATR_PRINTF_FORMAT(format_index, first_argument)

#endif
