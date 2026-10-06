// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_backend_linux.c
 * @brief Linux implementation of the operating-system monitor backend contract.
 *
 * This file is the platform seam for the Performance monitoring lifecycle.
 * Linux sampling policy, procfs/sysfs source ownership and collector ordering
 * remain below this boundary. Slow native collection runs on a dedicated
 * sampler thread; GTK-facing updates publish only completed plain-C snapshots.
 * The application-facing monitor.c therefore contains no Linux collector
 * knowledge and can be reused by another native backend.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_platform.h"

#include "common.h"
#include "monitor_linux_internal.h"
#include "pressure.h"
#include "refresh_policy.h"
#include "sampling_policy.h"
#include "system_sources.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct LsmLinuxSamplerState {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    LsmLinuxMonitorBackendState *backend;
    LsmMonitor sample;
    bool thread_started;
    bool request_pending;
    bool sample_in_progress;
    bool sample_ready;
    bool stop_requested;
    atomic_uint references;
};

#define LSM_TOPOLOGY_RETRY_INITIAL_SECONDS 1.0

static void copy_disk_snapshot(LsmDiskInfo *destination,
                               const LsmDiskInfo *source)
{
    if (!destination || !source) return;

    /*
     * LsmDiskInfo embeds the maximum partition array so a plain structure copy
     * moves tens of kilobytes per disk even when only one or two partitions are
     * populated. Copy the fixed metadata/telemetry prefix, then only the live
     * partition records. Callers publish partition_count last, so readers never
     * observe stale tail entries as part of the snapshot.
     */
    memcpy(destination, source, offsetof(LsmDiskInfo, partitions));
    const size_t partition_count =
        source->partition_count < LSM_MAX_PARTITIONS
            ? source->partition_count : LSM_MAX_PARTITIONS;
    if (partition_count > 0U)
        memcpy(destination->partitions, source->partitions,
               partition_count * sizeof(destination->partitions[0]));
    destination->partition_count = partition_count;
}

static void copy_public_snapshot(LsmMonitor *destination,
                                 const LsmMonitor *source,
                                 void *backend_state,
                                 bool preserve_process_totals)
{
    if (!destination || !source) return;
    const unsigned process_count = destination->cpu.process_count;
    const unsigned thread_count = destination->cpu.thread_count;

    destination->cpu = source->cpu;
    destination->memory = source->memory;
    destination->cpu_pressure = source->cpu_pressure;
    destination->memory_pressure = source->memory_pressure;
    destination->io_pressure = source->io_pressure;

    destination->disk_count =
        source->disk_count < LSM_MAX_DISKS ? source->disk_count : LSM_MAX_DISKS;
    for (size_t index = 0U; index < destination->disk_count; index++)
        copy_disk_snapshot(&destination->disks[index], &source->disks[index]);
    destination->disk_generation = source->disk_generation;
    destination->topology_generation = source->topology_generation;

    destination->net_count =
        source->net_count < LSM_MAX_NETS ? source->net_count : LSM_MAX_NETS;
    if (destination->net_count > 0U)
        memcpy(destination->nets, source->nets,
               destination->net_count * sizeof(destination->nets[0]));

    destination->bluetooth_count =
        source->bluetooth_count < LSM_MAX_BLUETOOTH
            ? source->bluetooth_count : LSM_MAX_BLUETOOTH;
    if (destination->bluetooth_count > 0U)
        memcpy(destination->bluetooth, source->bluetooth,
               destination->bluetooth_count * sizeof(destination->bluetooth[0]));

    destination->bluetooth_device_count =
        source->bluetooth_device_count < LSM_MAX_BLUETOOTH_DEVICES
            ? source->bluetooth_device_count : LSM_MAX_BLUETOOTH_DEVICES;
    if (destination->bluetooth_device_count > 0U)
        memcpy(destination->bluetooth_devices, source->bluetooth_devices,
               destination->bluetooth_device_count *
                   sizeof(destination->bluetooth_devices[0]));

    destination->gpu_count =
        source->gpu_count < LSM_MAX_GPUS ? source->gpu_count : LSM_MAX_GPUS;
    if (destination->gpu_count > 0U)
        memcpy(destination->gpus, source->gpus,
               destination->gpu_count * sizeof(destination->gpus[0]));

    destination->battery_count =
        source->battery_count < LSM_MAX_BATTERIES
            ? source->battery_count : LSM_MAX_BATTERIES;
    if (destination->battery_count > 0U)
        memcpy(destination->batteries, source->batteries,
               destination->battery_count * sizeof(destination->batteries[0]));

    destination->npu_count =
        source->npu_count < LSM_MAX_NPUS ? source->npu_count : LSM_MAX_NPUS;
    if (destination->npu_count > 0U)
        memcpy(destination->npus, source->npus,
               destination->npu_count * sizeof(destination->npus[0]));

    destination->sample_generation = source->sample_generation;
    destination->sample_monotonic_seconds = source->sample_monotonic_seconds;
    destination->backend_state = backend_state;
    if (preserve_process_totals) {
        destination->cpu.process_count = process_count;
        destination->cpu.thread_count = thread_count;
    }
}

static double topology_retry_delay(unsigned failures)
{
    if (failures <= 1U) return LSM_TOPOLOGY_RETRY_INITIAL_SECONDS;
    if (failures == 2U) return 2.0;
    if (failures == 3U) return 4.0;
    return LSM_TOPOLOGY_SCAN_INTERVAL_SECONDS;
}

static bool sample_once(LsmLinuxMonitorBackendState *state,
                        LsmMonitor *sample, bool force_topology)
{
    if (!state || !sample) return false;
    const double now = lsm_monotonic_seconds();
    if (!isfinite(now) || now <= state->last_update_monotonic)
        return false;
    const double elapsed = now - state->last_update_monotonic;
    state->last_update_monotonic = now;

    const bool retry_topology =
        state->topology_retry_pending &&
        now >= state->topology_retry_not_before_monotonic;
    const bool scheduled_topology =
        !state->topology_retry_pending &&
        lsm_refresh_interval_due(
            now, state->last_topology_scan_monotonic,
            LSM_TOPOLOGY_SCAN_INTERVAL_SECONDS);
    const bool refresh_topology =
        force_topology || retry_topology || scheduled_topology;
    const bool refresh_batteries = lsm_refresh_interval_due(
        now, state->last_battery_update_monotonic,
        LSM_BATTERY_UPDATE_INTERVAL_SECONDS);

    lsm_cpu_memory_update(sample, elapsed);
    const bool storage_complete =
        lsm_storage_update(sample, elapsed, refresh_topology);
    (void)lsm_pressure_read("/proc/pressure/cpu", &sample->cpu_pressure);
    (void)lsm_pressure_read("/proc/pressure/memory", &sample->memory_pressure);
    (void)lsm_pressure_read("/proc/pressure/io", &sample->io_pressure);
    const bool hardware_complete =
        lsm_hardware_update(
            sample, elapsed, refresh_topology, refresh_batteries);

    /* Publish completion identity only after every collector for this native
     * sample has returned. Presentation can therefore distinguish a genuinely
     * new snapshot from repeated GTK refreshes while the worker is still busy. */
    sample->sample_generation++;
    if (sample->sample_generation == 0U)
        sample->sample_generation = 1U;
    sample->sample_monotonic_seconds = now;

    if (refresh_topology) {
        if (storage_complete && hardware_complete) {
            state->last_topology_scan_monotonic = now;
            state->topology_retry_pending = false;
            state->topology_retry_failures = 0U;
            state->topology_retry_not_before_monotonic = 0.0;
        } else {
            /*
             * Retry promptly, but back off repeated failures so a permanently
             * blocked mount/device cannot turn the topology slow path into a
             * one-second hot loop. Collectors retain the last complete
             * topology until a later attempt succeeds.
             */
            if (state->topology_retry_failures < UINT_MAX)
                state->topology_retry_failures++;
            state->topology_retry_pending = true;
            state->topology_retry_not_before_monotonic =
                now + topology_retry_delay(state->topology_retry_failures);
        }
    }
    if (refresh_batteries)
        state->last_battery_update_monotonic = now;
    return true;
}

static void sampler_release(LsmLinuxSamplerState *sampler)
{
    if (!sampler ||
        atomic_fetch_sub_explicit(&sampler->references, 1U,
                                  memory_order_acq_rel) != 1U)
        return;
    LsmLinuxMonitorBackendState *state = sampler->backend;
    lsm_hardware_shutdown(&sampler->sample);
    lsm_cpu_memory_shutdown(&sampler->sample);
    (void)pthread_cond_destroy(&sampler->condition);
    (void)pthread_mutex_destroy(&sampler->mutex);
    if (state) {
        state->sampler_state = NULL;
        lsm_wifi_metadata_destroy(state->wifi_metadata);
        state->wifi_metadata = NULL;
        lsm_sources_destroy(state->system_sources);
        state->system_sources = NULL;
        free(state);
    }
    free(sampler);
}

static void *sampler_thread_main(void *user_data)
{
    LsmLinuxSamplerState *sampler = user_data;
    if (!sampler || !sampler->backend) return NULL;

    /* Storage topology can enter statvfs() on slow or blocked mounts and
     * hardware discovery can touch device interfaces. Perform both only after
     * the worker owns execution so GTK activation can construct the window. */
    const bool storage_complete =
        lsm_storage_initialise(&sampler->sample);
    const bool hardware_complete =
        lsm_hardware_initialise(&sampler->sample);
    if (!storage_complete || !hardware_complete) {
        (void)pthread_mutex_lock(&sampler->mutex);
        sampler->backend->topology_refresh_requested = true;
        (void)pthread_mutex_unlock(&sampler->mutex);
    }

    for (;;) {
        bool force_topology = false;
        (void)pthread_mutex_lock(&sampler->mutex);
        while (!sampler->request_pending && !sampler->stop_requested)
            (void)pthread_cond_wait(&sampler->condition, &sampler->mutex);
        if (sampler->stop_requested) {
            (void)pthread_mutex_unlock(&sampler->mutex);
            break;
        }
        sampler->request_pending = false;
        sampler->sample_in_progress = true;
        force_topology = sampler->backend->topology_refresh_requested;
        sampler->backend->topology_refresh_requested = false;
        (void)pthread_mutex_unlock(&sampler->mutex);

        const bool sampled = sample_once(
            sampler->backend, &sampler->sample, force_topology);

        (void)pthread_mutex_lock(&sampler->mutex);
        sampler->sample_in_progress = false;
        if (sampled && !sampler->stop_requested)
            sampler->sample_ready = true;
        (void)pthread_mutex_unlock(&sampler->mutex);
        if (!sampled) continue;
    }
    sampler_release(sampler);
    return NULL;
}

static void destroy_sampler(LsmLinuxMonitorBackendState *state)
{
    if (!state || !state->sampler_state) return;
    LsmLinuxSamplerState *sampler = state->sampler_state;
    if (sampler->thread_started) {
        (void)pthread_mutex_lock(&sampler->mutex);
        sampler->stop_requested = true;
        (void)pthread_cond_signal(&sampler->condition);
        (void)pthread_mutex_unlock(&sampler->mutex);

        /* The worker owns its backend reference and uses no application or
         * widget state. Let it release that reference after observing stop,
         * without holding shutdown behind an in-flight device read. */
        (void)pthread_detach(sampler->thread);
    }
    sampler_release(sampler);
}

bool lsm_monitor_platform_init(LsmMonitor *monitor)
{
    if (!monitor) return false;
    memset(monitor, 0, sizeof(*monitor));

    LsmLinuxMonitorBackendState *state = calloc(1U, sizeof(*state));
    if (!state) return false;
    monitor->backend_state = state;

    LsmLinuxSamplerState *sampler = calloc(1U, sizeof(*sampler));
    if (!sampler) {
        free(state);
        monitor->backend_state = NULL;
        return false;
    }
    if (pthread_mutex_init(&sampler->mutex, NULL) != 0) {
        free(sampler);
        free(state);
        monitor->backend_state = NULL;
        return false;
    }
    if (pthread_cond_init(&sampler->condition, NULL) != 0) {
        (void)pthread_mutex_destroy(&sampler->mutex);
        free(sampler);
        free(state);
        monitor->backend_state = NULL;
        return false;
    }
    sampler->backend = state;
    atomic_init(&sampler->references, 1U);
    sampler->sample.backend_state = state;
    state->sampler_state = sampler;

    if (!lsm_sources_init(&state->system_sources)) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }
    state->wifi_metadata = lsm_wifi_metadata_create();
    if (!state->wifi_metadata) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }

    if (!lsm_cpu_memory_initialise(&sampler->sample)) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }
    /* Slow storage/hardware discovery starts in sampler_thread_main(). */
    const double now = lsm_monotonic_seconds();
    if (!isfinite(now) || now <= 0.0) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }
    state->last_update_monotonic = now;
    state->last_topology_scan_monotonic = now;
    state->last_battery_update_monotonic = now;

    copy_public_snapshot(monitor, &sampler->sample, state, false);

    sampler->request_pending = true;
    (void)atomic_fetch_add_explicit(&sampler->references, 1U,
                                    memory_order_relaxed);
    const int thread_error = pthread_create(
        &sampler->thread, NULL, sampler_thread_main, sampler);
    if (thread_error != 0) {
        sampler_release(sampler);
        lsm_monitor_platform_destroy(monitor);
        return false;
    }
    sampler->thread_started = true;
    return true;
}

bool lsm_monitor_platform_update(LsmMonitor *monitor)
{
    if (!monitor) return false;
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state || !state->sampler_state) return false;
    LsmLinuxSamplerState *sampler = state->sampler_state;

    (void)pthread_mutex_lock(&sampler->mutex);
    if (sampler->sample_ready) {
        /*
         * The worker does not touch sample again until a new request is
         * signalled below. Publish directly from that retained buffer, avoiding
         * the former sample -> completed -> public double copy.
         */
        copy_public_snapshot(monitor, &sampler->sample, state, true);
        sampler->sample_ready = false;
    }
    if (!sampler->request_pending && !sampler->sample_in_progress) {
        sampler->request_pending = true;
        (void)pthread_cond_signal(&sampler->condition);
    }
    (void)pthread_mutex_unlock(&sampler->mutex);

    /* A refresh request is considered successful even when the worker is
     * still completing the previous native sample. The GTK caller therefore
     * never waits on procfs/sysfs/device I/O under system pressure. */
    return true;
}

void lsm_monitor_platform_request_topology_refresh(LsmMonitor *monitor)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return;
    LsmLinuxSamplerState *sampler = state->sampler_state;
    if (!sampler) {
        state->topology_refresh_requested = true;
        return;
    }
    (void)pthread_mutex_lock(&sampler->mutex);
    state->topology_refresh_requested = true;
    /*
     * Do not let a topology request overwrite an unread completed sample.
     * lsm_monitor_platform_update() will publish it first and then wake the
     * worker with the retained topology request.
     */
    if (!sampler->sample_ready &&
        !sampler->request_pending &&
        !sampler->sample_in_progress) {
        sampler->request_pending = true;
        (void)pthread_cond_signal(&sampler->condition);
    }
    (void)pthread_mutex_unlock(&sampler->mutex);
}

void lsm_monitor_platform_destroy(LsmMonitor *monitor)
{
    if (!monitor) return;

    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (state) {
        destroy_sampler(state);
    }
    monitor->backend_state = NULL;
}
