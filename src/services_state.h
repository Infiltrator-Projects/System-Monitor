// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file services_state.h
 * @brief Private feature-owned state for the Services page.
 *
 * Keeping this state separate from the complete application layout lets the
 * Services implementation own its widgets and asynchronous request lifetime
 * without depending on unrelated application subsystems.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_SERVICES_STATE_H
#define INFILTRATOR_SYSTEM_MONITOR_SERVICES_STATE_H

#include <gio/gio.h>
#include <gtk/gtk.h>

/** Services-page widgets and asynchronous systemd request state. */
typedef struct {
    GtkWidget *services_tree;
    GtkListStore *services_store;
    GtkWidget *services_search;
    GtkWidget *service_start_button;
    GtkWidget *service_stop_button;
    GtkWidget *service_restart_button;
    GtkWidget *service_enable_button;
    GtkWidget *service_count_label;
    gboolean services_available;
    gboolean services_refresh_pending;
    guint services_search_timer;
    GCancellable *services_refresh_cancellable;
    GCancellable *services_action_cancellable;
    guint services_action_pending;
} LsmServicesState;

#endif
