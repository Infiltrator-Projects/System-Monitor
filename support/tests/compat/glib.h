// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Minimal GLib main-context declarations used only by documentation syntax
 * checks that intentionally compile against the project's GTK compatibility
 * surface. Production builds use the distribution GLib headers via GTK.
 */
#ifndef LSM_MINIMAL_GLIB_H
#define LSM_MINIMAL_GLIB_H

#include <gtk/gtk.h>

#ifndef G_PRIORITY_DEFAULT
#define G_PRIORITY_DEFAULT 0
#endif

typedef struct _GMainContext GMainContext;

void g_main_context_invoke_full(
    GMainContext *context, gint priority, GSourceFunc function,
    gpointer data, GDestroyNotify notify);

#endif
