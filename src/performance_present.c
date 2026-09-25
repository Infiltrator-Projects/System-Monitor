// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_present.c
 * @brief Performance-page snapshot-presentation dispatch and shared state styling.
 *
 * CPU/memory/disk/network presentation lives in performance_present_core.c;
 * device-oriented Bluetooth/GPU/battery/NPU presentation lives in
 * performance_present_devices.c. All presentation consumes retained snapshots
 * only and must not perform hardware collection.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_present.h"
#include "performance_present_internal.h"
#include "performance_internal.h"
#include "ui_helpers.h"

#include <math.h>

const char *performance_present_preferred_hardware_name(
    const char *product, const char *vendor)
{
    if (performance_useful_hardware_name(product)) return product;
    if (performance_useful_hardware_name(vendor)) return vendor;
    return "N/A";
}

void performance_present_set_large_device_title(
    GtkWidget *label, const char *kind, size_t index,
    const char *hardware_name)
{
    if (!label || !kind) return;
    char text[LSM_NAME_LEN + 32];
    performance_numbered_device_name(text, sizeof(text), kind, index,
                                     hardware_name);
    if (!lsm_ui_text_needs_update(gtk_label_get_text(GTK_LABEL(label)), text))
        return;
    char *markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>", text);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
}

void performance_present_set_temperature_state(GtkWidget *widget,
                                               bool available,
                                               double celsius,
                                               double warning_threshold,
                                               double fault_threshold)
{
    if (!widget) return;
    GtkStyleContext *style = gtk_widget_get_style_context(widget);
    if (!style) return;

    gtk_style_context_remove_class(style, "lsm-state-warning");
    gtk_style_context_remove_class(style, "lsm-state-fault");
    gtk_style_context_remove_class(style, "lsm-status-warning");
    gtk_style_context_remove_class(style, "lsm-status-fault");

    /* Reserve the status-pill geometry in every temperature state. Warning
     * and fault classes change only colour/background/border, so crossing a
     * threshold cannot resize the metric row and steal height from its graph. */
    gtk_style_context_add_class(style, "lsm-status-chip");
    if (!available || !isfinite(celsius)) return;

    if (celsius >= fault_threshold) {
        gtk_style_context_add_class(style, "lsm-state-fault");
        gtk_style_context_add_class(style, "lsm-status-chip");
        gtk_style_context_add_class(style, "lsm-status-fault");
    } else if (celsius >= warning_threshold) {
        gtk_style_context_add_class(style, "lsm-state-warning");
        gtk_style_context_add_class(style, "lsm-status-chip");
        gtk_style_context_add_class(style, "lsm-status-warning");
    }
}

void lsm_performance_record_page_sample(
    LsmApp *app, LsmDevicePage *page)
{
    if (!app || !page) return;
    if (!performance_record_core_page_sample(app, page))
        performance_record_device_page_sample(app, page);
}

void lsm_performance_present_page(LsmApp *app, LsmDevicePage *page)
{
    if (!app || !page) return;
    if (!performance_present_core_page(app, page))
        performance_present_device_page(app, page);
}
