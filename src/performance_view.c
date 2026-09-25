// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_view.c
 * @brief Platform-neutral Performance and cross-tab summary value projection.
 *
 * The implementation uses only plain monitor data plus the portable Infiltratr
 * Common formatting contract. GTK and Win32 render these strings but do not
 * independently reinterpret telemetry availability or units.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_view.h"

#include <infiltratr/core.h>
#include <infiltratr/format.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void copy_or_na(char *destination, size_t size, const char *source)
{
    infiltratr_copy_string(destination, size,
                           source && source[0] ? source : "N/A");
}

static void format_pressure(const LsmPressureInfo *pressure,
                            char *buffer, size_t size)
{
    if (!buffer || size == 0U) return;
    if (!pressure || !pressure->available) {
        infiltratr_copy_string(buffer, size, "N/A");
        return;
    }
    if (pressure->full_available) {
        (void)snprintf(buffer, size, "Some %.1f%%  Full %.1f%%",
                       pressure->some_avg10, pressure->full_avg10);
    } else {
        (void)snprintf(buffer, size, "%.1f%%", pressure->some_avg10);
    }
}

static void format_unsigned(bool available, unsigned value,
                            char *buffer, size_t size)
{
    if (!buffer || size == 0U) return;
    if (!available) {
        infiltratr_copy_string(buffer, size, "N/A");
        return;
    }
    (void)snprintf(buffer, size, "%u", value);
}

static void format_u64(bool available, uint64_t value,
                       char *buffer, size_t size)
{
    if (!buffer || size == 0U) return;
    if (!available) {
        infiltratr_copy_string(buffer, size, "N/A");
        return;
    }
    (void)snprintf(buffer, size, "%llu", (unsigned long long)value);
}

static const InfiltratrScalarFormatOptions one_decimal_percent = {
    .struct_size = sizeof(InfiltratrScalarFormatOptions),
    .abi_version = INFILTRATR_SCALAR_FORMAT_OPTIONS_ABI,
    .decimal_places = 1U,
    .clamp = false,
    .minimum = 0.0L,
    .maximum = 0.0L,
    .prefix = "",
    .suffix = "%",
    .unavailable_text = "N/A"
};

static void append_text(char *buffer, size_t size, size_t *used,
                        const char *format, ...)
{
    if (!buffer || size == 0U || !used || *used >= size) return;
    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(
        buffer + *used, size - *used, format, arguments);
    va_end(arguments);
    if (written < 0) return;
    const size_t count = (size_t)written;
    if (count >= size - *used) {
        *used = size - 1U;
        buffer[*used] = '\0';
        return;
    }
    *used += count;
}

void lsm_summary_performance_view(const LsmMonitor *monitor,
                                  bool network_use_bits,
                                  LsmSummaryPerformanceView *view)
{
    if (!view) return;
    memset(view, 0, sizeof(*view));
    for (size_t index = 0U; index < LSM_SUMMARY_COUNT; index++)
        infiltratr_copy_string(
            view->values[index], sizeof(view->values[index]), "N/A");
    if (!monitor) return;

    infiltratr_format_percent(
        isfinite(monitor->cpu.usage_percent),
        monitor->cpu.usage_percent,
        view->values[LSM_SUMMARY_CPU],
        sizeof(view->values[LSM_SUMMARY_CPU]));
    infiltratr_format_percent(
        isfinite(monitor->memory.usage_percent),
        monitor->memory.usage_percent,
        view->values[LSM_SUMMARY_MEMORY],
        sizeof(view->values[LSM_SUMMARY_MEMORY]));

    bool disk_available = false;
    double disk_peak = 0.0;
    for (size_t index = 0U; index < monitor->disk_count; index++) {
        const double value = monitor->disks[index].active_percent;
        if (!isfinite(value)) continue;
        if (!disk_available || value > disk_peak) disk_peak = value;
        disk_available = true;
    }
    infiltratr_format_percent(
        disk_available, disk_peak,
        view->values[LSM_SUMMARY_DISK],
        sizeof(view->values[LSM_SUMMARY_DISK]));

    if (monitor->net_count > 0U) {
        long double network_rate = 0.0L;
        double network_peak = 0.0;
        bool utilisation_available = false;
        for (size_t index = 0U; index < monitor->net_count; index++) {
            const LsmNetInfo *net = &monitor->nets[index];
            if (isfinite(net->rx_bytes_per_sec) && net->rx_bytes_per_sec > 0.0)
                network_rate += (long double)net->rx_bytes_per_sec;
            if (isfinite(net->tx_bytes_per_sec) && net->tx_bytes_per_sec > 0.0)
                network_rate += (long double)net->tx_bytes_per_sec;
            if (net->utilisation_available &&
                isfinite(net->utilisation_percent) &&
                (!utilisation_available ||
                 net->utilisation_percent > network_peak)) {
                network_peak = net->utilisation_percent;
                utilisation_available = true;
            }
        }

        char rate[64];
        infiltratr_format_network(
            network_rate, network_use_bits, true, rate, sizeof(rate));
        if (utilisation_available && network_peak > 0.0) {
            char percent[32];
            infiltratr_format_percent(
                true, network_peak, percent, sizeof(percent));
            (void)snprintf(
                view->values[LSM_SUMMARY_NETWORK],
                sizeof(view->values[LSM_SUMMARY_NETWORK]),
                "%s (%s)", rate, percent);
        } else {
            infiltratr_copy_string(
                view->values[LSM_SUMMARY_NETWORK],
                sizeof(view->values[LSM_SUMMARY_NETWORK]), rate);
        }
    }

    bool gpu_available = false;
    double gpu_peak = 0.0;
    for (size_t index = 0U; index < monitor->gpu_count; index++) {
        const LsmGpuInfo *gpu = &monitor->gpus[index];
        if (!gpu->utilization_available ||
            !isfinite(gpu->utilization_percent))
            continue;
        if (!gpu_available || gpu->utilization_percent > gpu_peak)
            gpu_peak = gpu->utilization_percent;
        gpu_available = true;
    }
    infiltratr_format_percent(
        gpu_available, gpu_peak,
        view->values[LSM_SUMMARY_GPU],
        sizeof(view->values[LSM_SUMMARY_GPU]));
}

void lsm_cpu_performance_view(const LsmMonitor *monitor,
                              LsmCpuPerformanceView *view)
{
    if (!view) return;
    memset(view, 0, sizeof(*view));
    if (!monitor) {
        copy_or_na(view->subtitle, sizeof(view->subtitle), NULL);
        copy_or_na(view->rail_value, sizeof(view->rail_value), NULL);
        for (size_t index = 0U; index < LSM_CPU_METRIC_COUNT; index++)
            copy_or_na(view->metrics[index], sizeof(view->metrics[index]), NULL);
        for (size_t index = 0U; index < LSM_CPU_DETAIL_COUNT; index++)
            copy_or_na(view->details[index], sizeof(view->details[index]), NULL);
        return;
    }

    const LsmCpuInfo *cpu = &monitor->cpu;
    copy_or_na(view->subtitle, sizeof(view->subtitle), cpu->model);

    char percent[32];
    char speed[64];
    infiltratr_format_percent(
        true, cpu->usage_percent, percent, sizeof(percent));
    infiltratr_format_ghz(
        cpu->frequency_ghz > 0.0, cpu->frequency_ghz,
        speed, sizeof(speed));
    (void)snprintf(
        view->rail_value, sizeof(view->rail_value),
        "%s %s", percent, speed);

    infiltratr_copy_string(
        view->metrics[LSM_CPU_METRIC_UTILISATION],
        sizeof(view->metrics[LSM_CPU_METRIC_UTILISATION]), percent);
    infiltratr_copy_string(
        view->metrics[LSM_CPU_METRIC_SPEED],
        sizeof(view->metrics[LSM_CPU_METRIC_SPEED]), speed);
    format_unsigned(
        true, cpu->process_count,
        view->metrics[LSM_CPU_METRIC_PROCESSES],
        sizeof(view->metrics[LSM_CPU_METRIC_PROCESSES]));
    format_unsigned(
        true, cpu->thread_count,
        view->metrics[LSM_CPU_METRIC_THREADS],
        sizeof(view->metrics[LSM_CPU_METRIC_THREADS]));
    format_u64(
        true, cpu->file_handle_count,
        view->metrics[LSM_CPU_METRIC_HANDLES],
        sizeof(view->metrics[LSM_CPU_METRIC_HANDLES]));
    infiltratr_format_duration_clock(
        cpu->uptime_seconds,
        view->metrics[LSM_CPU_METRIC_UPTIME],
        sizeof(view->metrics[LSM_CPU_METRIC_UPTIME]));
    infiltratr_format_celsius(
        cpu->temperature_available && isfinite(cpu->temperature_c),
        cpu->temperature_c,
        view->metrics[LSM_CPU_METRIC_TEMPERATURE],
        sizeof(view->metrics[LSM_CPU_METRIC_TEMPERATURE]));
    format_pressure(
        &monitor->cpu_pressure,
        view->metrics[LSM_CPU_METRIC_PRESSURE],
        sizeof(view->metrics[LSM_CPU_METRIC_PRESSURE]));
    (void)infiltratr_format_scalar(
        true, cpu->user_percent, &one_decimal_percent,
        view->metrics[LSM_CPU_METRIC_USER],
        sizeof(view->metrics[LSM_CPU_METRIC_USER]));
    (void)infiltratr_format_scalar(
        true, cpu->kernel_percent, &one_decimal_percent,
        view->metrics[LSM_CPU_METRIC_KERNEL],
        sizeof(view->metrics[LSM_CPU_METRIC_KERNEL]));

    format_unsigned(
        cpu->physical_cores > 0U, cpu->physical_cores,
        view->details[LSM_CPU_DETAIL_CORES],
        sizeof(view->details[LSM_CPU_DETAIL_CORES]));
    format_unsigned(
        cpu->logical_cores > 0U, cpu->logical_cores,
        view->details[LSM_CPU_DETAIL_LOGICAL_PROCESSORS],
        sizeof(view->details[LSM_CPU_DETAIL_LOGICAL_PROCESSORS]));
    infiltratr_format_ghz(
        cpu->base_frequency_ghz > 0.0, cpu->base_frequency_ghz,
        view->details[LSM_CPU_DETAIL_BASE_SPEED],
        sizeof(view->details[LSM_CPU_DETAIL_BASE_SPEED]));
    infiltratr_format_ghz(
        cpu->max_frequency_ghz > 0.0, cpu->max_frequency_ghz,
        view->details[LSM_CPU_DETAIL_MAXIMUM_SPEED],
        sizeof(view->details[LSM_CPU_DETAIL_MAXIMUM_SPEED]));
    infiltratr_copy_string(
        view->details[LSM_CPU_DETAIL_VIRTUALISATION],
        sizeof(view->details[LSM_CPU_DETAIL_VIRTUALISATION]),
        cpu->virtualization_available
            ? (cpu->virtualization ? "Enabled" : "Disabled") : "N/A");
    copy_or_na(
        view->details[LSM_CPU_DETAIL_CACHE_L1],
        sizeof(view->details[LSM_CPU_DETAIL_CACHE_L1]), cpu->cache_l1);
    copy_or_na(
        view->details[LSM_CPU_DETAIL_CACHE_L2],
        sizeof(view->details[LSM_CPU_DETAIL_CACHE_L2]), cpu->cache_l2);
    copy_or_na(
        view->details[LSM_CPU_DETAIL_CACHE_L3],
        sizeof(view->details[LSM_CPU_DETAIL_CACHE_L3]), cpu->cache_l3);
    if (cpu->load_average_available) {
        (void)snprintf(
            view->details[LSM_CPU_DETAIL_LOAD_AVERAGE],
            sizeof(view->details[LSM_CPU_DETAIL_LOAD_AVERAGE]),
            "%.2f  %.2f  %.2f", cpu->load_average_1,
            cpu->load_average_5, cpu->load_average_15);
    } else {
        infiltratr_copy_string(
            view->details[LSM_CPU_DETAIL_LOAD_AVERAGE],
            sizeof(view->details[LSM_CPU_DETAIL_LOAD_AVERAGE]), "N/A");
    }
    format_unsigned(
        cpu->socket_count > 0U, cpu->socket_count,
        view->details[LSM_CPU_DETAIL_SOCKETS],
        sizeof(view->details[LSM_CPU_DETAIL_SOCKETS]));
    format_unsigned(
        cpu->numa_node_count > 0U, cpu->numa_node_count,
        view->details[LSM_CPU_DETAIL_NUMA_NODES],
        sizeof(view->details[LSM_CPU_DETAIL_NUMA_NODES]));
    if (cpu->interrupts_per_sec_available) {
        (void)snprintf(
            view->details[LSM_CPU_DETAIL_INTERRUPTS],
            sizeof(view->details[LSM_CPU_DETAIL_INTERRUPTS]),
            "%.0f", cpu->interrupts_per_sec);
    } else {
        infiltratr_copy_string(
            view->details[LSM_CPU_DETAIL_INTERRUPTS],
            sizeof(view->details[LSM_CPU_DETAIL_INTERRUPTS]), "N/A");
    }
    if (cpu->context_switches_per_sec_available) {
        (void)snprintf(
            view->details[LSM_CPU_DETAIL_CONTEXT_SWITCHES],
            sizeof(view->details[LSM_CPU_DETAIL_CONTEXT_SWITCHES]),
            "%.0f", cpu->context_switches_per_sec);
    } else {
        infiltratr_copy_string(
            view->details[LSM_CPU_DETAIL_CONTEXT_SWITCHES],
            sizeof(view->details[LSM_CPU_DETAIL_CONTEXT_SWITCHES]), "N/A");
    }
}

void lsm_memory_performance_view(const LsmMonitor *monitor,
                                 LsmMemoryPerformanceView *view)
{
    if (!view) return;
    memset(view, 0, sizeof(*view));
    if (!monitor) {
        copy_or_na(view->subtitle, sizeof(view->subtitle), NULL);
        copy_or_na(view->rail_value, sizeof(view->rail_value), NULL);
        for (size_t index = 0U; index < LSM_MEMORY_METRIC_COUNT; index++)
            copy_or_na(view->metrics[index], sizeof(view->metrics[index]), NULL);
        for (size_t index = 0U; index < LSM_MEMORY_DETAIL_COUNT; index++)
            copy_or_na(view->details[index], sizeof(view->details[index]), NULL);
        return;
    }

    const LsmMemoryInfo *memory = &monitor->memory;
    char used[64];
    char total[64];
    infiltratr_format_bytes(memory->used_bytes, used, sizeof(used));
    infiltratr_format_bytes(memory->total_bytes, total, sizeof(total));
    (void)snprintf(
        view->rail_value, sizeof(view->rail_value),
        "%s/%s (%.0f%%)", used, total, memory->usage_percent);
    infiltratr_format_memory_gb(
        memory->total_bytes, view->subtitle, sizeof(view->subtitle));

    infiltratr_format_memory_gb(
        memory->used_bytes,
        view->metrics[LSM_MEMORY_METRIC_IN_USE],
        sizeof(view->metrics[LSM_MEMORY_METRIC_IN_USE]));
    infiltratr_format_memory_gb(
        memory->available_bytes,
        view->metrics[LSM_MEMORY_METRIC_AVAILABLE],
        sizeof(view->metrics[LSM_MEMORY_METRIC_AVAILABLE]));

    char committed[64];
    char commit_limit[64];
    infiltratr_format_bytes(
        memory->committed_bytes, committed, sizeof(committed));
    infiltratr_format_bytes(
        memory->commit_limit_bytes, commit_limit, sizeof(commit_limit));
    (void)snprintf(
        view->metrics[LSM_MEMORY_METRIC_COMMITTED],
        sizeof(view->metrics[LSM_MEMORY_METRIC_COMMITTED]),
        "%s/%s", committed, commit_limit);
    infiltratr_format_memory_gb(
        memory->cached_bytes,
        view->metrics[LSM_MEMORY_METRIC_CACHED],
        sizeof(view->metrics[LSM_MEMORY_METRIC_CACHED]));
    infiltratr_format_memory_gb(
        memory->buffers_bytes,
        view->metrics[LSM_MEMORY_METRIC_BUFFERS],
        sizeof(view->metrics[LSM_MEMORY_METRIC_BUFFERS]));
    (void)snprintf(
        view->metrics[LSM_MEMORY_METRIC_SWAP],
        sizeof(view->metrics[LSM_MEMORY_METRIC_SWAP]),
        "%.1Lf/%.1Lf GB",
        (long double)memory->swap_used_bytes / 1073741824.0L,
        (long double)memory->swap_total_bytes / 1073741824.0L);
    infiltratr_format_bytes(
        memory->kernel_reclaimable_bytes,
        view->metrics[LSM_MEMORY_METRIC_KERNEL_RECLAIMABLE],
        sizeof(view->metrics[LSM_MEMORY_METRIC_KERNEL_RECLAIMABLE]));
    infiltratr_format_bytes(
        memory->kernel_nonreclaimable_bytes,
        view->metrics[LSM_MEMORY_METRIC_KERNEL_NONRECLAIMABLE],
        sizeof(view->metrics[LSM_MEMORY_METRIC_KERNEL_NONRECLAIMABLE]));
    infiltratr_format_bytes(
        memory->page_tables_bytes,
        view->metrics[LSM_MEMORY_METRIC_PAGE_TABLES],
        sizeof(view->metrics[LSM_MEMORY_METRIC_PAGE_TABLES]));
    format_pressure(
        &monitor->memory_pressure,
        view->metrics[LSM_MEMORY_METRIC_PRESSURE],
        sizeof(view->metrics[LSM_MEMORY_METRIC_PRESSURE]));

    infiltratr_format_mhz(
        memory->speed_mhz > 0U, (double)memory->speed_mhz,
        view->details[LSM_MEMORY_DETAIL_SPEED],
        sizeof(view->details[LSM_MEMORY_DETAIL_SPEED]));
    if (memory->slots_total > 0U) {
        (void)snprintf(
            view->details[LSM_MEMORY_DETAIL_SLOTS_USED],
            sizeof(view->details[LSM_MEMORY_DETAIL_SLOTS_USED]),
            "%u of %u", memory->slots_used, memory->slots_total);
    } else {
        infiltratr_copy_string(
            view->details[LSM_MEMORY_DETAIL_SLOTS_USED],
            sizeof(view->details[LSM_MEMORY_DETAIL_SLOTS_USED]), "N/A");
    }
    copy_or_na(
        view->details[LSM_MEMORY_DETAIL_FORM_FACTOR],
        sizeof(view->details[LSM_MEMORY_DETAIL_FORM_FACTOR]),
        memory->form_factor);
    infiltratr_format_bytes(
        memory->hardware_corrupted_bytes,
        view->details[LSM_MEMORY_DETAIL_HARDWARE_CORRUPTED],
        sizeof(view->details[LSM_MEMORY_DETAIL_HARDWARE_CORRUPTED]));

    if (!memory->module_details_available || memory->module_count == 0U) {
        infiltratr_copy_string(
            view->details[LSM_MEMORY_DETAIL_INSTALLED_MODULES],
            sizeof(view->details[LSM_MEMORY_DETAIL_INSTALLED_MODULES]), "N/A");
        return;
    }

    char *modules =
        view->details[LSM_MEMORY_DETAIL_INSTALLED_MODULES];
    const size_t modules_size =
        sizeof(view->details[LSM_MEMORY_DETAIL_INSTALLED_MODULES]);
    size_t used_length = 0U;
    modules[0] = '\0';
    for (size_t index = 0U; index < memory->module_count; index++) {
        const LsmMemoryModuleInfo *module = &memory->modules[index];
        char module_size[64];
        infiltratr_format_bytes(
            module->size_bytes, module_size, sizeof(module_size));
        if (index > 0U)
            append_text(modules, modules_size, &used_length, "\n");
        append_text(
            modules, modules_size, &used_length,
            "%s — %s %s, ",
            module->locator[0] ? module->locator : "Module",
            module_size,
            module->memory_type[0] ? module->memory_type : "N/A");
        if (module->speed_mhz > 0U) {
            append_text(
                modules, modules_size, &used_length,
                "%u MHz", module->speed_mhz);
        } else {
            append_text(modules, modules_size, &used_length, "N/A");
        }
        append_text(
            modules, modules_size, &used_length,
            ", %s %s, S/N %s",
            module->manufacturer[0] ? module->manufacturer : "N/A",
            module->part_number[0] ? module->part_number : "N/A",
            module->serial_number[0] ? module->serial_number : "N/A");
    }
}


static void device_view_unavailable(
    const char *title, LsmDevicePerformanceView *view)
{
    if (!view) return;
    memset(view, 0, sizeof(*view));
    copy_or_na(view->title, sizeof(view->title), title);
    infiltratr_copy_string(
        view->subtitle, sizeof(view->subtitle), "No device available");
    infiltratr_copy_string(
        view->rail_value, sizeof(view->rail_value), "N/A");
}

static void device_view_metric(
    LsmDevicePerformanceView *view, const char *label, const char *value)
{
    if (!view || view->metric_count >= LSM_DEVICE_PERFORMANCE_METRIC_COUNT)
        return;
    const size_t index = view->metric_count++;
    infiltratr_copy_string(
        view->metric_labels[index], sizeof(view->metric_labels[index]),
        label ? label : "");
    infiltratr_copy_string(
        view->metric_values[index], sizeof(view->metric_values[index]),
        value && value[0] ? value : "N/A");
}

void lsm_disk_performance_view(const LsmDiskInfo *disk, size_t index,
                               LsmDevicePerformanceView *view)
{
    if (!view) return;
    if (!disk) {
        char fallback[64];
        (void)snprintf(fallback, sizeof(fallback), "Disk %zu", index);
        device_view_unavailable(fallback, view);
        return;
    }

    memset(view, 0, sizeof(*view));
    if (disk->model[0]) {
        (void)snprintf(
            view->title, sizeof(view->title),
            "Disk %zu — %.96s", index, disk->model);
    } else {
        (void)snprintf(
            view->title, sizeof(view->title), "Disk %zu", index);
    }

    char capacity[64];
    infiltratr_format_disk_capacity(
        disk->size_bytes, capacity, sizeof(capacity));
    (void)snprintf(
        view->subtitle, sizeof(view->subtitle),
        "%s — %s", disk->name[0] ? disk->name : "Disk", capacity);
    (void)snprintf(
        view->rail_value, sizeof(view->rail_value),
        "%.0f%%", disk->active_percent);

    char value[128];
    (void)snprintf(
        value, sizeof(value), "%.1f MB/s",
        disk->read_bytes_per_sec / (1024.0 * 1024.0));
    device_view_metric(view, "Read speed", value);
    (void)snprintf(
        value, sizeof(value), "%.1f MB/s",
        disk->write_bytes_per_sec / (1024.0 * 1024.0));
    device_view_metric(view, "Write speed", value);
    (void)snprintf(value, sizeof(value), "%.0f%%", disk->active_percent);
    device_view_metric(view, "Active time", value);
    (void)snprintf(
        value, sizeof(value), "%.1f ms", disk->average_response_ms);
    device_view_metric(view, "Average response", value);
    (void)snprintf(value, sizeof(value), "%.2f", disk->queue_length);
    device_view_metric(view, "Queue length", value);
    device_view_metric(view, "Capacity", capacity);
    device_view_metric(
        view, "Media type",
        disk->media_type[0] ? disk->media_type : "N/A");
    device_view_metric(
        view, "Connection",
        disk->connection_type[0] ? disk->connection_type : "N/A");
    device_view_metric(
        view, "System disk", disk->system_disk ? "Yes" : "No");
}

void lsm_network_performance_view(const LsmNetInfo *net, size_t index,
                                  bool use_bits,
                                  LsmDevicePerformanceView *view)
{
    if (!view) return;
    const char *kind = net && net->wireless ? "Wi-Fi" : "Ethernet";
    if (!net) {
        char fallback[64];
        (void)snprintf(fallback, sizeof(fallback), "%s %zu", kind, index);
        device_view_unavailable(fallback, view);
        return;
    }

    memset(view, 0, sizeof(*view));
    if (net->product[0]) {
        (void)snprintf(
            view->title, sizeof(view->title),
            "%s %zu — %.88s", kind, index, net->product);
    } else {
        (void)snprintf(
            view->title, sizeof(view->title), "%s %zu", kind, index);
    }
    (void)snprintf(
        view->subtitle, sizeof(view->subtitle),
        "%s — %s",
        net->name[0] ? net->name : kind,
        net->connection_state[0] ? net->connection_state : "N/A");

    infiltratr_format_network_pair(
        (long double)net->tx_bytes_per_sec,
        (long double)net->rx_bytes_per_sec,
        use_bits, view->rail_value, sizeof(view->rail_value));

    char value[128];
    infiltratr_format_network(
        (long double)net->rx_bytes_per_sec,
        use_bits, true, value, sizeof(value));
    device_view_metric(view, "Receive", value);
    infiltratr_format_network(
        (long double)net->tx_bytes_per_sec,
        use_bits, true, value, sizeof(value));
    device_view_metric(view, "Send", value);
    infiltratr_format_link_speed_mbps(
        net->link_speed_mbps, value, sizeof(value));
    device_view_metric(view, "Link speed", value);
    infiltratr_format_percent(
        net->utilisation_available, net->utilisation_percent,
        value, sizeof(value));
    device_view_metric(view, "Utilisation", value);
    device_view_metric(view, "IPv4 address", net->ipv4);
    device_view_metric(view, "IPv6 address", net->ipv6);
    device_view_metric(view, "MAC address", net->mac);
    device_view_metric(view, "State", net->connection_state);
    device_view_metric(view, "Adapter", net->product);
}

void lsm_gpu_performance_view(const LsmGpuInfo *gpu, size_t index,
                              LsmDevicePerformanceView *view)
{
    if (!view) return;
    if (!gpu) {
        char fallback[64];
        (void)snprintf(fallback, sizeof(fallback), "GPU %zu", index);
        device_view_unavailable(fallback, view);
        return;
    }

    memset(view, 0, sizeof(*view));
    if (gpu->name[0]) {
        (void)snprintf(
            view->title, sizeof(view->title),
            "GPU %zu — %.96s", index, gpu->name);
    } else {
        (void)snprintf(view->title, sizeof(view->title), "GPU %zu", index);
    }
    infiltratr_copy_string(
        view->subtitle, sizeof(view->subtitle),
        gpu->metrics_source[0] ? gpu->metrics_source : "Graphics adapter");

    if (gpu->engine_metrics_capable && gpu->utilization_available) {
        if (gpu->temperature_available && isfinite(gpu->temperature_c)) {
            (void)snprintf(
                view->rail_value, sizeof(view->rail_value),
                "%s %.0f%% %.0f °C",
                gpu->active_engine[0] ? gpu->active_engine : "GPU",
                gpu->utilization_percent, gpu->temperature_c);
        } else {
            (void)snprintf(
                view->rail_value, sizeof(view->rail_value),
                "%s %.0f%% N/A",
                gpu->active_engine[0] ? gpu->active_engine : "GPU",
                gpu->utilization_percent);
        }
    } else if (gpu->utilization_available) {
        if (gpu->temperature_available && isfinite(gpu->temperature_c)) {
            (void)snprintf(
                view->rail_value, sizeof(view->rail_value),
                "%.0f%% %.0f °C",
                gpu->utilization_percent, gpu->temperature_c);
        } else {
            (void)snprintf(
                view->rail_value, sizeof(view->rail_value),
                "%.0f%% N/A", gpu->utilization_percent);
        }
    } else {
        infiltratr_copy_string(
            view->rail_value, sizeof(view->rail_value), "N/A");
    }

    char value[128];
    device_view_metric(
        view, "Product", gpu->name[0] ? gpu->name : "N/A");
    infiltratr_format_percent(
        gpu->utilization_available, gpu->utilization_percent,
        value, sizeof(value));
    device_view_metric(view, "Utilisation", value);
    infiltratr_format_celsius(
        gpu->temperature_available && isfinite(gpu->temperature_c),
        gpu->temperature_c, value, sizeof(value));
    device_view_metric(view, "Temperature", value);

    if (gpu->shared_system_memory) {
        device_view_metric(view, "Memory", "Dynamic system RAM");
    } else if (gpu->memory_total_bytes > 0U) {
        char used[64];
        char total[64];
        if (gpu->memory_usage_available)
            infiltratr_format_bytes(
                gpu->memory_used_bytes, used, sizeof(used));
        else
            infiltratr_copy_string(used, sizeof(used), "N/A");
        infiltratr_format_bytes(
            gpu->memory_total_bytes, total, sizeof(total));
        (void)snprintf(value, sizeof(value), "%.56s / %.56s", used, total);
        device_view_metric(view, "Memory", value);
    } else {
        device_view_metric(view, "Memory", "N/A");
    }

    device_view_metric(
        view, "Driver", gpu->driver[0] ? gpu->driver : "N/A");
    device_view_metric(
        view, "Driver version",
        gpu->driver_version[0] ? gpu->driver_version : "N/A");
    device_view_metric(
        view, "Active engine",
        gpu->active_engine[0] ? gpu->active_engine : "N/A");
    device_view_metric(
        view, "Telemetry",
        gpu->metrics_source[0]
            ? gpu->metrics_source
            : (gpu->supported_metrics
                   ? "Native driver telemetry"
                   : "Basic identification only"));
}
