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
#include "performance_view.h"
#include "metric_format.h"
#include "ui_helpers.h"

#include <math.h>
#include <stdio.h>

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

void lsm_performance_present_rail(LsmApp *app, LsmDevicePage *page)
{
    if (!app || !page || !page->button_value) return;

    switch (page->type) {
        case LSM_PAGE_CPU: {
            LsmCpuPerformanceView view;
            lsm_cpu_performance_view(&app->monitor, &view);
            lsm_ui_set_label_text(page->button_value, "%s", view.rail_value);
            return;
        }
        case LSM_PAGE_MEMORY: {
            LsmMemoryPerformanceView view;
            lsm_memory_performance_view(&app->monitor, &view);
            lsm_ui_set_label_text(page->button_value, "%s", view.rail_value);
            return;
        }
        case LSM_PAGE_DISK: {
            if (page->index >= app->monitor.disk_count) return;
            LsmDevicePerformanceView view;
            lsm_disk_performance_view(
                &app->monitor.disks[page->index], page->index, &view);
            lsm_ui_set_label_text(page->button_value, "%s", view.rail_value);
            return;
        }
        case LSM_PAGE_NETWORK: {
            if (page->index >= app->monitor.net_count) return;
            LsmDevicePerformanceView view;
            lsm_network_performance_view(
                &app->monitor.nets[page->index], page->index,
                app->runtime.network_use_bits, &view);
            lsm_ui_set_label_text(page->button_value, "%s", view.rail_value);
            return;
        }
        case LSM_PAGE_BLUETOOTH: {
            if (page->index >= app->monitor.bluetooth_device_count) return;
            const LsmBluetoothDeviceInfo *device =
                &app->monitor.bluetooth_devices[page->index];
            if (!device->traffic_available) {
                lsm_ui_set_label_text(page->button_value, "Traffic N/A");
                return;
            }
            char rates[64];
            lsm_metric_format_network_pair(
                (long double)device->tx_bytes_per_sec,
                (long double)device->rx_bytes_per_sec,
                app->runtime.network_use_bits, rates, sizeof(rates));
            lsm_ui_set_label_text(page->button_value, "%s", rates);
            return;
        }
        case LSM_PAGE_GPU: {
            if (page->index >= app->monitor.gpu_count) return;
            LsmDevicePerformanceView view;
            lsm_gpu_performance_view(
                &app->monitor.gpus[page->index], page->index, &view);
            lsm_ui_set_label_text(page->button_value, "%s", view.rail_value);
            return;
        }
        case LSM_PAGE_BATTERY: {
            if (page->index >= app->monitor.battery_count) return;
            const LsmBatteryInfo *battery =
                &app->monitor.batteries[page->index];
            char charge[32];
            if (isfinite(battery->capacity_percent))
                snprintf(charge, sizeof(charge), "%.0f%%",
                         battery->capacity_percent);
            else if (battery->capacity_level[0])
                snprintf(charge, sizeof(charge), "%s",
                         battery->capacity_level);
            else
                snprintf(charge, sizeof(charge), "N/A");
            lsm_ui_set_label_text(
                page->button_value, "%s — %s", charge,
                battery->status[0] ? battery->status : "N/A");
            return;
        }
        case LSM_PAGE_NPU: {
            if (page->index >= app->monitor.npu_count) return;
            const LsmNpuInfo *npu = &app->monitor.npus[page->index];
            if (!npu->utilization_available)
                lsm_ui_set_label_text(page->button_value, "Detected");
            else if (npu->utilization_percent < 0.5)
                lsm_ui_set_label_text(page->button_value, "Idle");
            else
                lsm_ui_set_label_text(
                    page->button_value, "%.0f%% active",
                    npu->utilization_percent);
            return;
        }
        case LSM_PAGE_COUNT:
            return;
    }
}
