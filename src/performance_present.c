// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_present.c
 * @brief Performance-page snapshot presentation with no hardware I/O.
 *
 * Page construction remains in performance.c. Keeping presentation here makes
 * refresh policy, formatting and regression testing independent of widget
 * creation and topology reconciliation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_present.h"
#include "performance_internal.h"
#include "app_internal.h"

#include "common.h"
#include "duration_format.h"
#include "metric_format.h"
#include "ui_helpers.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Small format/identity helpers keep page update functions focused on mapping
 * snapshot semantics to widgets rather than repeating display policy. */
static bool useful_hardware_name(const char *name)
{
    return name && name[0] && strcmp(name, "N/A") != 0 &&
           strncmp(name, "PCI ", 4) != 0;
}

static const char *preferred_hardware_name(const char *product,
                                           const char *vendor)
{
    if (useful_hardware_name(product)) return product;
    if (useful_hardware_name(vendor)) return vendor;
    return "N/A";
}

static void set_large_device_title(GtkWidget *label, const char *kind,
                                   size_t index, const char *hardware_name)
{
    if (!label || !kind) return;
    char text[LSM_NAME_LEN + 32];
    if (useful_hardware_name(hardware_name))
        snprintf(text, sizeof(text), "%s %zu — %s", kind, index,
                 hardware_name);
    else
        snprintf(text, sizeof(text), "%s %zu", kind, index);
    if (!lsm_ui_text_needs_update(gtk_label_get_text(GTK_LABEL(label)), text))
        return;
    char *markup = g_markup_printf_escaped(
        "<span size='18000' weight='bold'>%s</span>", text);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    g_free(markup);
}

static bool usable_battery_level(const char *level)
{
    return level && level[0] && strcasecmp(level, "Unknown") != 0;
}

static double coarse_battery_graph_value(const char *level)
{
    if (!usable_battery_level(level)) return NAN;
    if (strcasecmp(level, "Critical") == 0) return 5.0;
    if (strcasecmp(level, "Low") == 0) return 20.0;
    if (strcasecmp(level, "Normal") == 0) return 50.0;
    if (strcasecmp(level, "High") == 0) return 80.0;
    if (strcasecmp(level, "Full") == 0) return 100.0;
    return NAN;
}

static void format_battery_charge(const LsmBatteryInfo *battery,
                                  char *buffer, size_t size)
{
    if (isfinite(battery->capacity_percent))
        snprintf(buffer, size, "%.0f%%", battery->capacity_percent);
    else if (usable_battery_level(battery->capacity_level))
        snprintf(buffer, size, "%s", battery->capacity_level);
    else
        snprintf(buffer, size, "N/A");
}



static uint64_t signature_mix_byte(uint64_t hash, unsigned char value)
{
    hash ^= (uint64_t)value;
    return hash * UINT64_C(1099511628211);
}

static uint64_t signature_mix_text(uint64_t hash, const char *text)
{
    const unsigned char *cursor =
        (const unsigned char *)(text ? text : "");
    while (*cursor) hash = signature_mix_byte(hash, *cursor++);
    return signature_mix_byte(hash, 0U);
}

static uint64_t signature_mix_u64(uint64_t hash, uint64_t value)
{
    for (unsigned shift = 0U; shift < 64U; shift += 8U)
        hash = signature_mix_byte(
            hash, (unsigned char)((value >> shift) & UINT64_C(0xff)));
    return hash;
}

static uint64_t partition_store_signature(const LsmDiskInfo *disk)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    if (!disk) return hash;
    hash = signature_mix_u64(hash, (uint64_t)disk->partition_count);
    for (size_t index = 0U; index < disk->partition_count; index++) {
        const LsmPartitionInfo *partition = &disk->partitions[index];
        hash = signature_mix_text(hash, partition->device);
        hash = signature_mix_text(hash, partition->mount_point);
        hash = signature_mix_text(hash, partition->filesystem);
        hash = signature_mix_u64(hash, partition->total_bytes);
        hash = signature_mix_u64(hash, partition->used_bytes);
        hash = signature_mix_u64(hash, partition->used_percent);
        hash = signature_mix_u64(hash,
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
    if (cpu->frequency_ghz > 0.0)
        lsm_ui_set_label_text(widgets->speed, "%.2f GHz",
                              cpu->frequency_ghz);
    else
        lsm_ui_set_label_text(widgets->speed, "N/A");
    lsm_ui_set_label_text(widgets->processes, "%u", cpu->process_count);
    lsm_ui_set_label_text(widgets->threads, "%u", cpu->thread_count);
    lsm_ui_set_label_text(widgets->handles, "%llu",
                          (unsigned long long)cpu->file_handle_count);
    lsm_duration_format_clock(cpu->uptime_seconds, metric, sizeof(metric));
    lsm_ui_set_label_text(widgets->uptime, "%s", metric);
    lsm_ui_set_label_text(widgets->temperature, "%s",
        lsm_metric_format_celsius(isfinite(cpu->temperature_c),
                                  cpu->temperature_c, metric, sizeof(metric)));
    lsm_ui_set_label_text(widgets->cores, "%u", cpu->physical_cores);
    lsm_ui_set_label_text(widgets->logical_processors, "%u",
                          cpu->logical_cores);
    if (cpu->base_frequency_ghz > 0.0)
        lsm_ui_set_label_text(widgets->base_speed, "%.2f GHz",
                              cpu->base_frequency_ghz);
    else
        lsm_ui_set_label_text(widgets->base_speed, "N/A");
    if (cpu->max_frequency_ghz > 0.0)
        lsm_ui_set_label_text(widgets->maximum_speed, "%.2f GHz",
                              cpu->max_frequency_ghz);
    else
        lsm_ui_set_label_text(widgets->maximum_speed, "N/A");
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
    if (memory->speed_mhz > 0)
        lsm_ui_set_label_text(widgets->speed, "%u MHz", memory->speed_mhz);
    else
        lsm_ui_set_label_text(widgets->speed, "N/A");
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
    const char *product = preferred_hardware_name(net->product, net->vendor);
    if (strcmp(page->hardware_product, product) != 0) {
        g_strlcpy(page->hardware_product, product, sizeof(page->hardware_product));
        lsm_ui_set_label_text(widgets->product, "%s", page->hardware_product);
        const char *kind = net->wireless ? "Wi-Fi" : "Ethernet";
        if (strcmp(page->hardware_product, "N/A") == 0)
            lsm_ui_set_label_text(page->button_title, "%s %zu",
                                  kind, page->index);
        else
            lsm_ui_set_label_text(page->button_title, "%s %zu — %s",
                                  kind, page->index, page->hardware_product);
        set_large_device_title(page->title, kind, page->index,
                               page->hardware_product);
    }
    if (strcmp(page->hardware_vendor, net->vendor) != 0 && net->vendor[0]) {
        g_strlcpy(page->hardware_vendor, net->vendor, sizeof(page->hardware_vendor));
        lsm_ui_set_label_text(widgets->vendor, "%s", page->hardware_vendor);
    }
    char receive[64], send[64], total_received[64], total_sent[64];
    char scale[64], mid_scale[64];
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
        if (net->frequency_mhz > 0.0)
            lsm_ui_set_label_text(widgets->frequency, "%.0f MHz",
                                  net->frequency_mhz);
        else
            lsm_ui_set_label_text(widgets->frequency, "N/A");
        lsm_ui_set_label_text(widgets->access_point, "%s",
                           net->access_point[0] ? net->access_point : "N/A");
    }
    lsm_ui_set_label_text(page->scale_label, "%s", scale);
    if (widgets->mid_scale)
        lsm_ui_set_label_text(widgets->mid_scale, "%s", mid_scale);
}

static void update_bluetooth_page(LsmApp *app, LsmDevicePage *page)
{
    LsmBluetoothInfo *adapter = &app->monitor.bluetooth[page->index];
    LsmBluetoothPageWidgets *widgets = &page->widgets.bluetooth;
    const double connected = (double)adapter->connected_count;

    lsm_graph_push(page->graph, connected, 0.0, app->runtime.newer_on_right);
    lsm_graph_push(page->side_graph, connected, 0.0,
                   app->runtime.newer_on_right);

    lsm_ui_set_label_text(page->button_value, "%u connected",
                          adapter->connected_count);
    lsm_ui_set_label_text(page->scale_label, "BlueZ");
    lsm_ui_set_label_text(widgets->status, "%s",
                          adapter->powered ? "Powered" : "Powered off");
    lsm_ui_set_label_text(widgets->connected, "%u",
                          adapter->connected_count);
    lsm_ui_set_label_text(widgets->known, "%u", adapter->device_count);
    lsm_ui_set_label_text(widgets->paired, "%u", adapter->paired_count);
    lsm_ui_set_label_text(widgets->controller, "%s",
                          adapter->name[0] ? adapter->name : "N/A");
    lsm_ui_set_label_text(widgets->address, "%s",
                          adapter->address[0] ? adapter->address : "N/A");
    lsm_ui_set_label_text(widgets->adapter_name, "%s",
                          adapter->adapter_name[0]
                              ? adapter->adapter_name : "N/A");
    lsm_ui_set_label_text(widgets->alias, "%s",
                          adapter->alias[0] ? adapter->alias : "N/A");
    lsm_ui_set_label_text(widgets->discoverable, "%s",
                          adapter->discoverable ? "Yes" : "No");
    lsm_ui_set_label_text(widgets->pairable, "%s",
                          adapter->pairable ? "Yes" : "No");
    lsm_ui_set_label_text(widgets->discovering, "%s",
                          adapter->discovering ? "Yes" : "No");
    lsm_ui_set_label_text(widgets->trusted, "%u", adapter->trusted_count);
    lsm_ui_set_label_text(widgets->connected_devices, "%s",
                          adapter->connected_devices[0]
                              ? adapter->connected_devices : "None");
    lsm_ui_set_label_text(widgets->product, "%s",
                          adapter->alias[0] ? adapter->alias :
                          (adapter->adapter_name[0]
                               ? adapter->adapter_name : adapter->name));
}

static void update_gpu_page(LsmApp *app, LsmDevicePage *page)
{
    LsmGpuInfo *gpu = &app->monitor.gpus[page->index];
    LsmGpuPageWidgets *widgets = &page->widgets.gpu;
    char a[64];
    const gboolean temperature_available = gpu->temperature_available &&
        isfinite(gpu->temperature_c);
    const char *product = useful_hardware_name(gpu->name) ? gpu->name : "N/A";
    if (strcmp(page->hardware_product, product) != 0) {
        g_strlcpy(page->hardware_product, product, sizeof(page->hardware_product));
        if (strcmp(product, "N/A") == 0)
            lsm_ui_set_label_text(page->button_title, "GPU %zu", page->index);
        else
            lsm_ui_set_label_text(page->button_title, "GPU %zu — %s",
                                  page->index, product);
        set_large_device_title(page->title, "GPU", page->index, product);
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
        if (gpu->memory_clock_available && isfinite(gpu->memory_clock_mhz))
            lsm_ui_set_label_text(widgets->memory_clock, "%.0f MHz",
                                  gpu->memory_clock_mhz);
        else
            lsm_ui_set_label_text(widgets->memory_clock, "N/A");
        if (gpu->power_available && isfinite(gpu->power_watts))
            lsm_ui_set_label_text(widgets->power, "%.1f W", gpu->power_watts);
        else
            lsm_ui_set_label_text(widgets->power, "N/A");
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
            lsm_ui_set_label_text(widgets->clock, "%.0f MHz", npu->clock_mhz);
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


void lsm_performance_present_page(LsmApp *app, LsmDevicePage *page)
{
    if (!app || !page) return;
    switch (page->type) {
        case LSM_PAGE_CPU: update_cpu_page(app, page); break;
        case LSM_PAGE_MEMORY: update_memory_page(app, page); break;
        case LSM_PAGE_DISK: update_disk_page(app, page); break;
        case LSM_PAGE_NETWORK: update_network_page(app, page); break;
        case LSM_PAGE_BLUETOOTH: update_bluetooth_page(app, page); break;
        case LSM_PAGE_GPU: update_gpu_page(app, page); break;
        case LSM_PAGE_BATTERY: update_battery_page(app, page); break;
        case LSM_PAGE_NPU: update_npu_page(app, page); break;
    }
}
