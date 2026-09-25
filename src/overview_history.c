// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file overview_history.c
 * @brief Toolkit-neutral completed-snapshot retention for Overview.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "overview_history.h"

#include <infiltratr/core.h>

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct LsmOverviewHistory {
    LsmOverviewSample samples[LSM_OVERVIEW_HISTORY_CAPACITY];
    size_t count;
    size_t next;
    uint64_t last_generation;
    double last_timestamp;
    bool have_last;
};

static double bounded_percent(double value)
{
    if (!isfinite(value)) return 0.0;
    if (value < 0.0) return 0.0;
    return value > 100.0 ? 100.0 : value;
}

static void copy_identity(char *destination, size_t size,
                          const char *primary, const char *fallback)
{
    const char *source =
        primary && primary[0] ? primary :
        (fallback && fallback[0] ? fallback : "");
    infiltratr_copy_string(destination, size, source);
}

static void history_push(LsmOverviewHistory *history,
                         const LsmOverviewSample *sample)
{
    if (!history || !sample) return;
    history->samples[history->next] = *sample;
    history->next =
        (history->next + 1U) % LSM_OVERVIEW_HISTORY_CAPACITY;
    if (history->count < LSM_OVERVIEW_HISTORY_CAPACITY)
        history->count++;
}

static size_t history_slot(const LsmOverviewHistory *history, size_t index)
{
    const size_t oldest =
        (history->next + LSM_OVERVIEW_HISTORY_CAPACITY - history->count) %
        LSM_OVERVIEW_HISTORY_CAPACITY;
    return (oldest + index) % LSM_OVERVIEW_HISTORY_CAPACITY;
}

static void derive_disk(const LsmMonitor *monitor, LsmOverviewSample *sample)
{
    if (!monitor || !sample || monitor->disk_count == 0U)
        return;

    double activity_total = 0.0;
    double read_total = 0.0;
    double write_total = 0.0;
    size_t activity_count = 0U;

    for (size_t index = 0U; index < monitor->disk_count; index++) {
        const LsmDiskInfo *disk = &monitor->disks[index];
        if (isfinite(disk->active_percent) && disk->active_percent >= 0.0) {
            activity_total += bounded_percent(disk->active_percent);
            activity_count++;
        }
        if (isfinite(disk->read_bytes_per_sec) &&
            disk->read_bytes_per_sec >= 0.0)
            read_total += disk->read_bytes_per_sec;
        if (isfinite(disk->write_bytes_per_sec) &&
            disk->write_bytes_per_sec >= 0.0)
            write_total += disk->write_bytes_per_sec;
    }
    if (activity_count == 0U) return;

    /*
     * Overview is system-wide. A disk card that silently changes identity to
     * whichever device is busiest makes the graph describe different hardware
     * from sample to sample. Use the mean physical-disk active-time percentage
     * for the bounded 0-100 graph, and sum throughput across every disk.
     */
    sample->disk_available = true;
    sample->disk_percent =
        bounded_percent(activity_total / (double)activity_count);
    sample->disk_read_bytes_per_sec = read_total;
    sample->disk_write_bytes_per_sec = write_total;
    sample->disk_index = SIZE_MAX;
    sample->disk_identity[0] = '\0';
    sample->disk_name[0] = '\0';
}

static void derive_network(const LsmMonitor *monitor,
                           LsmOverviewSample *sample)
{
    if (!monitor || !sample || monitor->net_count == 0U)
        return;

    size_t busiest = SIZE_MAX;
    double busiest_rate = -1.0;
    for (size_t index = 0U; index < monitor->net_count; index++) {
        const LsmNetInfo *net = &monitor->nets[index];
        if (!isfinite(net->rx_bytes_per_sec) ||
            !isfinite(net->tx_bytes_per_sec) ||
            net->rx_bytes_per_sec < 0.0 || net->tx_bytes_per_sec < 0.0)
            continue;
        const double rate = net->rx_bytes_per_sec + net->tx_bytes_per_sec;
        if (busiest == SIZE_MAX || rate > busiest_rate) {
            busiest = index;
            busiest_rate = rate;
        }
    }
    if (busiest == SIZE_MAX) return;

    const LsmNetInfo *net = &monitor->nets[busiest];
    sample->network_available = true;
    sample->network_receive_bytes_per_sec = net->rx_bytes_per_sec;
    sample->network_send_bytes_per_sec = net->tx_bytes_per_sec;
    sample->network_bytes_per_sec =
        net->rx_bytes_per_sec + net->tx_bytes_per_sec;
    sample->network_index = busiest;
    copy_identity(
        sample->network_identity, sizeof(sample->network_identity),
        net->mac, net->name);
    infiltratr_copy_string(
        sample->network_name, sizeof(sample->network_name),
        net->product[0] ? net->product : net->name);
}

static void derive_gpu(const LsmMonitor *monitor, LsmOverviewSample *sample)
{
    if (!monitor || !sample || monitor->gpu_count == 0U)
        return;

    size_t busiest = SIZE_MAX;
    double busiest_usage = -1.0;
    for (size_t index = 0U; index < monitor->gpu_count; index++) {
        const LsmGpuInfo *gpu = &monitor->gpus[index];
        if (!gpu->utilization_available ||
            !isfinite(gpu->utilization_percent))
            continue;
        const double usage = bounded_percent(gpu->utilization_percent);
        if (busiest == SIZE_MAX || usage > busiest_usage) {
            busiest = index;
            busiest_usage = usage;
        }
    }
    if (busiest == SIZE_MAX) return;

    const LsmGpuInfo *gpu = &monitor->gpus[busiest];
    sample->gpu_available = true;
    sample->gpu_percent = busiest_usage;
    sample->gpu_index = busiest;
    copy_identity(
        sample->gpu_identity, sizeof(sample->gpu_identity),
        gpu->platform_identity, gpu->display_identifier);
    infiltratr_copy_string(
        sample->gpu_name, sizeof(sample->gpu_name),
        gpu->name[0] ? gpu->name : gpu->display_identifier);
}

static void derive_temperature(const LsmMonitor *monitor,
                               LsmOverviewSample *sample)
{
    if (!monitor || !sample) return;

    if (monitor->cpu.temperature_available &&
        isfinite(monitor->cpu.temperature_c)) {
        sample->temperature_available = true;
        sample->temperature_c = monitor->cpu.temperature_c;
        sample->temperature_source = LSM_OVERVIEW_TEMPERATURE_CPU;
        infiltratr_copy_string(
            sample->temperature_name, sizeof(sample->temperature_name),
            monitor->cpu.model[0] ? monitor->cpu.model : "CPU");
    }

    for (size_t index = 0U; index < monitor->gpu_count; index++) {
        const LsmGpuInfo *gpu = &monitor->gpus[index];
        if (!gpu->temperature_available || !isfinite(gpu->temperature_c))
            continue;
        if (!sample->temperature_available ||
            gpu->temperature_c > sample->temperature_c) {
            sample->temperature_available = true;
            sample->temperature_c = gpu->temperature_c;
            sample->temperature_source = LSM_OVERVIEW_TEMPERATURE_GPU;
            sample->temperature_gpu_index = index;
            copy_identity(
                sample->temperature_identity,
                sizeof(sample->temperature_identity),
                gpu->platform_identity, gpu->display_identifier);
            infiltratr_copy_string(
                sample->temperature_name,
                sizeof(sample->temperature_name),
                gpu->name[0] ? gpu->name : gpu->display_identifier);
        }
    }
}

static void derive_sample(const LsmMonitor *monitor,
                          LsmOverviewSample *sample)
{
    memset(sample, 0, sizeof(*sample));
    sample->generation = monitor->sample_generation;
    sample->monotonic_seconds = monitor->sample_monotonic_seconds;

    if (isfinite(monitor->cpu.usage_percent)) {
        sample->cpu_available = true;
        sample->cpu_percent = bounded_percent(monitor->cpu.usage_percent);
    }
    if (isfinite(monitor->cpu.user_percent) &&
        isfinite(monitor->cpu.kernel_percent)) {
        sample->cpu_breakdown_available = true;
        sample->cpu_user_percent =
            bounded_percent(monitor->cpu.user_percent);
        sample->cpu_kernel_percent =
            bounded_percent(monitor->cpu.kernel_percent);
    }
    if (monitor->memory.total_bytes > 0U &&
        isfinite(monitor->memory.usage_percent)) {
        sample->memory_available = true;
        sample->memory_percent =
            bounded_percent(monitor->memory.usage_percent);
    }

    derive_disk(monitor, sample);
    derive_network(monitor, sample);
    derive_gpu(monitor, sample);
    derive_temperature(monitor, sample);

    if (monitor->cpu_pressure.available &&
        isfinite(monitor->cpu_pressure.some_avg10)) {
        sample->cpu_pressure_available = true;
        sample->cpu_pressure_percent =
            bounded_percent(monitor->cpu_pressure.some_avg10);
    }
    if (monitor->memory_pressure.available &&
        isfinite(monitor->memory_pressure.some_avg10)) {
        sample->memory_pressure_available = true;
        sample->memory_pressure_percent =
            bounded_percent(monitor->memory_pressure.some_avg10);
    }
    if (monitor->io_pressure.available &&
        isfinite(monitor->io_pressure.some_avg10)) {
        sample->io_pressure_available = true;
        sample->io_pressure_percent =
            bounded_percent(monitor->io_pressure.some_avg10);
    }
}

LsmOverviewHistory *lsm_overview_history_create(void)
{
    return calloc(1U, sizeof(LsmOverviewHistory));
}

void lsm_overview_history_destroy(LsmOverviewHistory *history)
{
    free(history);
}

void lsm_overview_history_reset(LsmOverviewHistory *history)
{
    if (!history) return;
    memset(history, 0, sizeof(*history));
}

bool lsm_overview_history_record(LsmOverviewHistory *history,
                                 const LsmMonitor *monitor)
{
    if (!history || !monitor || monitor->sample_generation == 0U ||
        !isfinite(monitor->sample_monotonic_seconds) ||
        monitor->sample_monotonic_seconds <= 0.0)
        return false;

    if (history->have_last) {
        if (monitor->sample_generation == history->last_generation)
            return false;
        if (monitor->sample_monotonic_seconds <= history->last_timestamp)
            return false;

        const uint64_t expected =
            history->last_generation == UINT64_MAX
                ? 1U : history->last_generation + 1U;
        if (monitor->sample_generation != expected) {
            LsmOverviewSample gap;
            memset(&gap, 0, sizeof(gap));
            gap.gap = true;
            gap.monotonic_seconds =
                history->last_timestamp +
                (monitor->sample_monotonic_seconds -
                 history->last_timestamp) / 2.0;
            history_push(history, &gap);
        }
    }

    LsmOverviewSample sample;
    derive_sample(monitor, &sample);
    history_push(history, &sample);
    history->last_generation = monitor->sample_generation;
    history->last_timestamp = monitor->sample_monotonic_seconds;
    history->have_last = true;
    return true;
}

size_t lsm_overview_history_count(const LsmOverviewHistory *history)
{
    return history ? history->count : 0U;
}

bool lsm_overview_history_get(const LsmOverviewHistory *history,
                              size_t index, LsmOverviewSample *sample)
{
    if (!history || !sample || index >= history->count)
        return false;
    *sample = history->samples[history_slot(history, index)];
    return true;
}

bool lsm_overview_history_latest(const LsmOverviewHistory *history,
                                 LsmOverviewSample *sample)
{
    if (!history || !sample || history->count == 0U)
        return false;
    return lsm_overview_history_get(
        history, history->count - 1U, sample);
}

static bool identity_matches(const char *saved_identity,
                             const char *saved_name,
                             const char *current_identity,
                             const char *current_name)
{
    if (saved_identity && saved_identity[0] &&
        current_identity && current_identity[0])
        return strcmp(saved_identity, current_identity) == 0;
    return saved_name && saved_name[0] &&
           current_name && current_name[0] &&
           strcmp(saved_name, current_name) == 0;
}

size_t lsm_overview_resolve_disk(const LsmMonitor *monitor,
                                 const LsmOverviewSample *sample)
{
    if (!monitor || !sample || !sample->disk_available)
        return SIZE_MAX;
    for (size_t index = 0U; index < monitor->disk_count; index++) {
        const LsmDiskInfo *disk = &monitor->disks[index];
        if (identity_matches(
                sample->disk_identity, sample->disk_name,
                disk->instance_identity, disk->model[0] ? disk->model : disk->name))
            return index;
    }
    return SIZE_MAX;
}

size_t lsm_overview_resolve_network(const LsmMonitor *monitor,
                                    const LsmOverviewSample *sample)
{
    if (!monitor || !sample || !sample->network_available)
        return SIZE_MAX;
    for (size_t index = 0U; index < monitor->net_count; index++) {
        const LsmNetInfo *net = &monitor->nets[index];
        const char *identity = net->mac[0] ? net->mac : net->name;
        const char *name = net->product[0] ? net->product : net->name;
        if (identity_matches(
                sample->network_identity, sample->network_name,
                identity, name))
            return index;
    }
    return SIZE_MAX;
}

size_t lsm_overview_resolve_gpu(const LsmMonitor *monitor,
                                const LsmOverviewSample *sample)
{
    if (!monitor || !sample || !sample->gpu_available)
        return SIZE_MAX;
    for (size_t index = 0U; index < monitor->gpu_count; index++) {
        const LsmGpuInfo *gpu = &monitor->gpus[index];
        const char *identity =
            gpu->platform_identity[0]
                ? gpu->platform_identity : gpu->display_identifier;
        const char *name =
            gpu->name[0] ? gpu->name : gpu->display_identifier;
        if (identity_matches(
                sample->gpu_identity, sample->gpu_name,
                identity, name))
            return index;
    }
    return SIZE_MAX;
}

static bool process_precedes(const LsmProcessInfo *left,
                             const LsmProcessInfo *right)
{
    if (left->cpu_percent > right->cpu_percent) return true;
    if (left->cpu_percent < right->cpu_percent) return false;
    return left->pid < right->pid;
}

size_t lsm_overview_top_cpu_processes(
    const LsmProcessInfo *processes, size_t count,
    size_t indices[LSM_OVERVIEW_TOP_PROCESS_COUNT])
{
    if (!indices) return 0U;
    for (size_t slot = 0U; slot < LSM_OVERVIEW_TOP_PROCESS_COUNT; slot++)
        indices[slot] = SIZE_MAX;
    if (!processes) return 0U;

    size_t selected = 0U;
    for (size_t index = 0U; index < count; index++) {
        if (!isfinite(processes[index].cpu_percent) ||
            processes[index].cpu_percent < 0.0)
            continue;

        size_t position = 0U;
        while (position < selected &&
               !process_precedes(
                   &processes[index],
                   &processes[indices[position]]))
            position++;
        if (position >= LSM_OVERVIEW_TOP_PROCESS_COUNT)
            continue;

        const size_t limit =
            selected < LSM_OVERVIEW_TOP_PROCESS_COUNT
                ? selected : LSM_OVERVIEW_TOP_PROCESS_COUNT - 1U;
        for (size_t move = limit; move > position; move--)
            indices[move] = indices[move - 1U];
        indices[position] = index;
        if (selected < LSM_OVERVIEW_TOP_PROCESS_COUNT)
            selected++;
    }
    return selected;
}
