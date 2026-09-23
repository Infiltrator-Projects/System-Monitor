// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_view.c
 * @brief Platform-neutral CPU and memory Performance value projection.
 *
 * The implementation uses only plain monitor data plus the portable Infiltratr
 * Common formatting contract. GTK and Win32 render these strings but do not
 * independently reinterpret telemetry availability or units.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
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

static void format_one_decimal_percent(bool available, double value,
                                       char *buffer, size_t size)
{
    if (!buffer || size == 0U) return;
    if (!available || !isfinite(value)) {
        infiltratr_copy_string(buffer, size, "N/A");
        return;
    }
    (void)snprintf(buffer, size, "%.1f%%", value);
}

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
    format_one_decimal_percent(
        true, cpu->user_percent,
        view->metrics[LSM_CPU_METRIC_USER],
        sizeof(view->metrics[LSM_CPU_METRIC_USER]));
    format_one_decimal_percent(
        true, cpu->kernel_percent,
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
