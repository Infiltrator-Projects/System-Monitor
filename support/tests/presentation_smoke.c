// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file presentation_smoke.c
 * @brief Deterministic checks for the shared cross-platform presentation model.
 *
 * The native GTK and Win32 renderers must consume the same product labels,
 * field placement and formatted CPU/memory/device values. These checks intentionally
 * avoid either toolkit so architectural drift is caught at the shared boundary.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_view.h"
#include "presentation_contract.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void check_contract_identity(void)
{
    assert(strcmp(lsm_tab_label(LSM_TAB_PERFORMANCE), "Performance") == 0);
    assert(strcmp(lsm_tab_label(LSM_TAB_FILESYSTEMS), "File Systems") == 0);
    assert(strcmp(lsm_performance_page_title(LSM_PAGE_CPU), "CPU") == 0);
    assert(strcmp(lsm_performance_page_title(LSM_PAGE_MEMORY), "Memory") == 0);
    assert(strcmp(lsm_performance_colour_hex(LSM_PAGE_CPU), "#00adef") == 0);

    const LsmPresentationGridPosition user =
        lsm_cpu_metric_position(LSM_CPU_METRIC_USER);
    assert(user.column == 0U);
    assert(user.row == 4U);

    const LsmPresentationGridPosition switches =
        lsm_cpu_detail_position(LSM_CPU_DETAIL_CONTEXT_SWITCHES);
    assert(switches.column == 1U);
    assert(switches.row == 5U);

    const LsmPresentationGridPosition pressure =
        lsm_memory_metric_position(LSM_MEMORY_METRIC_PRESSURE);
    assert(pressure.column == 1U);
    assert(pressure.row == 4U);

    const LsmPresentationGridPosition modules =
        lsm_memory_detail_position(LSM_MEMORY_DETAIL_INSTALLED_MODULES);
    assert(modules.column == 0U);
    assert(modules.row == 4U);
}

static void check_cpu_availability_semantics(void)
{
    LsmMonitor monitor;
    memset(&monitor, 0, sizeof(monitor));
    (void)snprintf(
        monitor.cpu.model, sizeof(monitor.cpu.model), "Test CPU");
    monitor.cpu.usage_percent = 25.0;
    monitor.cpu.frequency_ghz = 2.5;
    monitor.cpu.physical_cores = 4U;
    monitor.cpu.logical_cores = 8U;
    monitor.cpu.virtualization_available = true;
    monitor.cpu.virtualization = false;
    monitor.cpu.load_average_available = true;
    monitor.cpu.interrupts_per_sec_available = true;
    monitor.cpu.interrupts_per_sec = 0.0;
    monitor.cpu.context_switches_per_sec_available = false;
    monitor.cpu.uptime_seconds = UINT64_C(3661);

    LsmCpuPerformanceView view;
    lsm_cpu_performance_view(&monitor, &view);

    assert(strcmp(view.subtitle, "Test CPU") == 0);
    assert(strcmp(view.rail_value, "25% 2.50 GHz") == 0);
    assert(strcmp(
        view.details[LSM_CPU_DETAIL_VIRTUALISATION], "Disabled") == 0);
    assert(strcmp(
        view.details[LSM_CPU_DETAIL_LOAD_AVERAGE],
        "0.00  0.00  0.00") == 0);
    assert(strcmp(
        view.details[LSM_CPU_DETAIL_INTERRUPTS], "0") == 0);
    assert(strcmp(
        view.details[LSM_CPU_DETAIL_CONTEXT_SWITCHES], "N/A") == 0);
    assert(strcmp(
        view.metrics[LSM_CPU_METRIC_TEMPERATURE], "N/A") == 0);

    monitor.cpu.virtualization_available = false;
    lsm_cpu_performance_view(&monitor, &view);
    assert(strcmp(
        view.details[LSM_CPU_DETAIL_VIRTUALISATION], "N/A") == 0);
}

static void check_memory_projection(void)
{
    LsmMonitor monitor;
    memset(&monitor, 0, sizeof(monitor));
    monitor.memory.total_bytes = UINT64_C(16) * 1024U * 1024U * 1024U;
    monitor.memory.used_bytes = UINT64_C(8) * 1024U * 1024U * 1024U;
    monitor.memory.available_bytes =
        UINT64_C(8) * 1024U * 1024U * 1024U;
    monitor.memory.usage_percent = 50.0;

    LsmMemoryPerformanceView view;
    lsm_memory_performance_view(&monitor, &view);

    assert(strcmp(view.subtitle, "16.0 GB") == 0);
    assert(strstr(view.rail_value, "50%") != NULL);
    assert(strcmp(
        view.metrics[LSM_MEMORY_METRIC_IN_USE], "8.0 GB") == 0);
    assert(strcmp(
        view.metrics[LSM_MEMORY_METRIC_AVAILABLE], "8.0 GB") == 0);
    assert(strcmp(
        view.details[LSM_MEMORY_DETAIL_SPEED], "N/A") == 0);
    assert(strcmp(
        view.details[LSM_MEMORY_DETAIL_SLOTS_USED], "N/A") == 0);
    assert(strcmp(
        view.details[LSM_MEMORY_DETAIL_INSTALLED_MODULES], "N/A") == 0);
}


static void check_device_projection(void)
{
    LsmDiskInfo disk;
    memset(&disk, 0, sizeof(disk));
    (void)snprintf(disk.name, sizeof(disk.name), "Disk 0");
    (void)snprintf(disk.model, sizeof(disk.model), "Test NVMe");
    (void)snprintf(disk.media_type, sizeof(disk.media_type), "Fixed");
    (void)snprintf(
        disk.connection_type, sizeof(disk.connection_type), "NVMe");
    disk.size_bytes = UINT64_C(512) * 1024U * 1024U * 1024U;
    disk.read_bytes_per_sec = 2.5 * 1024.0 * 1024.0;
    disk.write_bytes_per_sec = 1.25 * 1024.0 * 1024.0;
    disk.active_percent = 37.0;
    disk.average_response_ms = 1.5;
    disk.queue_length = 0.25;
    disk.system_disk = true;

    LsmDevicePerformanceView view;
    lsm_disk_performance_view(&disk, 0U, &view);
    assert(strcmp(view.title, "Disk 0 — Test NVMe") == 0);
    assert(strcmp(view.rail_value, "37%") == 0);
    assert(strcmp(view.metric_labels[LSM_DISK_VIEW_READ_SPEED], "Read speed") == 0);
    assert(strcmp(view.metric_values[LSM_DISK_VIEW_READ_SPEED], "2.5 MB/s") == 0);
    assert(strcmp(view.metric_values[LSM_DISK_VIEW_WRITE_SPEED], "1.2 MB/s") == 0);
    assert(strcmp(view.metric_values[LSM_DISK_VIEW_SYSTEM_DISK], "Yes") == 0);

    LsmNetInfo net;
    memset(&net, 0, sizeof(net));
    (void)snprintf(net.name, sizeof(net.name), "Ethernet");
    (void)snprintf(net.product, sizeof(net.product), "Test Adapter");
    (void)snprintf(net.ipv4, sizeof(net.ipv4), "192.0.2.10");
    (void)snprintf(net.mac, sizeof(net.mac), "00:11:22:33:44:55");
    (void)snprintf(
        net.connection_state, sizeof(net.connection_state), "Connected");
    net.rx_bytes_per_sec = 1000000.0;
    net.tx_bytes_per_sec = 300000.0;
    net.link_speed_mbps = 1000.0;
    net.utilisation_available = true;
    net.utilisation_percent = 1.04;

    lsm_network_performance_view(&net, 0U, false, &view);
    assert(strcmp(view.title, "Ethernet 0 — Test Adapter") == 0);
    assert(strstr(view.rail_value, "S:") != NULL);
    assert(strstr(view.rail_value, "R:") != NULL);
    assert(strcmp(view.metric_values[LSM_NETWORK_VIEW_LINK_SPEED], "1.00 Gb/s") == 0);
    assert(strcmp(view.metric_values[LSM_NETWORK_VIEW_IPV4], "192.0.2.10") == 0);

    LsmGpuInfo gpu;
    memset(&gpu, 0, sizeof(gpu));
    (void)snprintf(gpu.name, sizeof(gpu.name), "Test GPU");
    (void)snprintf(
        gpu.metrics_source, sizeof(gpu.metrics_source),
        "Windows display adapter identification");
    lsm_gpu_performance_view(&gpu, 0U, &view);
    assert(strcmp(view.title, "GPU 0 — Test GPU") == 0);
    assert(strcmp(view.rail_value, "N/A") == 0);
    assert(strcmp(view.metric_labels[LSM_GPU_VIEW_PRODUCT], "Product") == 0);
    assert(strcmp(view.metric_values[LSM_GPU_VIEW_PRODUCT], "Test GPU") == 0);
    assert(strcmp(view.metric_values[LSM_GPU_VIEW_UTILISATION], "N/A") == 0);
    assert(strcmp(
        view.metric_values[LSM_GPU_VIEW_TELEMETRY],
        "Windows display adapter identification") == 0);

    gpu.metrics_source[0] = '\0';
    gpu.supported_metrics = false;
    lsm_gpu_performance_view(&gpu, 0U, &view);
    assert(strcmp(
        view.metric_values[LSM_GPU_VIEW_TELEMETRY],
        "Basic identification only") == 0);
    gpu.supported_metrics = true;
    lsm_gpu_performance_view(&gpu, 0U, &view);
    assert(strcmp(
        view.metric_values[LSM_GPU_VIEW_TELEMETRY],
        "Native driver telemetry") == 0);

    lsm_disk_performance_view(NULL, 0U, &view);
    assert(strcmp(view.title, "Disk 0") == 0);
    assert(strcmp(view.rail_value, "N/A") == 0);
    assert(view.metric_count == 0U);
}

int main(void)
{
    check_contract_identity();
    check_cpu_availability_semantics();
    check_memory_projection();
    check_device_projection();
    puts("Shared presentation contract smoke passed.");
    return 0;
}
