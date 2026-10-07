// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file app_keyboard.c
 * @brief Page-aware keyboard commands kept outside page construction policy.
 *
 * Keyboard routing is presentation policy, not page registration. Keeping it
 * here prevents the central page descriptor registry from accumulating menu,
 * export and process-action dependencies. Private application layout is reached
 * only through app_presentation_context.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "app_keyboard.h"
#include "app_menu.h"
#include "app_page_registry.h"
#include "app_presentation_context.h"
#include "process_actions.h"
#include "process_export.h"
#include "process_navigation.h"

static gboolean focus_allows_pause(const LsmKeyboardControlView *view,
                                   GtkWidget *focus)
{
    return view &&
           (!focus || focus == view->notebook ||
            focus == view->processes_tree ||
            focus == view->details_tree ||
            focus == view->performance_stack);
}

static gboolean keyboard_key_press(GtkWidget *widget, GdkEventKey *event,
                                   gpointer user_data)
{
    (void)widget;
    LsmApp *app = user_data;
    LsmKeyboardControlView view;
    if (!lsm_app_keyboard_control_view(app, &view) ||
        !view.window || !view.notebook)
        return FALSE;

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
            GTK_NOTEBOOK(view.notebook));
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
            GTK_NOTEBOOK(view.notebook));
        GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(view.window));
        if ((current == LSM_TAB_PROCESSES &&
             focus == view.processes_tree) ||
            (current == LSM_TAB_DETAILS &&
             focus == view.details_tree)) {
            lsm_process_export_copy_selected(app);
            return TRUE;
        }
    }
    if (alt && event->keyval >= GDK_KEY_1 &&
        event->keyval <= GDK_KEY_1 + 8U) {
        const gint page_index = (gint)(event->keyval - GDK_KEY_1);
        if (page_index < LSM_TAB_COUNT) {
            gtk_notebook_set_current_page(
                GTK_NOTEBOOK(view.notebook), page_index);
            return TRUE;
        }
    }

    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(view.window));
    if (event->keyval == GDK_KEY_space && focus_allows_pause(&view, focus)) {
        if (view.pause_menu_item)
            gtk_check_menu_item_set_active(
                GTK_CHECK_MENU_ITEM(view.pause_menu_item), !view.paused);
        return TRUE;
    }

    const gint current = gtk_notebook_get_current_page(
        GTK_NOTEBOOK(view.notebook));
    if ((current == LSM_TAB_PROCESSES && focus == view.processes_tree) ||
        (current == LSM_TAB_DETAILS && focus == view.details_tree)) {
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
    LsmKeyboardControlView view;
    if (!lsm_app_keyboard_control_view(app, &view) || !view.window) return;
    g_signal_connect(view.window, "key-press-event",
                     G_CALLBACK(keyboard_key_press), app);
}
