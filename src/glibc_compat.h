// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file glibc_compat.h
 * @brief Build-time compatibility controls for modern glibc headers.
 *
 * New glibc headers redirect ordinary C17 parsing calls to C23-only symbol
 * versions whenever _GNU_SOURCE is enabled.  Those implementations are not
 * required by Linux-System-Monitor and unnecessarily prevent a binary built on
 * a current distribution from running with glibc 2.34-2.37.  This header is
 * force-included before the normal system headers and retains the established
 * C17 scanf/strto ABI while leaving every other GNU/POSIX interface enabled.
 *
 * _FILE_OFFSET_BITS=64 and _TIME_BITS=64 are supplied by the build system so a
 * 32-bit build uses large-file and 64-bit time interfaces rather than inheriting
 * the historic i386 limits.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_GLIBC_COMPAT_H
#define LINUX_SYSTEM_MONITOR_GLIBC_COMPAT_H

#include <features.h>

#if defined(__GLIBC__) && defined(__GLIBC_PREREQ) && __GLIBC_PREREQ(2, 38)
/* glibc 2.38/2.39 headers use C2X in this internal feature name; newer
 * revisions may use the final C23 spelling. Define both to retain the C17 ABI
 * across either header convention. */
#undef __GLIBC_USE_C2X_STRTOL
#define __GLIBC_USE_C2X_STRTOL 0
#undef __GLIBC_USE_C23_STRTOL
#define __GLIBC_USE_C23_STRTOL 0
#endif

#endif
