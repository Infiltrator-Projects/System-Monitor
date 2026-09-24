// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system_snapshot.c
 * @brief In-process diagnostic report generation with no helper commands.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "system_snapshot.h"

#include "app_internal.h"
#include "atomic_file.h"
#include "common.h"
#include "metric_format.h"
#include "project_info.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

static void set_error(char *error, size_t size, const char *message)
{
    if (!error || size == 0U) return;
    lsm_copy_string(error, size, message ? message : "Unknown error");
}

static void write_clean(FILE *file, const char *text)
{
    if (!file || !text) return;
    for (const unsigned char *cursor = (const unsigned char *)text;
         *cursor; cursor++) {
        if (*cursor == '\n' || *cursor == '\r' || *cursor == '\t')
            fputc(' ', file);
        else if (*cursor >= 32U)
            fputc(*cursor, file);
    }
}

static void read_os_name(char *buffer, size_t size)
{
    if (!buffer || size == 0U) return;
    lsm_copy_string(buffer, size, "Linux");
    FILE *file = fopen("/etc/os-release", "r");
    if (!file) return;
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        static const char prefix[] = "PRETTY_NAME=";
        if (!lsm_string_starts_with(line, prefix)) continue;
        char *value = line + sizeof(prefix) - 1U;
        lsm_trim_line_end(value);
        const size_t length = strlen(value);
        if (length >= 2U && value[0] == '"' && value[length - 1U] == '"') {
            value[length - 1U] = '\0';
            value++;
        }
        lsm_copy_string(buffer, size, value);
        break;
    }
    fclose(file);
}

static void write_memory_modules(FILE *file, const LsmMemoryInfo *memory)
{
    fprintf(file, "\nInstalled memory modules\n");
    if (!memory->module_details_available || memory->module_count == 0U) {
        fprintf(file, "  N/A\n");
        return;
    }
    for (size_t index = 0U; index < memory->module_count; index++) {
        const LsmMemoryModuleInfo *module = &memory->modules[index];
        char size[64], speed[32];
        lsm_format_bytes(module->size_bytes, size, sizeof(size));
        lsm_metric_format_mhz(module->speed_mhz > 0U,
                              (double)module->speed_mhz, speed, sizeof(speed));
        fprintf(file, "  %zu. %s | %s | %s | %s | %s | %s | S/N %s\n",
                index + 1U, module->locator, size, module->memory_type, speed,
                module->manufacturer,
                module->part_number, module->serial_number);
    }
}

static void write_memory_accounting(FILE *file,
                                    const LsmMemoryInfo *memory)
{
    char committed[64], commit_limit[64], cached[64], buffers[64];
    char swap_used[64], swap_total[64], reclaimable[64];
    char nonreclaimable[64], page_tables[64], corrupted[64];
    lsm_format_bytes(memory->committed_bytes, committed, sizeof(committed));
    lsm_format_bytes(memory->commit_limit_bytes, commit_limit,
                     sizeof(commit_limit));
    lsm_format_bytes(memory->cached_bytes, cached, sizeof(cached));
    lsm_format_bytes(memory->buffers_bytes, buffers, sizeof(buffers));
    lsm_format_bytes(memory->swap_used_bytes, swap_used, sizeof(swap_used));
    lsm_format_bytes(memory->swap_total_bytes, swap_total, sizeof(swap_total));
    lsm_format_bytes(memory->kernel_reclaimable_bytes, reclaimable,
                     sizeof(reclaimable));
    lsm_format_bytes(memory->kernel_nonreclaimable_bytes, nonreclaimable,
                     sizeof(nonreclaimable));
    lsm_format_bytes(memory->page_tables_bytes, page_tables,
                     sizeof(page_tables));
    lsm_format_bytes(memory->hardware_corrupted_bytes, corrupted,
                     sizeof(corrupted));
    fprintf(file,
        "  Commit: %s/%s | cached: %s | buffers: %s | swap: %s/%s\n"
        "  Kernel reclaimable: %s | non-reclaimable: %s | "
        "page tables: %s | corrupted: %s\n",
        committed, commit_limit, cached, buffers, swap_used, swap_total,
        reclaimable, nonreclaimable, page_tables, corrupted);
}

static void write_disks(FILE *file, const LsmMonitor *monitor)
{
    fprintf(file, "\nStorage\n");
    for (size_t index = 0U; index < monitor->disk_count; index++) {
        const LsmDiskInfo *disk = &monitor->disks[index];
        char size[64], read_total[64], write_total[64];
        lsm_format_bytes(disk->size_bytes, size, sizeof(size));
        lsm_format_bytes(disk->read_bytes_total, read_total,
                         sizeof(read_total));
        lsm_format_bytes(disk->write_bytes_total, write_total,
                         sizeof(write_total));
        fprintf(file,
            "  Disk %zu: %s (%s) | %s | %s | active %.1f%% | "
            "response %.2f ms | queue %.2f | read %s | written %s\n",
            index, disk->model, disk->name, size,
            disk->media_type[0] ? disk->media_type : "N/A",
            disk->active_percent, disk->average_response_ms,
            disk->queue_length, read_total, write_total);
        for (size_t part = 0U; part < disk->partition_count; part++) {
            const LsmPartitionInfo *partition = &disk->partitions[part];
            char total[64], used[64];
            lsm_format_bytes(partition->total_bytes, total, sizeof(total));
            lsm_format_bytes(partition->used_bytes, used, sizeof(used));
            fprintf(file, "    %s | %s | %s | %s | used %s (%u%%)\n",
                    partition->device,
                    partition->mount_point[0] ? partition->mount_point : "unmounted",
                    partition->filesystem, total,
                    partition->usage_known ? used : "N/A",
                    partition->usage_known ? partition->used_percent : 0U);
        }
    }
}

static void write_networks(FILE *file, const LsmMonitor *monitor)
{
    fprintf(file, "\nNetwork\n");
    for (size_t index = 0U; index < monitor->net_count; index++) {
        const LsmNetInfo *net = &monitor->nets[index];
        char received[64], sent[64];
        lsm_format_bytes(net->rx_bytes_total, received, sizeof(received));
        lsm_format_bytes(net->tx_bytes_total, sent, sizeof(sent));
        fprintf(file,
            "  %s: %s | %s | IPv4 %s | MAC %s | ",
            net->name, net->product[0] ? net->product : "N/A",
            net->connection_state[0] ? net->connection_state : "N/A",
            net->ipv4[0] ? net->ipv4 : "N/A",
            net->mac[0] ? net->mac : "N/A");
        char link_speed[64];
        lsm_metric_format_link_speed_mbps(net->link_speed_mbps, link_speed,
                                          sizeof(link_speed));
        fprintf(file, "link %s", link_speed);
        if (net->utilisation_available)
            fprintf(file, " | utilisation %.1f%%", net->utilisation_percent);
        else
            fprintf(file, " | utilisation N/A");
        fprintf(file, " | received %s | sent %s\n", received, sent);
    }
}

static const char *snapshot_yes_no(bool value)
{
    return value ? "yes" : "no";
}

static void write_bluetooth(FILE *file, const LsmMonitor *monitor)
{
    fprintf(file, "\nBluetooth\n");
    if (monitor->bluetooth_count == 0U &&
        monitor->bluetooth_device_count == 0U) {
        fprintf(file, "  No Bluetooth controllers or connected devices detected.\n");
        return;
    }

    for (size_t index = 0U; index < monitor->bluetooth_count; index++) {
        const LsmBluetoothInfo *adapter = &monitor->bluetooth[index];
        char received[64], sent[64], rx_rate[64], tx_rate[64];
        lsm_format_bytes(adapter->rx_bytes_total, received, sizeof(received));
        lsm_format_bytes(adapter->tx_bytes_total, sent, sizeof(sent));
        if (adapter->traffic_available) {
            lsm_metric_format_network(
                (long double)adapter->rx_bytes_per_sec, false, true,
                rx_rate, sizeof(rx_rate));
            lsm_metric_format_network(
                (long double)adapter->tx_bytes_per_sec, false, true,
                tx_rate, sizeof(tx_rate));
        } else {
            lsm_copy_string(rx_rate, sizeof(rx_rate), "N/A");
            lsm_copy_string(tx_rate, sizeof(tx_rate), "N/A");
        }
        fprintf(file,
            "  Controller %zu: %s | address %s | powered %s | "
            "devices %u, connected %u, paired %u, trusted %u | "
            "RX %s (%s) | TX %s (%s)\n",
            index,
            adapter->alias[0] ? adapter->alias :
                (adapter->adapter_name[0] ? adapter->adapter_name
                                          : adapter->name),
            adapter->address[0] ? adapter->address : "N/A",
            snapshot_yes_no(adapter->powered), adapter->device_count,
            adapter->connected_count, adapter->paired_count,
            adapter->trusted_count, received, rx_rate, sent, tx_rate);
    }

    for (size_t index = 0U; index < monitor->bluetooth_device_count; index++) {
        const LsmBluetoothDeviceInfo *device =
            &monitor->bluetooth_devices[index];
        char received[64], sent[64], rx_rate[64], tx_rate[64];
        lsm_format_bytes(device->rx_bytes_total, received, sizeof(received));
        lsm_format_bytes(device->tx_bytes_total, sent, sizeof(sent));
        if (device->traffic_available) {
            lsm_metric_format_network(
                (long double)device->rx_bytes_per_sec, false, true,
                rx_rate, sizeof(rx_rate));
            lsm_metric_format_network(
                (long double)device->tx_bytes_per_sec, false, true,
                tx_rate, sizeof(tx_rate));
        } else {
            lsm_copy_string(rx_rate, sizeof(rx_rate), "N/A");
            lsm_copy_string(tx_rate, sizeof(tx_rate), "N/A");
        }
        fprintf(file, "  Device %zu: ", index);
        write_clean(file, device->alias[0] ? device->alias :
                          (device->name[0] ? device->name : "Unnamed"));
        fprintf(file,
            " | %s | controller %s | links %u | paired %s | trusted %s | "
            "services %s | RX %s (%s) | TX %s (%s)\n",
            device->address[0] ? device->address : "N/A",
            device->controller[0] ? device->controller : "N/A",
            device->link_count, snapshot_yes_no(device->paired),
            snapshot_yes_no(device->trusted),
            device->services_resolved ? "resolved" : "pending",
            received, rx_rate, sent, tx_rate);
    }
}

static void write_batteries(FILE *file, const LsmMonitor *monitor)
{
    fprintf(file, "\nBatteries and peripheral power\n");
    if (monitor->battery_count == 0U) {
        fprintf(file, "  No battery devices detected.\n");
        return;
    }

    for (size_t index = 0U; index < monitor->battery_count; index++) {
        const LsmBatteryInfo *battery = &monitor->batteries[index];
        char charge[32] = "N/A";
        char power[32] = "N/A";
        char temperature[32] = "N/A";
        if (isfinite(battery->capacity_percent))
            (void)snprintf(charge, sizeof(charge), "%.0f%%",
                           battery->capacity_percent);
        else if (battery->capacity_level[0])
            lsm_copy_string(charge, sizeof(charge), battery->capacity_level);
        if (isfinite(battery->power_watts))
            (void)snprintf(power, sizeof(power), "%.2f W",
                           battery->power_watts);
        if (isfinite(battery->temperature_c))
            (void)snprintf(temperature, sizeof(temperature), "%.1f C",
                           battery->temperature_c);

        fprintf(file, "  Battery %zu: ", index);
        write_clean(file, battery->model[0] ? battery->model :
                          (battery->name[0] ? battery->name : "Unnamed"));
        fprintf(file,
            " | %s | %s | charge %s | power %s | temperature %s | "
            "cycles %u | source %s | present %s\n",
            battery->is_peripheral ? "peripheral" : "system",
            battery->status[0] ? battery->status : "N/A",
            charge, power, temperature, battery->cycle_count,
            battery->battery_source[0] ? battery->battery_source :
                (battery->connection[0] ? battery->connection : "N/A"),
            snapshot_yes_no(battery->present));
    }
}

static void write_graphics(FILE *file, const LsmMonitor *monitor)
{
    fprintf(file, "\nGraphics and accelerators\n");
    for (size_t index = 0U; index < monitor->gpu_count; index++) {
        const LsmGpuInfo *gpu = &monitor->gpus[index];
        fprintf(file,
            "  GPU %zu: %s | driver %s %s | PCI %s | utilisation ",
            index, gpu->name, gpu->driver,
            gpu->driver_version[0] ? gpu->driver_version : "N/A",
            gpu->pci_location[0] ? gpu->pci_location : "N/A");
        if (gpu->utilization_available)
            fprintf(file, "%.1f%%", gpu->utilization_percent);
        else
            fprintf(file, "N/A");
        fprintf(file, " | active engine %s | source %s\n",
                gpu->active_engine[0] ? gpu->active_engine : "N/A",
                gpu->metrics_source[0] ? gpu->metrics_source : "N/A");
    }
    for (size_t index = 0U; index < monitor->npu_count; index++) {
        const LsmNpuInfo *npu = &monitor->npus[index];
        fprintf(file, "  NPU %zu: %s | driver %s | source %s\n",
                index, npu->name, npu->driver,
                npu->metrics_source[0] ? npu->metrics_source : "N/A");
    }
}

static void write_processes(FILE *file, const LsmApp *app)
{
    fprintf(file,
        "\nProcesses (%zu)\n"
        "  PID\tName\tUser\tCPU\tRAM\tGPU\tGPU engine\tCommand\n",
        app->process.process_snapshot_count);
    for (size_t index = 0U; index < app->process.process_snapshot_count; index++) {
        const LsmProcessInfo *process = &app->process.process_snapshot[index];
        char ram[64];
        lsm_format_bytes(process->rss_bytes, ram, sizeof(ram));
        fprintf(file, "  %llu\t",
                (unsigned long long)process->pid);
        write_clean(file, process->name);
        fprintf(file, "\t");
        write_clean(file, process->user);
        fprintf(file, "\t%.1f%%\t%s\t", process->cpu_percent, ram);
        if (process->gpu_available)
            fprintf(file, "%.1f%%\t", process->gpu_percent);
        else
            fprintf(file, "N/A\t");
        write_clean(file, process->gpu_engine[0] ? process->gpu_engine : "N/A");
        fprintf(file, "\t");
        write_clean(file, process->command);
        fprintf(file, "\n");
    }
}

static void format_pressure_snapshot(const LsmPressureInfo *pressure,
                                     char *buffer, size_t size)
{
    if (!buffer || size == 0U) return;
    if (!pressure || !pressure->available) {
        lsm_copy_string(buffer, size, "N/A");
        return;
    }
    if (pressure->full_available)
        (void)snprintf(buffer, size, "some %.2f%% | full %.2f%%",
                       pressure->some_avg10, pressure->full_avg10);
    else
        (void)snprintf(buffer, size, "some %.2f%%",
                       pressure->some_avg10);
}

static bool write_snapshot(FILE *file, const void *user_data)
{
    const LsmApp *app = user_data;
    time_t now = time(NULL);
    struct tm local;
    char generated[64] = "N/A";
    if (localtime_r(&now, &local))
        (void)strftime(generated, sizeof(generated),
                       "%d/%m/%Y %H:%M:%S %z", &local);
    struct utsname kernel;
    memset(&kernel, 0, sizeof(kernel));
    (void)uname(&kernel);
    char hostname[256] = "N/A";
    (void)gethostname(hostname, sizeof(hostname) - 1U);
    char os_name[256];
    read_os_name(os_name, sizeof(os_name));

    const LsmMonitor *monitor = &app->monitor;
    char memory_used[64], memory_total[64];
    char cpu_speed[32], memory_speed[32], slots[32];
    char cpu_pressure[64], memory_pressure[64], io_pressure[64];
    lsm_format_bytes(monitor->memory.used_bytes, memory_used,
                     sizeof(memory_used));
    lsm_format_bytes(monitor->memory.total_bytes, memory_total,
                     sizeof(memory_total));
    lsm_metric_format_ghz(monitor->cpu.frequency_ghz > 0.0,
                          monitor->cpu.frequency_ghz, cpu_speed,
                          sizeof(cpu_speed));
    lsm_metric_format_mhz(monitor->memory.speed_mhz > 0U,
                          (double)monitor->memory.speed_mhz, memory_speed,
                          sizeof(memory_speed));
    if (monitor->memory.slots_total > 0U)
        (void)snprintf(slots, sizeof(slots), "%u of %u",
                       monitor->memory.slots_used,
                       monitor->memory.slots_total);
    else
        (void)snprintf(slots, sizeof(slots), "N/A");
    format_pressure_snapshot(&monitor->cpu_pressure,
                             cpu_pressure, sizeof(cpu_pressure));
    format_pressure_snapshot(&monitor->memory_pressure,
                             memory_pressure, sizeof(memory_pressure));
    format_pressure_snapshot(&monitor->io_pressure,
                             io_pressure, sizeof(io_pressure));
    fprintf(file,
        "System Monitor diagnostic snapshot\n"
        "Version: %s\nGenerated: %s\nHost: %s\nOperating system: %s\n"
        "Kernel: %s %s\nArchitecture: %s\n\n"
        "CPU\n  %s\n  Utilisation: %.1f%% (user %.1f%%, kernel %.1f%%)\n"
        "  Speed: %s | cores: %u | logical: %u | sockets: %u | NUMA: %u\n"
        "  Load average: %.2f %.2f %.2f | interrupts/s: %.0f | context switches/s: %.0f\n"
        "  CPU pressure (10 s): %s\n"
        "  Processes: %u | threads: %u | handles: %llu | uptime: %llu s\n\n"
        "Memory\n  %s of %s used (%.1f%%) | speed: %s | slots: %s\n"
        "  Memory pressure (10 s): %s\n"
        "  I/O pressure (10 s): %s\n",
        lsm_project_info()->version, generated, hostname, os_name, kernel.release,
        kernel.version, kernel.machine, monitor->cpu.model,
        monitor->cpu.usage_percent, monitor->cpu.user_percent,
        monitor->cpu.kernel_percent, cpu_speed,
        monitor->cpu.physical_cores, monitor->cpu.logical_cores,
        monitor->cpu.socket_count, monitor->cpu.numa_node_count,
        monitor->cpu.load_average_1, monitor->cpu.load_average_5,
        monitor->cpu.load_average_15, monitor->cpu.interrupts_per_sec,
        monitor->cpu.context_switches_per_sec, cpu_pressure,
        monitor->cpu.process_count, monitor->cpu.thread_count,
        (unsigned long long)monitor->cpu.file_handle_count,
        (unsigned long long)monitor->cpu.uptime_seconds,
        memory_used, memory_total, monitor->memory.usage_percent,
        memory_speed, slots, memory_pressure, io_pressure);
    write_memory_accounting(file, &monitor->memory);
    write_memory_modules(file, &monitor->memory);
    write_disks(file, monitor);
    write_networks(file, monitor);
    write_bluetooth(file, monitor);
    write_batteries(file, monitor);
    write_graphics(file, monitor);
    write_processes(file, app);
    return ferror(file) == 0;
}

bool lsm_system_snapshot_write(const LsmApp *app, const char *path,
                               char *error, size_t error_size)
{
    if (error && error_size > 0U) error[0] = '\0';
    if (!app || !path || !*path) {
        set_error(error, error_size, "No destination file was selected.");
        return false;
    }
    const int failure = lsm_atomic_file_write(
        path, LSM_ATOMIC_FILE_USER_DOCUMENT, write_snapshot, app);
    if (failure == 0) return true;
    set_error(error, error_size, strerror(failure));
    return false;
}
