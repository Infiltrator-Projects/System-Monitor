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
#include <math.h>
#include <pthread.h>
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
    LsmMonitor completed;
    bool thread_started;
    bool request_pending;
    bool sample_ready;
    bool stop_requested;
    atomic_uint references;
};

#define LSM_SAMPLER_SHUTDOWN_WAIT_MS 250L

static void copy_public_snapshot(LsmMonitor *destination,
                                 const LsmMonitor *source,
                                 void *backend_state,
                                 bool preserve_process_totals)
{
    if (!destination || !source) return;
    const unsigned process_count = destination->cpu.process_count;
    const unsigned thread_count = destination->cpu.thread_count;
    *destination = *source;
    destination->backend_state = backend_state;
    if (preserve_process_totals) {
        destination->cpu.process_count = process_count;
        destination->cpu.thread_count = thread_count;
    }
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

    const bool refresh_topology = force_topology ||
        lsm_refresh_interval_due(
            now, state->last_topology_scan_monotonic,
            LSM_TOPOLOGY_SCAN_INTERVAL_SECONDS);
    const bool refresh_batteries = lsm_refresh_interval_due(
        now, state->last_battery_update_monotonic,
        LSM_BATTERY_UPDATE_INTERVAL_SECONDS);

    lsm_cpu_memory_update(sample, elapsed);
    lsm_storage_update(sample, elapsed, refresh_topology);
    (void)lsm_pressure_read("/proc/pressure/cpu", &sample->cpu_pressure);
    (void)lsm_pressure_read("/proc/pressure/memory", &sample->memory_pressure);
    (void)lsm_pressure_read("/proc/pressure/io", &sample->io_pressure);
    lsm_hardware_update(sample, elapsed, refresh_topology, refresh_batteries);

    if (refresh_topology)
        state->last_topology_scan_monotonic = now;
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
    if (!lsm_storage_initialise(&sampler->sample)) {
        (void)pthread_mutex_lock(&sampler->mutex);
        sampler->backend->topology_refresh_requested = true;
        (void)pthread_mutex_unlock(&sampler->mutex);
    }
    lsm_hardware_initialise(&sampler->sample);

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
        force_topology = sampler->backend->topology_refresh_requested;
        sampler->backend->topology_refresh_requested = false;
        (void)pthread_mutex_unlock(&sampler->mutex);

        if (!sample_once(sampler->backend, &sampler->sample, force_topology))
            continue;

        (void)pthread_mutex_lock(&sampler->mutex);
        if (!sampler->stop_requested) {
            sampler->completed = sampler->sample;
            sampler->completed.backend_state = NULL;
            sampler->sample_ready = true;
        }
        (void)pthread_mutex_unlock(&sampler->mutex);
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

        struct timespec deadline;
        int join_result = lsm_posix_deadline_after_milliseconds(
            CLOCK_REALTIME, (uint64_t)LSM_SAMPLER_SHUTDOWN_WAIT_MS,
            &deadline);
        if (join_result == 0)
            join_result = pthread_timedjoin_np(
                sampler->thread, NULL, &deadline);

        if (join_result != 0) {
            /* Both parties own a reference from thread creation onward. A
             * timeout may race with worker exit, so cleanup cannot depend on
             * a flag transferred after the timed join. The last reference
             * releases the backend in either order, including a blocked
             * collector that finishes after the GUI has relinquished it. */
            (void)pthread_detach(sampler->thread);
        }
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
        /* The completed slot already lives in heap-owned sampler state. Copy it
         * directly while holding the short publication lock rather than placing
         * another multi-megabyte LsmMonitor on the caller's stack. */
        copy_public_snapshot(monitor, &sampler->completed, state, true);
        sampler->sample_ready = false;
    }
    sampler->request_pending = true;
    (void)pthread_cond_signal(&sampler->condition);
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
    sampler->request_pending = true;
    (void)pthread_cond_signal(&sampler->condition);
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
