// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Documentation-syntax compatibility additions layered over the compiler's
 * real C library. Production builds never add support/tests/compat to their
 * include path.
 */
#ifndef LSM_DOCUMENTATION_COMPAT_STDLIB_H
#define LSM_DOCUMENTATION_COMPAT_STDLIB_H

/* This shim is intentionally a compiler-system header: include_next is only
 * used by the documentation/test compatibility surface, never production. */
#pragma GCC system_header
#include_next <stdlib.h>

/* Only the deliberately minimal GTK compatibility surface needs a stand-in
 * for GLib's checked allocation helper. Real GLib owns g_try_new in ordinary
 * and peripheral-smoke builds, so defining it globally here would collide with
 * gmem.h and turn the compatibility layer itself into a build regression. */
#if defined(LSM_MINIMAL_GTK3_H) && !defined(g_try_new)
#define g_try_new(type, count) ((type *)malloc((count) * sizeof(type)))
#endif

#endif
