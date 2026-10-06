// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_keyboard.c
 * @brief Page-aware keyboard commands kept outside page construction policy.
 *
 * Keyboard routing is presentation policy, not page registration. Keeping it
 * here prevents the central page descriptor registry from accumulating menu,
 * export and process-action dependencies.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_keyboard.h"
#include "app_internal.h"
#include "app_menu.h"
#include "app_page_registry.h"
#include "process_actions.h"
#include "process_export.h"
#include "process_navigation.h"

static gboolean focus_allows_pause(const LsmApp *app, GtkWidget *focus)
{
    return !focus || focus == app->shell.notebook ||
           focus == app->processes.processes_tree ||
           focus == app->details.details_tree ||
           focus == app->performance.performance_stack;
}

static gboolean keyboard_key_press(GtkWidget *widget, GdkEventKey *event,
                                   gpointer user_data)
{
    (void)widget;
    LsmApp *app = user_data;
    if (!app) return FALSE;

    const gboolean control = (event->state & GDK_CONTROL_MASK) != 0;
    const gboolean shift = (event->state & GDK_SHIFT_MASK) != 0;
    const gboolean alt = (event->state & GDK_MOD1_MASK) != 0;

    if (event->keyval == GDK_KEY_F5) {
        lsm_app_menu_refresh(NULL, app);
        return TRUE;
    }
    if (control && (event->keyval == GDK_KEY_f ||
                    event->keyval == GDK_KEY_F)) {
        const gint current = gtk_notebook_get_current_page(
            GTK_NOTEBOOK(app->shell.notebook));
        GtkWidget *search = current >= 0 && current < LSM_TAB_COUNT
            ? lsm_app_page_registry_search_widget(
                  app, (LsmTabIndex)current)
            : NULL;
        if (search) {
            gtk_widget_grab_focus(search);
            return TRUE;
        }
    }
    if (control && shift && (event->keyval == GDK_KEY_s ||
                             event->keyval == GDK_KEY_S)) {
        lsm_app_menu_save_snapshot(NULL, app);
        return TRUE;
    }
    if (control && (event->keyval == GDK_KEY_c ||
                    event->keyval == GDK_KEY_C)) {
        const gint current = gtk_notebook_get_current_page(
            GTK_NOTEBOOK(app->shell.notebook));
        GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(app->shell.window));
        if ((current == LSM_TAB_PROCESSES &&
             focus == app->processes.processes_tree) ||
            (current == LSM_TAB_DETAILS &&
             focus == app->details.details_tree)) {
            lsm_process_export_copy_selected(app);
            return TRUE;
        }
    }
    if (alt && event->keyval >= GDK_KEY_1 &&
        event->keyval <= GDK_KEY_1 + 8U) {
        const gint page_index = (gint)(event->keyval - GDK_KEY_1);
        if (page_index < LSM_TAB_COUNT) {
            gtk_notebook_set_current_page(
                GTK_NOTEBOOK(app->shell.notebook), page_index);
            return TRUE;
        }
    }

    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(app->shell.window));
    if (event->keyval == GDK_KEY_space && focus_allows_pause(app, focus)) {
        if (app->shell.pause_menu_item)
            gtk_check_menu_item_set_active(
                GTK_CHECK_MENU_ITEM(app->shell.pause_menu_item),
                !app->runtime.paused);
        return TRUE;
    }

    const gint current = gtk_notebook_get_current_page(
        GTK_NOTEBOOK(app->shell.notebook));
    if ((current == LSM_TAB_PROCESSES &&
         focus == app->processes.processes_tree) ||
        (current == LSM_TAB_DETAILS &&
         focus == app->details.details_tree)) {
        if (event->keyval == GDK_KEY_Return ||
            event->keyval == GDK_KEY_KP_Enter) {
            if (current == LSM_TAB_PROCESSES)
                lsm_processes_go_to_details(app);
            else
                lsm_processes_show_selected_details(app);
            return TRUE;
        }
        if (event->keyval == GDK_KEY_Delete) {
            lsm_processes_end_selected(app);
            return TRUE;
        }
    }
    return FALSE;
}

void lsm_app_keyboard_connect(LsmApp *app)
{
    if (!app || !app->shell.window) return;
    g_signal_connect(app->shell.window, "key-press-event",
                     G_CALLBACK(keyboard_key_press), app);
}
