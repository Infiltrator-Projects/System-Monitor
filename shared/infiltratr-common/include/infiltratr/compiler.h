// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file compiler.h
 * @brief Portable compiler annotations shared by The Infiltratr C projects.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATR_COMMON_COMPILER_H
#define INFILTRATR_COMMON_COMPILER_H

#if defined(__GNUC__) || defined(__clang__)
#define INFILTRATR_LIKELY(expression) __builtin_expect(!!(expression), 1)
#define INFILTRATR_UNLIKELY(expression) __builtin_expect(!!(expression), 0)
#define INFILTRATR_COLD __attribute__((cold))
#define INFILTRATR_PRINTF_FORMAT(format_index, first_argument) \
    __attribute__((format(printf, format_index, first_argument)))
#else
#define INFILTRATR_LIKELY(expression) (!!(expression))
#define INFILTRATR_UNLIKELY(expression) (!!(expression))
#define INFILTRATR_COLD
#define INFILTRATR_PRINTF_FORMAT(format_index, first_argument)
#endif

#endif
