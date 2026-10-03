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

/* The GTK compatibility surface models the small GLib allocation subset used
 * by the source. This macro is compile-only; production builds use GLib's
 * checked g_try_new implementation. */
#ifndef g_try_new
#define g_try_new(type, count) ((type *)malloc((count) * sizeof(type)))
#endif

#endif
