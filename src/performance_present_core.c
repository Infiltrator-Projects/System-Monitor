// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_present_core.c
 * @brief CPU, memory, disk and network snapshot presentation.
 *
 * Presentation consumes retained plain-C monitor state only. Hardware discovery
 * and sampling remain below the presentation boundary.
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
    char metric[64];
    lsm_ui_set_label_text(page->subtitle, "%s",
                          cpu->model[0] ? cpu->model : "N/A");
    lsm_graph_push(page->graph, cpu->usage_percent, 0.0, app->runtime.newer_on_right);
    lsm_graph_push(page->side_graph, cpu->usage_percent, 0.0, app->runtime.newer_on_right);
    if (cpu->frequency_ghz > 0.0)
        lsm_ui_set_label_text(page->button_value, "%.0f%% %.2f GHz",
                              cpu->usage_percent, cpu->frequency_ghz);
    else
        lsm_ui_set_label_text(page->button_value, "%.0f%% N/A",
                              cpu->usage_percent);
    lsm_ui_set_label_text(widgets->utilisation, "%.0f%%", cpu->usage_percent);
    lsm_ui_set_label_text(widgets->user_time, "%.1f%%", cpu->user_percent);
    lsm_ui_set_label_text(widgets->kernel_time, "%.1f%%",
                          cpu->kernel_percent);
    lsm_ui_set_label_text(widgets->speed, "%s",
        lsm_metric_format_ghz(cpu->frequency_ghz > 0.0,
                              cpu->frequency_ghz, metric, sizeof(metric)));
    lsm_ui_set_label_text(widgets->processes, "%u", cpu->process_count);
    lsm_ui_set_label_text(widgets->threads, "%u", cpu->thread_count);
    lsm_ui_set_label_text(widgets->handles, "%llu",
                          (unsigned long long)cpu->file_handle_count);
    infiltratr_format_duration_clock(cpu->uptime_seconds, metric, sizeof(metric));
    lsm_ui_set_label_text(widgets->uptime, "%s", metric);
    lsm_ui_set_label_text(widgets->temperature, "%s",
        lsm_metric_format_celsius(isfinite(cpu->temperature_c),
                                  cpu->temperature_c, metric, sizeof(metric)));
    performance_present_set_temperature_state(widgets->temperature, isfinite(cpu->temperature_c),
                          cpu->temperature_c, 80.0, 95.0);
    set_pressure_text(widgets->pressure, &app->monitor.cpu_pressure);
    lsm_ui_set_label_text(widgets->cores, "%u", cpu->physical_cores);
    lsm_ui_set_label_text(widgets->logical_processors, "%u",
                          cpu->logical_cores);
    lsm_ui_set_label_text(widgets->base_speed, "%s",
        lsm_metric_format_ghz(cpu->base_frequency_ghz > 0.0,
                              cpu->base_frequency_ghz, metric,
                              sizeof(metric)));
    lsm_ui_set_label_text(widgets->maximum_speed, "%s",
        lsm_metric_format_ghz(cpu->max_frequency_ghz > 0.0,
                              cpu->max_frequency_ghz, metric,
                              sizeof(metric)));
    lsm_ui_set_label_text(widgets->virtualisation, "%s",
                          cpu->virtualization ? "Enabled" : "Disabled");
    lsm_ui_set_label_text(widgets->cache_l1, "%s", cpu->cache_l1);
    lsm_ui_set_label_text(widgets->cache_l2, "%s", cpu->cache_l2);
    lsm_ui_set_label_text(widgets->cache_l3, "%s", cpu->cache_l3);
    lsm_ui_set_label_text(widgets->load_average, "%.2f  %.2f  %.2f",
                          cpu->load_average_1, cpu->load_average_5,
                          cpu->load_average_15);
    lsm_ui_set_label_text(widgets->sockets, "%u", cpu->socket_count);
    lsm_ui_set_label_text(widgets->numa_nodes, "%u", cpu->numa_node_count);
    lsm_ui_set_label_text(widgets->interrupts, "%.0f",
                          cpu->interrupts_per_sec);
    lsm_ui_set_label_text(widgets->context_switches, "%.0f",
                          cpu->context_switches_per_sec);
    for (unsigned i = 0; i < cpu->logical_cores; i++) {
        lsm_graph_push(app->performance.cpu_core_graphs[i], cpu->core_usage[i], 0.0,
                       app->runtime.newer_on_right);
        lsm_ui_set_label_text(app->performance.cpu_core_labels[i], "CPU %u — %.0f%%",
                           i, cpu->core_usage[i]);
    }
}

static void update_memory_page(LsmApp *app, LsmDevicePage *page)
{
    LsmMemoryInfo *memory = &app->monitor.memory;
    LsmMemoryPageWidgets *widgets = &page->widgets.memory;
    char a[64], b[64];
    lsm_graph_push(page->graph, memory->usage_percent, 0.0, app->runtime.newer_on_right);
    lsm_graph_push(page->side_graph, memory->usage_percent, 0.0, app->runtime.newer_on_right);
    lsm_format_bytes(memory->used_bytes, a, sizeof(a));
    lsm_format_bytes(memory->total_bytes, b, sizeof(b));
    lsm_ui_set_label_text(page->button_value, "%s/%s (%.0f%%)", a, b,
                          memory->usage_percent);
    lsm_metric_format_memory_gb(memory->total_bytes, a, sizeof(a));
    lsm_ui_set_label_text(page->subtitle, "%s", a);

    lsm_metric_format_memory_gb(memory->used_bytes, a, sizeof(a));
    lsm_ui_set_label_text(widgets->in_use, "%s", a);
    lsm_metric_format_memory_gb(memory->available_bytes, a, sizeof(a));
    lsm_ui_set_label_text(widgets->available, "%s", a);
    lsm_format_bytes(memory->committed_bytes, a, sizeof(a));
    lsm_format_bytes(memory->commit_limit_bytes, b, sizeof(b));
    lsm_ui_set_label_text(widgets->committed, "%s/%s", a, b);
    lsm_metric_format_memory_gb(memory->buffers_bytes, a, sizeof(a));
    lsm_ui_set_label_text(widgets->buffers, "%s", a);
    lsm_metric_format_memory_gb(memory->cached_bytes, a, sizeof(a));
    lsm_ui_set_label_text(widgets->cached, "%s", a);
    snprintf(a, sizeof(a), "%.1Lf/%.1Lf GB",
             (long double)memory->swap_used_bytes / 1073741824.0L,
             (long double)memory->swap_total_bytes / 1073741824.0L);
    lsm_ui_set_label_text(widgets->swap, "%s", a);
    lsm_format_bytes(memory->kernel_reclaimable_bytes, a, sizeof(a));
    lsm_ui_set_label_text(widgets->kernel_reclaimable, "%s", a);
    lsm_format_bytes(memory->kernel_nonreclaimable_bytes, a, sizeof(a));
    lsm_ui_set_label_text(widgets->kernel_nonreclaimable, "%s", a);
    lsm_format_bytes(memory->page_tables_bytes, a, sizeof(a));
    lsm_ui_set_label_text(widgets->page_tables, "%s", a);
    set_pressure_text(widgets->pressure, &app->monitor.memory_pressure);
    lsm_ui_set_label_text(widgets->speed, "%s",
        lsm_metric_format_mhz(memory->speed_mhz > 0U,
                              (double)memory->speed_mhz, a, sizeof(a)));
    if (memory->slots_total > 0)
        lsm_ui_set_label_text(widgets->slots_used, "%u of %u",
                              memory->slots_used, memory->slots_total);
    else
        lsm_ui_set_label_text(widgets->slots_used, "N/A");
    lsm_ui_set_label_text(widgets->form_factor, "%s",
                       memory->form_factor[0] ? memory->form_factor : "N/A");
    lsm_ui_set_label_text(widgets->hardware_corrupted, "%s",
                       lsm_format_bytes(memory->hardware_corrupted_bytes,
                                        a, sizeof(a)));
    if (memory->module_details_available && memory->module_count > 0U) {
        GString *modules = g_string_new(NULL);
        for (size_t index = 0U; index < memory->module_count; index++) {
            const LsmMemoryModuleInfo *module = &memory->modules[index];
            char size[64];
            lsm_format_bytes(module->size_bytes, size, sizeof(size));
            if (index > 0U) g_string_append_printf(modules, "\n");
            g_string_append_printf(
                modules, "%s — %s %s, ",
                module->locator[0] ? module->locator : "Module",
                size, module->memory_type[0] ? module->memory_type : "N/A");
            if (module->speed_mhz > 0U)
                g_string_append_printf(modules, "%u MHz", module->speed_mhz);
            else
                g_string_append(modules, "N/A");
            g_string_append_printf(modules, ", %s %s, S/N %s",
                module->manufacturer[0] ? module->manufacturer : "N/A",
                module->part_number[0] ? module->part_number : "N/A",
                module->serial_number[0]
                    ? module->serial_number : "N/A");
        }
        lsm_ui_set_label_text(widgets->modules, "%s", modules->str);
        g_string_free(modules, TRUE);
    } else {
        lsm_ui_set_label_text(widgets->modules, "N/A");
    }
    if (page->composition_area) gtk_widget_queue_draw(page->composition_area);
}

static void update_disk_page(LsmApp *app, LsmDevicePage *page)
{
    LsmDiskInfo *disk = &app->monitor.disks[page->index];
    LsmDiskPageWidgets *widgets = &page->widgets.disk;
    const double megabyte = 1024.0 * 1024.0;
    const double read_mb = disk->read_bytes_per_sec / megabyte;
    const double write_mb = disk->write_bytes_per_sec / megabyte;
    char total[64], used[96], capacity[64];

    lsm_graph_push(page->graph, disk->active_percent, 0.0, app->runtime.newer_on_right);
    lsm_graph_push(page->secondary_graph, read_mb, write_mb,
                   app->runtime.newer_on_right);
    lsm_graph_push(page->side_graph, disk->active_percent, 0.0, app->runtime.newer_on_right);
    lsm_ui_set_label_text(page->button_value, "%.0f%%", disk->active_percent);
    lsm_ui_set_label_text(widgets->read_speed, "%.1f MB/s", read_mb);
    lsm_ui_set_label_text(widgets->active_time, "%.0f%%",
                          disk->active_percent);
    lsm_ui_set_label_text(widgets->write_speed, "%.1f MB/s", write_mb);
    lsm_ui_set_label_text(widgets->average_response, "%.1f ms",
                          disk->average_response_ms);
    lsm_ui_set_label_text(widgets->queue_length, "%.2f",
                          disk->queue_length);
    lsm_ui_set_label_text(widgets->current_requests, "%u",
                          disk->in_progress_operations);
    set_pressure_text(widgets->io_pressure, &app->monitor.io_pressure);
    lsm_ui_set_label_text(widgets->media_type, "%s",
                          disk->media_type[0] ? disk->media_type : "N/A");
    lsm_ui_set_label_text(widgets->connection_type, "%s",
                          disk->connection_type[0]
                              ? disk->connection_type : "N/A");
    lsm_ui_set_label_text(widgets->system_disk, "%s",
                          disk->system_disk ? "Yes" : "No");
    lsm_ui_set_label_text(widgets->read_total, "%s",
                          lsm_format_bytes(disk->read_bytes_total,
                                           total, sizeof(total)));
    lsm_ui_set_label_text(widgets->write_total, "%s",
                          lsm_format_bytes(disk->write_bytes_total,
                                           used, sizeof(used)));
    lsm_ui_set_label_text(page->scale_label, "%.0f MB/s",
                       lsm_graph_get_maximum(page->secondary_graph));
    lsm_metric_format_disk_capacity(disk->size_bytes, capacity, sizeof(capacity));
    lsm_ui_set_label_text(page->subtitle, "%s — %s", disk->name, capacity);

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
    char receive[64], send[64], total_received[64], total_sent[64];
    char scale[64], mid_scale[64], frequency[64];
    lsm_graph_push(page->graph, net->rx_bytes_per_sec, net->tx_bytes_per_sec,
                   app->runtime.newer_on_right);
    lsm_graph_push(page->side_graph, net->rx_bytes_per_sec, net->tx_bytes_per_sec,
                   app->runtime.newer_on_right);

    lsm_metric_format_network(
        (long double)net->rx_bytes_per_sec, app->runtime.network_use_bits, true,
        receive, sizeof(receive));
    lsm_metric_format_network(
        (long double)net->tx_bytes_per_sec, app->runtime.network_use_bits, true,
        send, sizeof(send));
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

    char compact_rates[64];
    lsm_metric_format_network_pair(
        (long double)net->tx_bytes_per_sec,
        (long double)net->rx_bytes_per_sec,
        app->runtime.network_use_bits, compact_rates, sizeof(compact_rates));
    lsm_ui_set_label_text(page->button_value, "%s", compact_rates);
    lsm_ui_set_label_text(widgets->receive_rate, "%s", receive);
    lsm_ui_set_label_text(widgets->received_total, "%s", total_received);
    lsm_ui_set_label_text(widgets->send_rate, "%s", send);
    lsm_ui_set_label_text(widgets->sent_total, "%s", total_sent);
    lsm_ui_set_label_text(widgets->ipv4, "%s",
                          net->ipv4[0] ? net->ipv4 : "N/A");
    lsm_ui_set_label_text(widgets->ipv6, "%s",
                          net->ipv6[0] ? net->ipv6 : "N/A");
    lsm_ui_set_label_text(widgets->mac, "%s",
                          net->mac[0] ? net->mac : "N/A");
    lsm_ui_set_label_text(widgets->connection_state, "%s",
                          net->connection_state[0]
                              ? net->connection_state : "N/A");
    if (net->utilisation_available)
        lsm_ui_set_label_text(widgets->utilisation, "%.1f%%",
                              net->utilisation_percent);
    else
        lsm_ui_set_label_text(widgets->utilisation, "N/A");
    char link_speed[64];
    lsm_metric_format_link_speed_mbps(net->link_speed_mbps, link_speed,
                                      sizeof(link_speed));
    lsm_ui_set_label_text(widgets->link_speed, "%s", link_speed);
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
            return false;
    }
    return false;
}
