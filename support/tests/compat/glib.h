// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Minimal GLib main-context declarations used only by documentation syntax
 * checks that intentionally compile against the project's GTK compatibility
 * surface. Production builds use the distribution GLib headers via GTK.
 */
#ifndef LSM_MINIMAL_GLIB_H
#define LSM_MINIMAL_GLIB_H

#include <gtk/gtk.h>

#include <stdint.h>
#include <stdlib.h>

#ifndef G_PRIORITY_DEFAULT
#define G_PRIORITY_DEFAULT 0
#endif
#ifndef G_PRIORITY_DEFAULT_IDLE
#define G_PRIORITY_DEFAULT_IDLE 200
#endif

typedef struct _GMainContext GMainContext;

void g_main_context_invoke_full(
    GMainContext *context, gint priority, GSourceFunc function,
    gpointer data, GDestroyNotify notify);

/* The strict syntax shim also needs the one non-throwing GLib allocation macro
 * used by App History. Keep this compatibility definition local to the shim so
 * production translation units always receive GLib's own implementation. */
static inline void *glib_compat_try_new(size_t count, size_t element_size)
{
    if (element_size != 0U && count > SIZE_MAX / element_size)
        return NULL;
    return malloc(count * element_size);
}

#ifndef g_try_new
#define g_try_new(type, count) \
    ((type *)glib_compat_try_new((count), sizeof(type)))
#endif

#endif
