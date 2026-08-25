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
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_platform.h"

#include "common.h"
#include "monitor_linux_internal.h"
#include "refresh_policy.h"
#include "sampling_policy.h"
#include "system_sources.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

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
};

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
    lsm_hardware_update(sample, elapsed, refresh_topology, refresh_batteries);

    if (refresh_topology)
        state->last_topology_scan_monotonic = now;
    if (refresh_batteries)
        state->last_battery_update_monotonic = now;
    return true;
}

static void *sampler_thread_main(void *user_data)
{
    LsmLinuxSamplerState *sampler = user_data;
    if (!sampler || !sampler->backend) return NULL;

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
        (void)pthread_join(sampler->thread, NULL);
        sampler->thread_started = false;
    }

    lsm_hardware_shutdown(&sampler->sample);
    lsm_cpu_memory_shutdown(&sampler->sample);
    (void)pthread_cond_destroy(&sampler->condition);
    (void)pthread_mutex_destroy(&sampler->mutex);
    free(sampler);
    state->sampler_state = NULL;
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
    if (!lsm_storage_initialise(&sampler->sample)) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }
    lsm_hardware_initialise(&sampler->sample);

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
    const int thread_error = pthread_create(
        &sampler->thread, NULL, sampler_thread_main, sampler);
    if (thread_error != 0) {
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

    LsmMonitor completed;
    bool have_completed = false;
    (void)pthread_mutex_lock(&sampler->mutex);
    if (sampler->sample_ready) {
        completed = sampler->completed;
        sampler->sample_ready = false;
        have_completed = true;
    }
    sampler->request_pending = true;
    (void)pthread_cond_signal(&sampler->condition);
    (void)pthread_mutex_unlock(&sampler->mutex);

    if (have_completed)
        copy_public_snapshot(monitor, &completed, state, true);

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
        lsm_wifi_metadata_destroy(state->wifi_metadata);
        state->wifi_metadata = NULL;
        lsm_sources_destroy(state->system_sources);
        state->system_sources = NULL;
        free(state);
    }
    monitor->backend_state = NULL;
}
