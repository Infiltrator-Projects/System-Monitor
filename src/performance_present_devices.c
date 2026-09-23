// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_present_devices.c
 * @brief Bluetooth, GPU, battery and NPU snapshot presentation.
 *
 * Device presentation consumes retained monitor state only. It may format,
 * graph and hide unavailable metrics but never performs hardware discovery.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_present_internal.h"
#include "performance_internal.h"

#include "common.h"
#include "duration_format.h"
#include "metric_format.h"
#include "ui_helpers.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool usable_battery_level(const char *level)
{
    return level && level[0] && !lsm_ascii_equal_ci(level, "Unknown");
}

static double coarse_battery_graph_value(const char *level)
{
    if (!usable_battery_level(level)) return NAN;
    if (lsm_ascii_equal_ci(level, "Critical")) return 5.0;
    if (lsm_ascii_equal_ci(level, "Low")) return 20.0;
    if (lsm_ascii_equal_ci(level, "Normal")) return 50.0;
    if (lsm_ascii_equal_ci(level, "High")) return 80.0;
    if (lsm_ascii_equal_ci(level, "Full")) return 100.0;
    return NAN;
}

static void format_battery_charge(const LsmBatteryInfo *battery,
                                  char *buffer, size_t size)
{
    if (isfinite(battery->capacity_percent))
        snprintf(buffer, size, "%.0f%%", battery->capacity_percent);
    else if (usable_battery_level(battery->capacity_level))
        lsm_copy_string(buffer, size, battery->capacity_level);
    else
        snprintf(buffer, size, "N/A");
}


static void update_bluetooth_page(LsmApp *app, LsmDevicePage *page)
{
    LsmBluetoothDeviceInfo *device =
        &app->monitor.bluetooth_devices[page->index];
    LsmBluetoothPageWidgets *widgets = &page->widgets.bluetooth;
    char receive[64], send[64], received[64], sent[64];
    char scale[64], mid_scale[64], compact_rates[64];

    const double rx_rate = device->traffic_available
        ? device->rx_bytes_per_sec : 0.0;
    const double tx_rate = device->traffic_available
        ? device->tx_bytes_per_sec : 0.0;
    lsm_graph_push(page->graph, rx_rate, tx_rate,
                   app->runtime.newer_on_right);
    lsm_graph_push(page->side_graph, rx_rate, tx_rate,
                   app->runtime.newer_on_right);

    if (device->traffic_available) {
        lsm_metric_format_network(
            (long double)device->rx_bytes_per_sec,
            app->runtime.network_use_bits, true,
            receive, sizeof(receive));
        lsm_metric_format_network(
            (long double)device->tx_bytes_per_sec,
            app->runtime.network_use_bits, true,
            send, sizeof(send));
        lsm_metric_format_network(
            (long double)device->rx_bytes_total,
            app->runtime.network_use_bits, false,
            received, sizeof(received));
        lsm_metric_format_network(
            (long double)device->tx_bytes_total,
            app->runtime.network_use_bits, false,
            sent, sizeof(sent));
        lsm_metric_format_network_pair(
            (long double)device->tx_bytes_per_sec,
            (long double)device->rx_bytes_per_sec,
            app->runtime.network_use_bits,
            compact_rates, sizeof(compact_rates));
        lsm_ui_set_label_text(page->button_value, "%s", compact_rates);
    } else {
        snprintf(receive, sizeof(receive), "N/A");
        snprintf(send, sizeof(send), "N/A");
        snprintf(received, sizeof(received), "N/A");
        snprintf(sent, sizeof(sent), "N/A");
        lsm_ui_set_label_text(page->button_value, "Traffic N/A");
    }

    const double graph_maximum = lsm_graph_get_maximum(page->graph);
    lsm_metric_format_network(
        (long double)graph_maximum,
        app->runtime.network_use_bits, true, scale, sizeof(scale));
    lsm_metric_format_network(
        (long double)(graph_maximum / 2.0),
        app->runtime.network_use_bits, true, mid_scale, sizeof(mid_scale));
    lsm_ui_set_label_text(page->scale_label, "%s", scale);
    lsm_ui_set_label_text(widgets->mid_scale, "%s", mid_scale);
    lsm_ui_set_label_text(widgets->receive_rate, "%s", receive);
    lsm_ui_set_label_text(widgets->received_total, "%s", received);
    lsm_ui_set_label_text(widgets->send_rate, "%s", send);
    lsm_ui_set_label_text(widgets->sent_total, "%s", sent);

    lsm_ui_set_label_text(widgets->status, "%s",
                          device->connected ? "Connected" : "Disconnected");
    if (device->traffic_available)
        lsm_ui_set_label_text(widgets->links, "%u", device->link_count);
    else
        lsm_ui_set_label_text(widgets->links, "N/A");
    lsm_ui_set_label_text(widgets->paired, "%s",
                          device->paired ? "Yes" : "No");
    lsm_ui_set_label_text(widgets->trusted, "%s",
                          device->trusted ? "Yes" : "No");
    lsm_ui_set_label_text(widgets->controller, "%s",
                          device->controller[0] ? device->controller : "N/A");
    lsm_ui_set_label_text(widgets->address, "%s",
                          device->address[0] ? device->address : "N/A");
    lsm_ui_set_label_text(widgets->name, "%s",
                          device->name[0] ? device->name : "N/A");
    lsm_ui_set_label_text(widgets->alias, "%s",
                          device->alias[0] ? device->alias : "N/A");
    lsm_ui_set_label_text(widgets->address_type, "%s",
                          device->address_type[0]
                              ? device->address_type : "N/A");
    lsm_ui_set_label_text(widgets->services_resolved, "%s",
                          device->services_resolved ? "Yes" : "No");
    lsm_ui_set_label_text(widgets->icon, "%s",
                          device->icon[0] ? device->icon : "N/A");
    lsm_ui_set_label_text(widgets->modalias, "%s",
                          device->modalias[0] ? device->modalias : "N/A");
    lsm_ui_set_label_text(widgets->product, "%s",
                          device->controller[0]
                              ? device->controller : "Bluetooth");
}

static void update_gpu_page(LsmApp *app, LsmDevicePage *page)
{
    LsmGpuInfo *gpu = &app->monitor.gpus[page->index];
    LsmGpuPageWidgets *widgets = &page->widgets.gpu;
    char a[64];
    const gboolean temperature_available = gpu->temperature_available &&
        isfinite(gpu->temperature_c);
    const char *product = performance_useful_hardware_name(gpu->name) ? gpu->name : "N/A";
    if (strcmp(page->hardware_product, product) != 0) {
        lsm_copy_string(page->hardware_product,
                        sizeof(page->hardware_product), product);
        if (strcmp(product, "N/A") == 0)
            lsm_ui_set_label_text(page->button_title, "GPU %zu", page->index);
        else
            lsm_ui_set_label_text(page->button_title, "GPU %zu — %s",
                                  page->index, product);
        performance_present_set_large_device_title(page->title, "GPU", page->index, product);
    }
    if (page->optional_note)
        gtk_widget_set_visible(page->optional_note,
                               !gpu->supported_metrics);

    const gboolean has_engine_graphs = lsm_gpu_has_engine_metrics(gpu);
    widgets->engine_graphs_available = has_engine_graphs;
    if (has_engine_graphs && !widgets->graph_defaults_initialised) {
        LsmGpuMetric defaults[LSM_GPU_GRAPH_SLOT_COUNT];
        lsm_gpu_default_metrics(gpu, defaults, LSM_GPU_GRAPH_SLOT_COUNT);
        lsm_performance_populate_gpu_metric_selector(&widgets->single_engine_graph, gpu,
                                     defaults[0]);
        for (size_t slot_index = 0U;
             slot_index < LSM_GPU_GRAPH_SLOT_COUNT; slot_index++) {
            LsmGpuGraphSlot *slot = &widgets->engine_graphs[slot_index];
            lsm_performance_populate_gpu_metric_selector(slot, gpu, defaults[slot_index]);
        }
        widgets->graph_defaults_initialised = TRUE;
    }
    if (widgets->fallback_graph_box)
        gtk_widget_set_visible(widgets->fallback_graph_box,
                               !has_engine_graphs);
    if (widgets->detailed_graph_box)
        gtk_widget_set_visible(widgets->detailed_graph_box, has_engine_graphs);
    lsm_ui_set_label_text(page->subtitle, "%s",
        has_engine_graphs ? "GPU engines"
                          : (gpu->engine_metrics_capable
                                 ? "Peak engine utilisation" : "Utilisation"));

    const double utilisation = gpu->utilization_available
        ? gpu->utilization_percent : 0.0;
    lsm_graph_push(page->graph, utilisation, gpu->memory_percent,
                   app->runtime.newer_on_right);
    lsm_graph_push(page->side_graph, utilisation, 0.0,
                   app->runtime.newer_on_right);

    char graph_metric[32];
    LsmGpuGraphSlot *single = &widgets->single_engine_graph;
    const bool single_available = lsm_gpu_metric_available(gpu, single->metric);
    const double single_value = lsm_gpu_metric_value(gpu, single->metric);
    lsm_graph_push(single->graph, single_value, 0.0, app->runtime.newer_on_right);
    lsm_ui_set_label_text(
        single->value, "%s",
        lsm_metric_format_percent(single_available, single_value, graph_metric,
                                  sizeof(graph_metric)));
    for (size_t slot_index = 0U;
         slot_index < LSM_GPU_GRAPH_SLOT_COUNT; slot_index++) {
        LsmGpuGraphSlot *slot = &widgets->engine_graphs[slot_index];
        const bool available = lsm_gpu_metric_available(gpu, slot->metric);
        const double value = lsm_gpu_metric_value(gpu, slot->metric);
        lsm_graph_push(slot->graph, value, 0.0, app->runtime.newer_on_right);
        lsm_ui_set_label_text(
            slot->value, "%s",
            lsm_metric_format_percent(available, value, graph_metric,
                                      sizeof(graph_metric)));
    }
    lsm_graph_push(widgets->memory_graph, gpu->memory_percent, 0.0,
                   app->runtime.newer_on_right);
    if (gpu->memory_total_bytes > 0U)
        lsm_ui_set_label_text(widgets->memory_graph_value, "%s",
            lsm_format_bytes(gpu->memory_total_bytes, graph_metric,
                             sizeof(graph_metric)));
    else if (gpu->shared_system_memory)
        lsm_ui_set_label_text(widgets->memory_graph_value, "Dynamic");
    else
        lsm_ui_set_label_text(widgets->memory_graph_value, "N/A");

    const double busiest_percent = gpu->active_engine_percent;
    const char *busiest = gpu->active_engine[0]
        ? gpu->active_engine : "N/A";
    if (gpu->engine_metrics_capable && gpu->utilization_available) {
        if (temperature_available)
            lsm_ui_set_label_text(page->button_value, "%s %.0f%% %.0f °C",
                                  busiest, gpu->utilization_percent,
                                  gpu->temperature_c);
        else
            lsm_ui_set_label_text(page->button_value, "%s %.0f%% N/A",
                                  busiest, gpu->utilization_percent);
    } else if (gpu->utilization_available && temperature_available) {
        lsm_ui_set_label_text(page->button_value, "%.0f%% %.0f °C",
                              gpu->utilization_percent, gpu->temperature_c);
    } else if (gpu->utilization_available) {
        lsm_ui_set_label_text(page->button_value, "%.0f%% N/A",
                              gpu->utilization_percent);
    } else {
        lsm_ui_set_label_text(page->button_value, "N/A");
    }

    lsm_ui_set_label_text(widgets->product, "%s", product);
    lsm_ui_set_label_text(widgets->utilisation, "%s",
        lsm_metric_format_percent(gpu->utilization_available,
                                  gpu->utilization_percent, a, sizeof(a)));
    if (gpu->shared_system_memory)
        lsm_ui_set_label_text(widgets->memory_usage, "Dynamic");
    else
        lsm_ui_set_label_text(widgets->memory_usage, "%.0f%%",
                              gpu->memory_percent);
    lsm_ui_set_label_text(widgets->temperature, "%s",
        lsm_metric_format_celsius(temperature_available, gpu->temperature_c,
                                  a, sizeof(a)));
    performance_present_set_temperature_state(widgets->temperature, temperature_available,
                          gpu->temperature_c, 80.0, 95.0);
    lsm_ui_set_label_text(widgets->core_clock, "%s",
        lsm_metric_format_mhz(gpu->core_clock_available, gpu->core_clock_mhz,
                              a, sizeof(a)));

    if (gpu->shared_system_memory) {
        lsm_ui_set_label_text(widgets->memory_used, "Dynamic system RAM");
        lsm_ui_set_label_text(widgets->memory_total, "None");
    } else if (gpu->memory_total_bytes > 0) {
        lsm_ui_set_label_text(widgets->memory_used, "%s",
                           lsm_format_bytes(gpu->memory_used_bytes, a, sizeof(a)));
        lsm_ui_set_label_text(widgets->memory_total, "%s",
                           lsm_format_bytes(gpu->memory_total_bytes, a, sizeof(a)));
    } else {
        lsm_ui_set_label_text(widgets->memory_used, "N/A");
        lsm_ui_set_label_text(widgets->memory_total, "N/A");
    }
    lsm_ui_set_label_text(widgets->driver, "%s", gpu->driver);
    lsm_ui_set_label_text(widgets->driver_version, "%s",
                          gpu->driver_version[0]
                              ? gpu->driver_version : "N/A");
    lsm_ui_set_label_text(widgets->pci_location, "%s",
                          gpu->pci_location[0] ? gpu->pci_location : "N/A");
    if (strcmp(busiest, "N/A") == 0 || strcmp(busiest, "Idle") == 0)
        lsm_ui_set_label_text(widgets->active_engine, "%s", busiest);
    else
        lsm_ui_set_label_text(widgets->active_engine, "%s (%.0f%%)",
                              busiest, busiest_percent);
    lsm_ui_set_label_text(widgets->metrics, "%s",
                       gpu->metrics_source[0]
                           ? gpu->metrics_source
                           : (gpu->supported_metrics
                                  ? "Native driver telemetry"
                                  : "Basic identification only"));
    if (gpu->engine_metrics_capable) {
        lsm_ui_set_label_text(widgets->engine_1, "%s",
            lsm_metric_format_percent(gpu->render_available, gpu->render_percent,
                                      a, sizeof(a)));
        lsm_ui_set_label_text(widgets->engine_2, "%s",
            lsm_metric_format_percent(gpu->compute_available, gpu->compute_percent,
                                      a, sizeof(a)));
        lsm_ui_set_label_text(widgets->engine_3, "%s",
            lsm_metric_format_percent(gpu->video_available, gpu->video_percent,
                                      a, sizeof(a)));
        lsm_ui_set_label_text(widgets->engine_4, "%s",
            lsm_metric_format_percent(gpu->video_enhance_available,
                                      gpu->video_enhance_percent, a, sizeof(a)));
        lsm_ui_set_label_text(widgets->engine_5, "%s",
            lsm_metric_format_percent(gpu->copy_available, gpu->copy_percent,
                                      a, sizeof(a)));
        lsm_ui_set_label_text(widgets->memory_clock, "%s",
            lsm_metric_format_mhz(gpu->memory_clock_available,
                                  gpu->memory_clock_mhz, a, sizeof(a)));
        lsm_ui_set_label_text(widgets->power, "%s",
            lsm_metric_format_watts(gpu->power_available, gpu->power_watts,
                                    a, sizeof(a)));
        if (gpu->fan_available && isfinite(gpu->fan_percent))
            lsm_ui_set_label_text(widgets->cooling, "%.0f%%", gpu->fan_percent);
        else if (gpu->integrated_cooling)
            lsm_ui_set_label_text(widgets->cooling, "System-managed");
        else
            lsm_ui_set_label_text(widgets->cooling, "N/A");
        if (strcmp(busiest, "N/A") == 0)
            lsm_ui_set_label_text(widgets->busiest_engine, "N/A");
        else if (strcmp(busiest, "Idle") == 0)
            lsm_ui_set_label_text(widgets->busiest_engine, "Idle");
        else
            lsm_ui_set_label_text(widgets->busiest_engine, "%s (%.0f%%)",
                                  busiest, busiest_percent);
    } else {
        if (gpu->memory_busy_available)
            lsm_ui_set_label_text(widgets->engine_1, "%.0f%%",
                                  gpu->memory_busy_percent);
        else
            lsm_ui_set_label_text(widgets->engine_1, "N/A");
        if (gpu->encoder_available)
            lsm_ui_set_label_text(widgets->engine_2, "%.0f%%",
                                  gpu->encoder_percent);
        else
            lsm_ui_set_label_text(widgets->engine_2, "N/A");
        if (gpu->decoder_available)
            lsm_ui_set_label_text(widgets->engine_3, "%.0f%%",
                                  gpu->decoder_percent);
        else
            lsm_ui_set_label_text(widgets->engine_3, "N/A");
        lsm_ui_set_label_text(widgets->memory_clock, "%s",
            lsm_metric_format_mhz(gpu->memory_clock_available,
                                  gpu->memory_clock_mhz, a, sizeof(a)));
        lsm_ui_set_label_text(widgets->power, "%s",
            lsm_metric_format_watts(gpu->power_available, gpu->power_watts,
                                    a, sizeof(a)));
        if (gpu->fan_available && isfinite(gpu->fan_percent))
            lsm_ui_set_label_text(widgets->cooling, "%.0f%%", gpu->fan_percent);
        else
            lsm_ui_set_label_text(widgets->cooling, "N/A");
    }
}


/* Battery pages share one widget set for system batteries and peripherals;
 * helper accessors keep the type-specific caption/value mapping explicit. */
static GtkWidget *battery_detail_value(LsmBatteryPageWidgets *widgets,
                                       size_t index)
{
    switch (index) {
        case 0U: return widgets->manufacturer;
        case 1U: return widgets->detail_2;
        case 2U: return widgets->detail_3;
        case 3U: return widgets->detail_4;
        case 4U: return widgets->detail_5;
        case 5U: return widgets->detail_6;
        case 6U: return widgets->detail_7;
        case 7U: return widgets->detail_8;
        case 8U: return widgets->detail_9;
        case 9U: return widgets->power_source;
        case 10U: return widgets->temperature;
        default: return NULL;
    }
}

static GtkWidget *battery_detail_caption(LsmBatteryPageWidgets *widgets,
                                         size_t index)
{
    switch (index) {
        case 0U: return widgets->manufacturer_caption;
        case 1U: return widgets->detail_2_caption;
        case 2U: return widgets->detail_3_caption;
        case 3U: return widgets->detail_4_caption;
        case 4U: return widgets->detail_5_caption;
        case 5U: return widgets->detail_6_caption;
        case 6U: return widgets->detail_7_caption;
        case 7U: return widgets->detail_8_caption;
        case 8U: return widgets->detail_9_caption;
        case 9U: return widgets->power_source_caption;
        case 10U: return widgets->temperature_caption;
        default: return NULL;
    }
}

static void set_battery_detail(LsmBatteryPageWidgets *widgets, size_t index,
                               const char *caption, const char *value)
{
    GtkWidget *caption_widget = battery_detail_caption(widgets, index);
    GtkWidget *value_widget = battery_detail_value(widgets, index);
    if (caption_widget)
        lsm_ui_set_label_text(caption_widget, "%s", caption);
    if (value_widget)
        lsm_ui_set_label_text(value_widget, "%s",
                              value && value[0] ? value : "N/A");
}

static const char *yes_no(bool value)
{
    return value ? "Yes" : "No";
}

static void update_peripheral_details(LsmBatteryPageWidgets *widgets,
                                      const LsmBatteryInfo *battery)
{
    set_battery_detail(widgets, 0U, "Manufacturer", battery->manufacturer);
    set_battery_detail(widgets, 1U, "Device type", battery->device_type);
    set_battery_detail(widgets, 2U, "Connection",
                       battery->connection[0] ? battery->connection
                                              : battery->technology);
    set_battery_detail(widgets, 3U, "Address", battery->serial);
    set_battery_detail(widgets, 4U, "Battery source",
                       battery->battery_source);
    set_battery_detail(widgets, 5U, "Paired",
                       battery->bluetooth_details_available
                           ? yes_no(battery->paired) : "N/A");
    set_battery_detail(widgets, 6U, "Trusted",
                       battery->bluetooth_details_available
                           ? yes_no(battery->trusted) : "N/A");
    set_battery_detail(widgets, 7U, "Services",
                       battery->bluetooth_details_available
                           ? (battery->services_resolved ? "Resolved" : "Pending")
                           : "N/A");
    set_battery_detail(widgets, 8U, "Device ID", battery->modalias);
    set_battery_detail(widgets, 9U, "Power source", "Peripheral device");
    if (isfinite(battery->temperature_c)) {
        char temperature[32];
        snprintf(temperature, sizeof(temperature), "%.1f °C",
                 battery->temperature_c);
        set_battery_detail(widgets, 10U, "Temperature", temperature);
    } else {
        set_battery_detail(widgets, 10U, "Temperature", "N/A");
    }
}

static void update_system_battery_details(LsmBatteryPageWidgets *widgets,
                                          const LsmBatteryInfo *battery)
{
    static const char *const captions[] = {
        "Manufacturer", "Health", "Technology", "Energy now",
        "Full capacity", "Design capacity", "Voltage", "Current",
        "Cycles", "Power source", "Temperature"
    };
    for (size_t index = 0U; index < G_N_ELEMENTS(captions); index++)
        set_battery_detail(widgets, index, captions[index], NULL);

    lsm_ui_set_label_text(widgets->manufacturer, "%s",
                          battery->manufacturer[0]
                              ? battery->manufacturer : "N/A");
    lsm_ui_set_label_text(widgets->detail_2, "%s",
                          battery->health[0] ? battery->health : "N/A");
    lsm_ui_set_label_text(widgets->detail_3, "%s",
                          battery->technology[0]
                              ? battery->technology : "N/A");
    if (isfinite(battery->energy_now_wh))
        lsm_ui_set_label_text(widgets->detail_4, "%.1f Wh",
                              battery->energy_now_wh);
    else lsm_ui_set_label_text(widgets->detail_4, "N/A");
    if (isfinite(battery->energy_full_wh))
        lsm_ui_set_label_text(widgets->detail_5, "%.1f Wh",
                              battery->energy_full_wh);
    else lsm_ui_set_label_text(widgets->detail_5, "N/A");
    if (isfinite(battery->energy_design_wh))
        lsm_ui_set_label_text(widgets->detail_6, "%.1f Wh",
                              battery->energy_design_wh);
    else lsm_ui_set_label_text(widgets->detail_6, "N/A");
    if (isfinite(battery->voltage_volts))
        lsm_ui_set_label_text(widgets->detail_7, "%.2f V",
                              battery->voltage_volts);
    else lsm_ui_set_label_text(widgets->detail_7, "N/A");
    if (isfinite(battery->current_amps))
        lsm_ui_set_label_text(widgets->detail_8, "%.2f A",
                              battery->current_amps);
    else lsm_ui_set_label_text(widgets->detail_8, "N/A");
    if (battery->cycle_count)
        lsm_ui_set_label_text(widgets->detail_9, "%u", battery->cycle_count);
    else lsm_ui_set_label_text(widgets->detail_9, "N/A");
    lsm_ui_set_label_text(widgets->power_source, "%s",
                          battery->on_ac_power
                              ? "AC connected" : "Battery power");
    if (isfinite(battery->temperature_c))
        lsm_ui_set_label_text(widgets->temperature, "%.1f °C",
                              battery->temperature_c);
    else lsm_ui_set_label_text(widgets->temperature, "N/A");
}

static void update_battery_page(LsmApp *app, LsmDevicePage *page)
{
    LsmBatteryInfo *battery = &app->monitor.batteries[page->index];
    LsmBatteryPageWidgets *widgets = &page->widgets.battery;
    char text[64], charge[32];
    format_battery_charge(battery, charge, sizeof(charge));
    const bool exact_capacity = isfinite(battery->capacity_percent);
    const bool coarse_capacity = usable_battery_level(battery->capacity_level);
    const double graph_value = exact_capacity
        ? battery->capacity_percent
        : coarse_battery_graph_value(battery->capacity_level);
    lsm_graph_push(page->graph, graph_value, 0.0, app->runtime.newer_on_right);
    lsm_graph_push(page->side_graph, graph_value, 0.0, app->runtime.newer_on_right);
    lsm_ui_set_label_text(page->button_value, "%s — %s", charge,
                          battery->status[0] ? battery->status : "N/A");
    lsm_ui_set_label_text(widgets->product, "%s",
                       battery->model[0] ? battery->model : battery->name);
    lsm_ui_set_label_text(widgets->charge, "%s", charge);
    lsm_ui_set_label_text(widgets->status, "%s",
                          battery->status[0] ? battery->status : "N/A");
    lsm_ui_set_label_text(page->scale_label, "%s",
                          exact_capacity ? "100%" :
                          coarse_capacity ? "Coarse level" : "N/A");
    lsm_duration_format_remaining(battery->seconds_remaining, text,
                                  sizeof(text));
    lsm_ui_set_label_text(widgets->remaining, "%s", text);
    lsm_ui_set_label_text(widgets->power, "%s",
        lsm_metric_format_watts(isfinite(battery->power_watts),
                                battery->power_watts, text, sizeof(text)));
    if (battery->is_peripheral)
        update_peripheral_details(widgets, battery);
    else
        update_system_battery_details(widgets, battery);
}

static void update_npu_page(LsmApp *app, LsmDevicePage *page)
{
    LsmNpuInfo *npu = &app->monitor.npus[page->index];
    LsmNpuPageWidgets *widgets = &page->widgets.npu;
    if (page->optional_note)
        gtk_widget_set_visible(page->optional_note,
                               !npu->supported_metrics);
    char bytes[64];
    char metric[64];
    const double utilization = npu->utilization_available
        ? npu->utilization_percent : 0.0;
    const double memory = npu->memory_total_available
        ? npu->memory_percent : 0.0;
    lsm_graph_push(page->graph, utilization, memory, app->runtime.newer_on_right);
    lsm_graph_push(page->side_graph, utilization, 0.0, app->runtime.newer_on_right);
    if (npu->utilization_available) {
        if (npu->utilization_percent < 0.5) {
            lsm_ui_set_label_text(page->button_value, "Idle");
            lsm_ui_set_label_text(widgets->activity, "Idle");
        } else {
            lsm_ui_set_label_text(page->button_value, "%.0f%% active",
                                  npu->utilization_percent);
            lsm_ui_set_label_text(widgets->activity, "%.0f%% active",
                                  npu->utilization_percent);
        }
    } else {
        lsm_ui_set_label_text(page->button_value, "Detected");
        lsm_ui_set_label_text(widgets->activity, "N/A");
    }
    lsm_ui_set_label_text(widgets->product, "%s", npu->name);
    if (npu->memory_used_available) {
        lsm_format_bytes(npu->memory_used_bytes, bytes, sizeof(bytes));
        if (npu->memory_total_available)
            lsm_ui_set_label_text(widgets->memory_used_live, "%s (%.0f%%)", bytes,
                                  npu->memory_percent);
        else
            lsm_ui_set_label_text(widgets->memory_used_live, "%s", bytes);
    } else {
        lsm_ui_set_label_text(widgets->memory_used_live, "N/A");
    }
    lsm_ui_set_label_text(widgets->temperature, "%s",
        lsm_metric_format_celsius(npu->temperature_available, npu->temperature_c,
                                  metric, sizeof(metric)));
    if (npu->clock_available && isfinite(npu->clock_mhz)) {
        if (npu->clock_mhz <= 0.0 && npu->utilization_available &&
            npu->utilization_percent < 0.5)
            lsm_ui_set_label_text(widgets->clock, "0 MHz (idle)");
        else
            lsm_ui_set_label_text(widgets->clock, "%s",
                lsm_metric_format_mhz(true, npu->clock_mhz,
                                      metric, sizeof(metric)));
    } else {
        lsm_ui_set_label_text(widgets->clock, "N/A");
    }
    if (npu->memory_used_available)
        lsm_ui_set_label_text(widgets->memory_used, "%s",
                           lsm_format_bytes(npu->memory_used_bytes, bytes, sizeof(bytes)));
    else
        lsm_ui_set_label_text(widgets->memory_used, "N/A");
    if (npu->memory_total_available)
        lsm_ui_set_label_text(widgets->memory_total, "%s",
                           lsm_format_bytes(npu->memory_total_bytes, bytes, sizeof(bytes)));
    else
        lsm_ui_set_label_text(widgets->memory_total, "N/A");
    lsm_ui_set_label_text(widgets->driver, "%s",
                          npu->driver[0] ? npu->driver : "N/A");
    lsm_ui_set_label_text(widgets->metrics, "%s",
                          npu->metrics_source[0]
                              ? npu->metrics_source
                              : "Identification only");
    lsm_ui_set_label_text(widgets->power, "%s",
        lsm_metric_format_watts(npu->power_available, npu->power_watts,
                                metric, sizeof(metric)));
    lsm_ui_set_label_text(widgets->device, "%s",
                          npu->device_identifier[0]
                              ? npu->device_identifier
                              : "N/A");
}

void performance_present_device_page(LsmApp *app, LsmDevicePage *page)
{
    if (!app || !page) return;
    switch (page->type) {
        case LSM_PAGE_BLUETOOTH: update_bluetooth_page(app, page); break;
        case LSM_PAGE_GPU: update_gpu_page(app, page); break;
        case LSM_PAGE_BATTERY: update_battery_page(app, page); break;
        case LSM_PAGE_NPU: update_npu_page(app, page); break;
        case LSM_PAGE_CPU:
        case LSM_PAGE_MEMORY:
        case LSM_PAGE_DISK:
        case LSM_PAGE_NETWORK:
        case LSM_PAGE_COUNT:
            break;
    }
}
