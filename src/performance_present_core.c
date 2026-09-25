// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_present_core.c
 * @brief CPU, memory, disk and network snapshot presentation.
 *
 * Presentation consumes retained plain-C monitor state only. Hardware discovery
 * and sampling remain below the presentation boundary.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_present_internal.h"
#include "performance_internal.h"
#include "performance_view.h"
#include "temporal_presentation.h"

#include "common.h"
#include "metric_format.h"
#include "ui_helpers.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void set_pressure_text(GtkWidget *widget,
                              const LsmPressureInfo *pressure)
{
    if (!widget) return;
    if (!pressure || !pressure->available) {
        lsm_ui_set_label_text(widget, "N/A");
        return;
    }
    if (pressure->full_available)
        lsm_ui_set_label_text(widget, "Some %.1f%%  Full %.1f%%",
                              pressure->some_avg10, pressure->full_avg10);
    else
        lsm_ui_set_label_text(widget, "%.1f%%", pressure->some_avg10);
}



static uint64_t signature_mix_text_field(uint64_t hash, const char *text)
{
    hash = lsm_fnv1a64_mix_text(hash, text);
    return lsm_fnv1a64_mix_byte(hash, 0U);
}

static uint64_t partition_store_signature(const LsmDiskInfo *disk)
{
    uint64_t hash = LSM_FNV1A64_OFFSET_BASIS;
    if (!disk) return hash;
    hash = lsm_fnv1a64_mix_u64_le(hash, (uint64_t)disk->partition_count);
    for (size_t index = 0U; index < disk->partition_count; index++) {
        const LsmPartitionInfo *partition = &disk->partitions[index];
        hash = signature_mix_text_field(hash, partition->device);
        hash = signature_mix_text_field(hash, partition->mount_point);
        hash = signature_mix_text_field(hash, partition->filesystem);
        hash = lsm_fnv1a64_mix_u64_le(hash, partition->total_bytes);
        hash = lsm_fnv1a64_mix_u64_le(hash, partition->used_bytes);
        hash = lsm_fnv1a64_mix_u64_le(hash, partition->used_percent);
        hash = lsm_fnv1a64_mix_u64_le(hash,
            partition->usage_known ? UINT64_C(1) : UINT64_C(0));
    }
    return hash;
}

/* Each updater consumes only the retained snapshot. It may format, graph and
 * hide unavailable fields, but it must never perform hardware discovery. */
static void update_cpu_page(LsmApp *app, LsmDevicePage *page)
{
    LsmCpuInfo *cpu = &app->monitor.cpu;
    LsmCpuPageWidgets *widgets = &page->widgets.cpu;
    LsmCpuPerformanceView view;
    lsm_cpu_performance_view(&app->monitor, &view);
    (void)lsm_temporal_format_elapsed_seconds(
        cpu->uptime_seconds,
        view.metrics[LSM_CPU_METRIC_UPTIME],
        sizeof(view.metrics[LSM_CPU_METRIC_UPTIME]));

    lsm_ui_set_label_text(page->subtitle, "%s", view.subtitle);
    lsm_ui_set_label_text(page->button_value, "%s", view.rail_value);

    GtkWidget *metric_widgets[LSM_CPU_METRIC_COUNT] = {
        widgets->utilisation, widgets->speed,
        widgets->processes, widgets->threads,
        widgets->handles, widgets->uptime,
        widgets->temperature, widgets->pressure,
        widgets->user_time, widgets->kernel_time
    };
    for (size_t index = 0U; index < LSM_CPU_METRIC_COUNT; index++) {
        lsm_ui_set_label_text(
            metric_widgets[index], "%s", view.metrics[index]);
    }

    performance_present_set_temperature_state(
        widgets->temperature,
        cpu->temperature_available && isfinite(cpu->temperature_c),
        cpu->temperature_c, 80.0, 95.0);

    GtkWidget *detail_widgets[LSM_CPU_DETAIL_COUNT] = {
        widgets->cores,
        widgets->logical_processors,
        widgets->base_speed,
        widgets->maximum_speed,
        widgets->virtualisation,
        widgets->cache_l1,
        widgets->cache_l2,
        widgets->cache_l3,
        widgets->load_average,
        widgets->sockets,
        widgets->numa_nodes,
        widgets->interrupts,
        widgets->context_switches
    };
    for (size_t index = 0U; index < LSM_CPU_DETAIL_COUNT; index++) {
        lsm_ui_set_label_text(
            detail_widgets[index], "%s", view.details[index]);
    }

    for (unsigned index = 0U; index < cpu->logical_cores; index++) {
        lsm_ui_set_label_text(
            app->performance.cpu_core_labels[index],
            "CPU %u — %.0f%%", index, cpu->core_usage[index]);
    }
}

static void update_memory_page(LsmApp *app, LsmDevicePage *page)
{
    LsmMemoryPageWidgets *widgets = &page->widgets.memory;
    LsmMemoryPerformanceView view;
    lsm_memory_performance_view(&app->monitor, &view);

    lsm_ui_set_label_text(page->button_value, "%s", view.rail_value);
    lsm_ui_set_label_text(page->subtitle, "%s", view.subtitle);

    GtkWidget *metric_widgets[LSM_MEMORY_METRIC_COUNT] = {
        widgets->in_use,
        widgets->available,
        widgets->committed,
        widgets->cached,
        widgets->buffers,
        widgets->swap,
        widgets->kernel_reclaimable,
        widgets->kernel_nonreclaimable,
        widgets->page_tables,
        widgets->pressure
    };
    for (size_t index = 0U; index < LSM_MEMORY_METRIC_COUNT; index++) {
        lsm_ui_set_label_text(
            metric_widgets[index], "%s", view.metrics[index]);
    }

    GtkWidget *detail_widgets[LSM_MEMORY_DETAIL_COUNT] = {
        widgets->speed,
        widgets->slots_used,
        widgets->form_factor,
        widgets->hardware_corrupted,
        widgets->modules
    };
    for (size_t index = 0U; index < LSM_MEMORY_DETAIL_COUNT; index++) {
        lsm_ui_set_label_text(
            detail_widgets[index], "%s", view.details[index]);
    }

    if (page->composition_area)
        gtk_widget_queue_draw(page->composition_area);
}

static void update_disk_page(LsmApp *app, LsmDevicePage *page)
{
    LsmDiskInfo *disk = &app->monitor.disks[page->index];
    LsmDiskPageWidgets *widgets = &page->widgets.disk;
    LsmDevicePerformanceView view;
    lsm_disk_performance_view(disk, page->index, &view);
    char total[64], used[96];

    lsm_ui_set_label_text(page->button_value, "%s", view.rail_value);
    lsm_ui_set_label_text(
        widgets->read_speed, "%s",
        view.metric_values[LSM_DISK_VIEW_READ_SPEED]);
    lsm_ui_set_label_text(
        widgets->active_time, "%s",
        view.metric_values[LSM_DISK_VIEW_ACTIVE_TIME]);
    lsm_ui_set_label_text(
        widgets->write_speed, "%s",
        view.metric_values[LSM_DISK_VIEW_WRITE_SPEED]);
    lsm_ui_set_label_text(
        widgets->average_response, "%s",
        view.metric_values[LSM_DISK_VIEW_AVERAGE_RESPONSE]);
    lsm_ui_set_label_text(
        widgets->queue_length, "%s",
        view.metric_values[LSM_DISK_VIEW_QUEUE_LENGTH]);
    lsm_ui_set_label_text(widgets->current_requests, "%u",
                          disk->in_progress_operations);
    set_pressure_text(widgets->io_pressure, &app->monitor.io_pressure);
    lsm_ui_set_label_text(
        widgets->media_type, "%s",
        view.metric_values[LSM_DISK_VIEW_MEDIA_TYPE]);
    lsm_ui_set_label_text(
        widgets->connection_type, "%s",
        view.metric_values[LSM_DISK_VIEW_CONNECTION]);
    lsm_ui_set_label_text(
        widgets->system_disk, "%s",
        view.metric_values[LSM_DISK_VIEW_SYSTEM_DISK]);
    lsm_ui_set_label_text(widgets->read_total, "%s",
                          lsm_format_bytes(disk->read_bytes_total,
                                           total, sizeof(total)));
    lsm_ui_set_label_text(widgets->write_total, "%s",
                          lsm_format_bytes(disk->write_bytes_total,
                                           used, sizeof(used)));
    lsm_ui_set_label_text(page->scale_label, "%.0f MB/s",
                       lsm_graph_get_maximum(page->secondary_graph));
    lsm_ui_set_label_text(page->subtitle, "%s", view.subtitle);

    if (page->partition_store) {
        const uint64_t signature = partition_store_signature(disk);
        if (!page->partition_store_signature_valid ||
            page->partition_store_signature != signature) {
            gtk_list_store_clear(page->partition_store);
            for (size_t i = 0; i < disk->partition_count; i++) {
                const LsmPartitionInfo *partition = &disk->partitions[i];
                GtkTreeIter iterator;
                gtk_list_store_append(page->partition_store, &iterator);
                lsm_format_bytes(partition->total_bytes, total, sizeof(total));
                if (partition->usage_known) {
                    char used_size[64];
                    lsm_format_bytes(partition->used_bytes, used_size,
                                     sizeof(used_size));
                    snprintf(used, sizeof(used), "%s (%u%%)", used_size,
                             partition->used_percent);
                } else {
                    snprintf(used, sizeof(used), "N/A");
                }
                gtk_list_store_set(page->partition_store, &iterator,
                    0, partition->device,
                    1, partition->mount_point,
                    2, partition->filesystem,
                    3, total,
                    4, used,
                    -1);
            }
            page->partition_store_signature = signature;
            page->partition_store_signature_valid = TRUE;
        }
    }
}

static void update_network_page(LsmApp *app, LsmDevicePage *page)
{
    LsmNetInfo *net = &app->monitor.nets[page->index];
    LsmNetworkPageWidgets *widgets = &page->widgets.network;
    LsmDevicePerformanceView view;
    lsm_network_performance_view(
        net, page->index, app->runtime.network_use_bits, &view);
    /* Identity comes from the backend snapshot. Raw bus identifiers are
       intentionally not promoted into the product-name position. */
    const char *product = performance_present_preferred_hardware_name(net->product, net->vendor);
    if (strcmp(page->hardware_product, product) != 0) {
        lsm_copy_string(page->hardware_product,
                        sizeof(page->hardware_product), product);
        lsm_ui_set_label_text(widgets->product, "%s", page->hardware_product);
        const char *kind = net->wireless ? "Wi-Fi" : "Ethernet";
        if (strcmp(page->hardware_product, "N/A") == 0)
            lsm_ui_set_label_text(page->button_title, "%s %zu",
                                  kind, page->index);
        else
            lsm_ui_set_label_text(page->button_title, "%s %zu — %s",
                                  kind, page->index, page->hardware_product);
        performance_present_set_large_device_title(page->title, kind, page->index,
                               page->hardware_product);
    }
    if (strcmp(page->hardware_vendor, net->vendor) != 0 && net->vendor[0]) {
        lsm_copy_string(page->hardware_vendor,
                        sizeof(page->hardware_vendor), net->vendor);
        lsm_ui_set_label_text(widgets->vendor, "%s", page->hardware_vendor);
    }
    char total_received[64], total_sent[64];
    char scale[64], mid_scale[64], frequency[64];

    lsm_metric_format_network(
        (long double)net->rx_bytes_total, app->runtime.network_use_bits, false,
        total_received, sizeof(total_received));
    lsm_metric_format_network(
        (long double)net->tx_bytes_total, app->runtime.network_use_bits, false,
        total_sent, sizeof(total_sent));
    const double graph_maximum = lsm_graph_get_maximum(page->graph);
    lsm_metric_format_network(
        (long double)graph_maximum,
        app->runtime.network_use_bits, true, scale, sizeof(scale));
    lsm_metric_format_network(
        (long double)(graph_maximum / 2.0),
        app->runtime.network_use_bits, true, mid_scale, sizeof(mid_scale));

    lsm_ui_set_label_text(page->button_value, "%s", view.rail_value);
    lsm_ui_set_label_text(
        widgets->receive_rate, "%s",
        view.metric_values[LSM_NETWORK_VIEW_RECEIVE]);
    lsm_ui_set_label_text(widgets->received_total, "%s", total_received);
    lsm_ui_set_label_text(
        widgets->send_rate, "%s",
        view.metric_values[LSM_NETWORK_VIEW_SEND]);
    lsm_ui_set_label_text(widgets->sent_total, "%s", total_sent);
    lsm_ui_set_label_text(
        widgets->ipv4, "%s",
        view.metric_values[LSM_NETWORK_VIEW_IPV4]);
    lsm_ui_set_label_text(
        widgets->ipv6, "%s",
        view.metric_values[LSM_NETWORK_VIEW_IPV6]);
    lsm_ui_set_label_text(
        widgets->mac, "%s",
        view.metric_values[LSM_NETWORK_VIEW_MAC]);
    lsm_ui_set_label_text(
        widgets->connection_state, "%s",
        view.metric_values[LSM_NETWORK_VIEW_STATE]);
    lsm_ui_set_label_text(
        widgets->utilisation, "%s",
        view.metric_values[LSM_NETWORK_VIEW_UTILISATION]);
    lsm_ui_set_label_text(
        widgets->link_speed, "%s",
        view.metric_values[LSM_NETWORK_VIEW_LINK_SPEED]);
    if (net->wireless) {
        lsm_ui_set_label_text(widgets->wifi_network, "%s",
                              net->ssid[0] ? net->ssid : "N/A");
        lsm_ui_set_label_text(widgets->signal, "%.0f%%", net->signal_percent);
        lsm_ui_set_label_text(widgets->frequency, "%s",
            lsm_metric_format_mhz(net->frequency_mhz > 0.0,
                                  net->frequency_mhz, frequency,
                                  sizeof(frequency)));
        lsm_ui_set_label_text(widgets->access_point, "%s",
                           net->access_point[0] ? net->access_point : "N/A");
    }
    lsm_ui_set_label_text(page->scale_label, "%s", scale);
    if (widgets->mid_scale)
        lsm_ui_set_label_text(widgets->mid_scale, "%s", mid_scale);
}

bool performance_record_core_page_sample(
    LsmApp *app, LsmDevicePage *page)
{
    if (!app || !page) return false;

    switch (page->type) {
        case LSM_PAGE_CPU: {
            const LsmCpuInfo *cpu = &app->monitor.cpu;
            lsm_graph_push(page->graph, cpu->usage_percent, 0.0,
                           app->runtime.newer_on_right);
            lsm_graph_push(page->side_graph, cpu->usage_percent, 0.0,
                           app->runtime.newer_on_right);
            if (app->performance.cpu_core_graphs) {
                for (unsigned index = 0U;
                     index < cpu->logical_cores; index++) {
                    lsm_graph_push(
                        app->performance.cpu_core_graphs[index],
                        cpu->core_usage[index], 0.0,
                        app->runtime.newer_on_right);
                }
            }
            return true;
        }
        case LSM_PAGE_MEMORY: {
            const LsmMemoryInfo *memory = &app->monitor.memory;
            lsm_graph_push(page->graph, memory->usage_percent, 0.0,
                           app->runtime.newer_on_right);
            lsm_graph_push(page->side_graph, memory->usage_percent, 0.0,
                           app->runtime.newer_on_right);
            return true;
        }
        case LSM_PAGE_DISK: {
            if (page->index >= app->monitor.disk_count) return true;
            const LsmDiskInfo *disk = &app->monitor.disks[page->index];
            const double megabyte = 1024.0 * 1024.0;
            lsm_graph_push(page->graph, disk->active_percent, 0.0,
                           app->runtime.newer_on_right);
            lsm_graph_push(
                page->secondary_graph,
                disk->read_bytes_per_sec / megabyte,
                disk->write_bytes_per_sec / megabyte,
                app->runtime.newer_on_right);
            lsm_graph_push(page->side_graph, disk->active_percent, 0.0,
                           app->runtime.newer_on_right);
            return true;
        }
        case LSM_PAGE_NETWORK: {
            if (page->index >= app->monitor.net_count) return true;
            const LsmNetInfo *net = &app->monitor.nets[page->index];
            lsm_graph_push(
                page->graph, net->rx_bytes_per_sec, net->tx_bytes_per_sec,
                app->runtime.newer_on_right);
            lsm_graph_push(
                page->side_graph, net->rx_bytes_per_sec, net->tx_bytes_per_sec,
                app->runtime.newer_on_right);
            return true;
        }
        case LSM_PAGE_BLUETOOTH:
        case LSM_PAGE_GPU:
        case LSM_PAGE_BATTERY:
        case LSM_PAGE_NPU:
        case LSM_PAGE_COUNT:
            return false;
    }
    return false;
}

bool performance_present_core_page(LsmApp *app, LsmDevicePage *page)
{
    if (!app || !page) return false;
    switch (page->type) {
        case LSM_PAGE_CPU: update_cpu_page(app, page); return true;
        case LSM_PAGE_MEMORY: update_memory_page(app, page); return true;
        case LSM_PAGE_DISK: update_disk_page(app, page); return true;
        case LSM_PAGE_NETWORK: update_network_page(app, page); return true;
        case LSM_PAGE_BLUETOOTH:
        case LSM_PAGE_GPU:
        case LSM_PAGE_BATTERY:
        case LSM_PAGE_NPU:
        case LSM_PAGE_COUNT:
            return false;
    }
    return false;
}
