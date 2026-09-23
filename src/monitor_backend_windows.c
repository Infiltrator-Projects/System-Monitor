// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_backend_windows.c
 * @brief Native Windows implementation of the shared monitor backend contract.
 *
 * Windows and Linux publish into the same LsmMonitor data model.  This adapter
 * owns only Win32-specific discovery and sampling: system CPU/memory through
 * Win32/PSAPI, physical storage through the storage IOCTL surface, network
 * interfaces through IP Helper, and graphics-adapter identity through the
 * native display-device API.  Presentation policy remains above this seam.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2016-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_platform.h"

#include <infiltratr/core.h>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 1
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <winioctl.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LSM_WINDOWS_TOPOLOGY_REFRESH_MS 30000ULL
#define LSM_WINDOWS_VOLUME_BUFFER 4096U
#define LSM_WINDOWS_STORAGE_DESCRIPTOR_BUFFER 2048U
#define LSM_WINDOWS_EXTENTS_BUFFER 4096U

typedef struct {
    bool valid;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t read_count;
    uint64_t write_count;
    uint64_t read_time_100ns;
    uint64_t write_time_100ns;
} LsmWindowsDiskBaseline;

typedef struct {
    bool valid;
    uint64_t key;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
} LsmWindowsNetBaseline;

typedef struct {
    uint64_t idle_time;
    uint64_t kernel_time;
    uint64_t user_time;
    bool cpu_baseline_valid;
    ULONGLONG previous_sample_tick;
    ULONGLONG last_topology_tick;
    bool topology_refresh_requested;
    bool winsock_started;
    LsmWindowsDiskBaseline disks[LSM_MAX_DISKS];
    LsmWindowsNetBaseline nets[LSM_MAX_NETS];
} LsmWindowsMonitorBackendState;

static uint64_t filetime_value(FILETIME value)
{
    return ((uint64_t)value.dwHighDateTime << 32U) |
           (uint64_t)value.dwLowDateTime;
}

static uint64_t large_integer_u64(LARGE_INTEGER value)
{
    return value.QuadPart > 0 ? (uint64_t)value.QuadPart : 0U;
}

static uint64_t pages_to_bytes(SIZE_T pages, SIZE_T page_size)
{
    if (page_size != 0U &&
        (uint64_t)pages > UINT64_MAX / (uint64_t)page_size)
        return UINT64_MAX;
    return (uint64_t)pages * (uint64_t)page_size;
}

static unsigned size_to_unsigned(SIZE_T value)
{
    return (uint64_t)value > (uint64_t)UINT_MAX
        ? UINT_MAX : (unsigned)value;
}

static double percent_u64(uint64_t part, uint64_t total)
{
    if (total == 0U) return 0.0;
    const double value = ((double)part * 100.0) / (double)total;
    return value > 100.0 ? 100.0 : value;
}

static double rate_u64(uint64_t current, uint64_t previous, double elapsed)
{
    if (elapsed <= 0.0 || current < previous) return 0.0;
    return (double)(current - previous) / elapsed;
}

static void wide_to_utf8(const wchar_t *source, char *destination,
                         size_t destination_size)
{
    if (!destination || destination_size == 0U) return;
    destination[0] = '\0';
    if (!source || !source[0] || destination_size > (size_t)INT_MAX) return;

    const int result = WideCharToMultiByte(
        CP_UTF8, 0U, source, -1, destination,
        (int)destination_size, NULL, NULL);
    if (result <= 0)
        destination[0] = '\0';
}

static bool read_cpu_times(uint64_t *idle, uint64_t *kernel, uint64_t *user)
{
    if (!idle || !kernel || !user) return false;

    FILETIME idle_time;
    FILETIME kernel_time;
    FILETIME user_time;
    if (!GetSystemTimes(&idle_time, &kernel_time, &user_time))
        return false;

    *idle = filetime_value(idle_time);
    *kernel = filetime_value(kernel_time);
    *user = filetime_value(user_time);
    return true;
}

static void populate_cpu_identity(LsmMonitor *monitor)
{
    if (!monitor) return;

    SYSTEM_INFO system_info;
    GetNativeSystemInfo(&system_info);
    monitor->cpu.logical_cores = (unsigned)system_info.dwNumberOfProcessors;

    monitor->cpu.model[0] = '\0';
    const DWORD length = GetEnvironmentVariableA(
        "PROCESSOR_IDENTIFIER", monitor->cpu.model,
        (DWORD)sizeof(monitor->cpu.model));
    if (length == 0U || length >= (DWORD)sizeof(monitor->cpu.model))
        monitor->cpu.model[0] = '\0';
}

static bool update_cpu_snapshot(LsmMonitor *monitor,
                                LsmWindowsMonitorBackendState *state)
{
    if (!monitor || !state) return false;

    uint64_t idle = 0U;
    uint64_t kernel = 0U;
    uint64_t user = 0U;
    if (!read_cpu_times(&idle, &kernel, &user))
        return false;

    if (state->cpu_baseline_valid &&
        idle >= state->idle_time &&
        kernel >= state->kernel_time &&
        user >= state->user_time) {
        const uint64_t idle_delta = idle - state->idle_time;
        const uint64_t kernel_delta = kernel - state->kernel_time;
        const uint64_t user_delta = user - state->user_time;
        const uint64_t total_delta = kernel_delta + user_delta;
        const uint64_t busy_kernel_delta =
            kernel_delta >= idle_delta ? kernel_delta - idle_delta : 0U;
        const uint64_t busy_delta = user_delta + busy_kernel_delta;

        monitor->cpu.usage_percent = percent_u64(busy_delta, total_delta);
        monitor->cpu.user_percent = percent_u64(user_delta, total_delta);
        monitor->cpu.kernel_percent =
            percent_u64(busy_kernel_delta, total_delta);
    } else {
        monitor->cpu.usage_percent = 0.0;
        monitor->cpu.user_percent = 0.0;
        monitor->cpu.kernel_percent = 0.0;
    }

    state->idle_time = idle;
    state->kernel_time = kernel;
    state->user_time = user;
    state->cpu_baseline_valid = true;
    monitor->cpu.uptime_seconds = (uint64_t)(GetTickCount64() / 1000ULL);
    return true;
}

static bool update_memory_snapshot(LsmMonitor *monitor)
{
    if (!monitor) return false;

    PERFORMANCE_INFORMATION performance;
    memset(&performance, 0, sizeof(performance));
    performance.cb = (DWORD)sizeof(performance);
    if (GetPerformanceInfo(&performance, (DWORD)sizeof(performance))) {
        monitor->memory.total_bytes = pages_to_bytes(
            performance.PhysicalTotal, performance.PageSize);
        monitor->memory.available_bytes = pages_to_bytes(
            performance.PhysicalAvailable, performance.PageSize);
        monitor->memory.cached_bytes = pages_to_bytes(
            performance.SystemCache, performance.PageSize);
        monitor->memory.committed_bytes = pages_to_bytes(
            performance.CommitTotal, performance.PageSize);
        monitor->memory.commit_limit_bytes = pages_to_bytes(
            performance.CommitLimit, performance.PageSize);
        monitor->cpu.process_count =
            size_to_unsigned((SIZE_T)performance.ProcessCount);
        monitor->cpu.thread_count =
            size_to_unsigned((SIZE_T)performance.ThreadCount);
        monitor->cpu.file_handle_count = (uint64_t)performance.HandleCount;
    } else {
        MEMORYSTATUSEX status;
        memset(&status, 0, sizeof(status));
        status.dwLength = (DWORD)sizeof(status);
        if (!GlobalMemoryStatusEx(&status))
            return false;
        monitor->memory.total_bytes = (uint64_t)status.ullTotalPhys;
        monitor->memory.available_bytes = (uint64_t)status.ullAvailPhys;
        monitor->memory.cached_bytes = 0U;
        monitor->memory.committed_bytes = 0U;
        monitor->memory.commit_limit_bytes = 0U;
    }

    monitor->memory.used_bytes =
        monitor->memory.total_bytes >= monitor->memory.available_bytes
            ? monitor->memory.total_bytes - monitor->memory.available_bytes
            : 0U;
    monitor->memory.usage_percent = percent_u64(
        monitor->memory.used_bytes, monitor->memory.total_bytes);
    return true;
}

static const char *storage_bus_name(STORAGE_BUS_TYPE bus)
{
    switch ((unsigned)bus) {
        case 1U: return "SCSI";
        case 2U: return "ATAPI";
        case 3U: return "ATA";
        case 7U: return "USB";
        case 8U: return "RAID";
        case 11U: return "SATA";
        case 17U: return "NVMe";
        case 18U: return "SCM";
        case 19U: return "UFS";
        default: return "Unknown";
    }
}

static void descriptor_text(
    const unsigned char *buffer, size_t buffer_size, DWORD offset,
    char *destination, size_t destination_size)
{
    if (!destination || destination_size == 0U) return;
    destination[0] = '\0';
    if (!buffer || offset == 0U || (size_t)offset >= buffer_size) return;

    const char *source = (const char *)(buffer + offset);
    size_t available = buffer_size - (size_t)offset;
    size_t length = 0U;
    while (length < available && source[length] != '\0')
        length++;
    while (length > 0U && source[length - 1U] == ' ')
        length--;
    while (*source == ' ' && length > 0U) {
        source++;
        length--;
    }

    const size_t copy = length < destination_size - 1U
        ? length : destination_size - 1U;
    if (copy > 0U)
        memcpy(destination, source, copy);
    destination[copy] = '\0';
}

static bool query_disk_identity(HANDLE disk, LsmDiskInfo *info)
{
    if (disk == INVALID_HANDLE_VALUE || !info) return false;

    STORAGE_PROPERTY_QUERY query;
    memset(&query, 0, sizeof(query));
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    unsigned char buffer[LSM_WINDOWS_STORAGE_DESCRIPTOR_BUFFER];
    memset(buffer, 0, sizeof(buffer));
    DWORD bytes = 0U;
    if (!DeviceIoControl(
            disk, IOCTL_STORAGE_QUERY_PROPERTY,
            &query, (DWORD)sizeof(query),
            buffer, (DWORD)sizeof(buffer),
            &bytes, NULL) ||
        bytes < sizeof(STORAGE_DEVICE_DESCRIPTOR))
        return false;

    const STORAGE_DEVICE_DESCRIPTOR *descriptor =
        (const STORAGE_DEVICE_DESCRIPTOR *)buffer;
    char vendor[64];
    char product[96];
    descriptor_text(
        buffer, (size_t)bytes, descriptor->VendorIdOffset,
        vendor, sizeof(vendor));
    descriptor_text(
        buffer, (size_t)bytes, descriptor->ProductIdOffset,
        product, sizeof(product));

    if (vendor[0] && product[0])
        (void)snprintf(info->model, sizeof(info->model), "%s %s", vendor, product);
    else if (product[0])
        infiltratr_copy_string(info->model, sizeof(info->model), product);
    else if (vendor[0])
        infiltratr_copy_string(info->model, sizeof(info->model), vendor);

    infiltratr_copy_string(
        info->connection_type, sizeof(info->connection_type),
        storage_bus_name(descriptor->BusType));
    infiltratr_copy_string(
        info->media_type, sizeof(info->media_type),
        descriptor->RemovableMedia ? "Removable" : "Fixed");
    return true;
}

static void query_disk_size(HANDLE disk, LsmDiskInfo *info)
{
    if (disk == INVALID_HANDLE_VALUE || !info) return;

    unsigned char buffer[
        sizeof(DISK_GEOMETRY_EX) + sizeof(DISK_PARTITION_INFO) +
        sizeof(DISK_DETECTION_INFO)];
    memset(buffer, 0, sizeof(buffer));
    DWORD bytes = 0U;
    if (DeviceIoControl(
            disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
            NULL, 0U, buffer, (DWORD)sizeof(buffer), &bytes, NULL) &&
        bytes >= sizeof(DISK_GEOMETRY_EX)) {
        const DISK_GEOMETRY_EX *geometry =
            (const DISK_GEOMETRY_EX *)buffer;
        info->size_bytes = large_integer_u64(geometry->DiskSize);
    }
}

static bool query_disk_performance(HANDLE disk, DISK_PERFORMANCE *performance)
{
    if (disk == INVALID_HANDLE_VALUE || !performance) return false;
    memset(performance, 0, sizeof(*performance));
    DWORD bytes = 0U;
    return DeviceIoControl(
        disk, IOCTL_DISK_PERFORMANCE, NULL, 0U,
        performance, (DWORD)sizeof(*performance),
        &bytes, NULL) != FALSE &&
        bytes >= sizeof(*performance);
}

static void update_disk_performance(
    LsmDiskInfo *disk, LsmWindowsDiskBaseline *baseline,
    const DISK_PERFORMANCE *performance, double elapsed)
{
    if (!disk || !baseline || !performance) return;

    const uint64_t read_bytes = large_integer_u64(performance->BytesRead);
    const uint64_t write_bytes = large_integer_u64(performance->BytesWritten);
    const uint64_t read_count = (uint64_t)performance->ReadCount;
    const uint64_t write_count = (uint64_t)performance->WriteCount;
    const uint64_t read_time =
        large_integer_u64(performance->ReadTime);
    const uint64_t write_time =
        large_integer_u64(performance->WriteTime);

    disk->read_bytes_total = read_bytes;
    disk->write_bytes_total = write_bytes;
    disk->queue_length = (double)performance->QueueDepth;
    disk->in_progress_operations = performance->QueueDepth;

    if (baseline->valid && elapsed > 0.0) {
        disk->read_bytes_per_sec =
            rate_u64(read_bytes, baseline->read_bytes, elapsed);
        disk->write_bytes_per_sec =
            rate_u64(write_bytes, baseline->write_bytes, elapsed);

        if (read_time >= baseline->read_time_100ns &&
            write_time >= baseline->write_time_100ns) {
            const uint64_t read_time_delta =
                read_time - baseline->read_time_100ns;
            const uint64_t write_time_delta =
                write_time - baseline->write_time_100ns;
            const long double elapsed_100ns =
                (long double)elapsed * 10000000.0L;
            const long double busy =
                (long double)read_time_delta +
                (long double)write_time_delta;
            double active = elapsed_100ns > 0.0L
                ? (double)((busy * 100.0L) / elapsed_100ns)
                : 0.0;
            if (active > 100.0) active = 100.0;
            if (active < 0.0) active = 0.0;
            disk->active_percent = active;

            const uint64_t read_ops =
                read_count >= baseline->read_count
                    ? read_count - baseline->read_count : 0U;
            const uint64_t write_ops =
                write_count >= baseline->write_count
                    ? write_count - baseline->write_count : 0U;
            const uint64_t operations = read_ops + write_ops;
            disk->read_response_ms = read_ops > 0U
                ? (double)read_time_delta / 10000.0 / (double)read_ops
                : 0.0;
            disk->write_response_ms = write_ops > 0U
                ? (double)write_time_delta / 10000.0 / (double)write_ops
                : 0.0;
            disk->average_response_ms = operations > 0U
                ? (double)(read_time_delta + write_time_delta) /
                    10000.0 / (double)operations
                : 0.0;
        }
    }

    baseline->read_bytes = read_bytes;
    baseline->write_bytes = write_bytes;
    baseline->read_count = read_count;
    baseline->write_count = write_count;
    baseline->read_time_100ns = read_time;
    baseline->write_time_100ns = write_time;
    baseline->valid = true;
}

static bool disk_identity_changed(
    const LsmDiskInfo *old_disks, size_t old_count,
    const LsmDiskInfo *new_disks, size_t new_count)
{
    if (old_count != new_count) return true;
    for (size_t index = 0U; index < new_count; index++) {
        if (strcmp(old_disks[index].instance_identity,
                   new_disks[index].instance_identity) != 0)
            return true;
    }
    return false;
}

static void enumerate_physical_disks(
    LsmMonitor *monitor, LsmWindowsMonitorBackendState *state,
    double elapsed)
{
    if (!monitor || !state) return;

    LsmDiskInfo old_disks[LSM_MAX_DISKS];
    const size_t old_count = monitor->disk_count;
    memcpy(old_disks, monitor->disks, sizeof(old_disks));

    LsmDiskInfo discovered[LSM_MAX_DISKS];
    memset(discovered, 0, sizeof(discovered));
    size_t count = 0U;

    for (unsigned number = 0U;
         number < LSM_MAX_DISKS && count < LSM_MAX_DISKS;
         number++) {
        char path[64];
        (void)snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%u", number);
        HANDLE disk = CreateFileA(
            path, 0U, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0U, NULL);
        if (disk == INVALID_HANDLE_VALUE)
            continue;

        LsmDiskInfo *info = &discovered[count];
        (void)snprintf(info->name, sizeof(info->name), "Disk %u", number);
        infiltratr_copy_string(
            info->instance_identity, sizeof(info->instance_identity), path);
        query_disk_size(disk, info);
        (void)query_disk_identity(disk, info);

        DISK_PERFORMANCE performance;
        if (query_disk_performance(disk, &performance))
            update_disk_performance(
                info, &state->disks[count], &performance, elapsed);
        else
            state->disks[count].valid = false;

        CloseHandle(disk);
        count++;
    }

    if (disk_identity_changed(old_disks, old_count, discovered, count)) {
        monitor->disk_generation++;
        monitor->topology_generation++;
    }

    memset(monitor->disks, 0, sizeof(monitor->disks));
    if (count > 0U)
        memcpy(monitor->disks, discovered, count * sizeof(discovered[0]));
    monitor->disk_count = count;
}

static int physical_disk_index(
    const LsmMonitor *monitor, DWORD disk_number)
{
    if (!monitor) return -1;
    char identity[64];
    (void)snprintf(
        identity, sizeof(identity),
        "\\\\.\\PhysicalDrive%lu", (unsigned long)disk_number);
    for (size_t index = 0U; index < monitor->disk_count; index++) {
        if (strcmp(monitor->disks[index].instance_identity, identity) == 0)
            return (int)index;
    }
    return -1;
}

static bool volume_is_system_volume(const char *mount_point)
{
    if (!mount_point || !mount_point[0]) return false;
    char windows_directory[MAX_PATH];
    const UINT length =
        GetWindowsDirectoryA(windows_directory, (UINT)sizeof(windows_directory));
    if (length == 0U || length >= (UINT)sizeof(windows_directory))
        return false;
    return strlen(mount_point) >= 2U &&
        windows_directory[0] == mount_point[0] &&
        windows_directory[1] == ':';
}

static void append_volume_to_disk(
    LsmMonitor *monitor, int disk_index, const char *volume_name,
    const char *mount_point, const char *filesystem,
    uint64_t total_bytes, uint64_t used_bytes, bool usage_known)
{
    if (!monitor || disk_index < 0 ||
        (size_t)disk_index >= monitor->disk_count)
        return;

    LsmDiskInfo *disk = &monitor->disks[(size_t)disk_index];
    if (disk->partition_count >= LSM_MAX_PARTITIONS) return;

    LsmPartitionInfo *partition =
        &disk->partitions[disk->partition_count++];
    memset(partition, 0, sizeof(*partition));
    infiltratr_copy_string(
        partition->device, sizeof(partition->device),
        volume_name && volume_name[0] ? volume_name : "Volume");
    infiltratr_copy_string(
        partition->mount_point, sizeof(partition->mount_point),
        mount_point && mount_point[0] ? mount_point : "N/A");
    infiltratr_copy_string(
        partition->filesystem, sizeof(partition->filesystem),
        filesystem && filesystem[0] ? filesystem : "N/A");
    partition->total_bytes = total_bytes;
    partition->used_bytes = used_bytes;
    partition->usage_known = usage_known;
    partition->used_percent = usage_known && total_bytes > 0U
        ? (unsigned)percent_u64(used_bytes, total_bytes) : 0U;
    if (mount_point && volume_is_system_volume(mount_point))
        disk->system_disk = true;
}

static void enumerate_disk_volumes(LsmMonitor *monitor)
{
    if (!monitor || monitor->disk_count == 0U) return;

    char volume_name[MAX_PATH];
    HANDLE search = FindFirstVolumeA(volume_name, (DWORD)sizeof(volume_name));
    if (search == INVALID_HANDLE_VALUE) return;

    bool more = true;
    while (more) {
        char volume_path[MAX_PATH];
        infiltratr_copy_string(volume_path, sizeof(volume_path), volume_name);
        const size_t volume_length = strlen(volume_path);
        if (volume_length > 0U &&
            volume_path[volume_length - 1U] == '\\')
            volume_path[volume_length - 1U] = '\0';

        HANDLE volume = CreateFileA(
            volume_path, 0U, FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0U, NULL);
        if (volume != INVALID_HANDLE_VALUE) {
            unsigned char extents_buffer[LSM_WINDOWS_EXTENTS_BUFFER];
            memset(extents_buffer, 0, sizeof(extents_buffer));
            DWORD bytes = 0U;
            if (DeviceIoControl(
                    volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                    NULL, 0U, extents_buffer,
                    (DWORD)sizeof(extents_buffer), &bytes, NULL) &&
                bytes >= sizeof(VOLUME_DISK_EXTENTS)) {
                const VOLUME_DISK_EXTENTS *extents =
                    (const VOLUME_DISK_EXTENTS *)extents_buffer;

                char mount_points[LSM_WINDOWS_VOLUME_BUFFER];
                DWORD required = 0U;
                mount_points[0] = '\0';
                if (!GetVolumePathNamesForVolumeNameA(
                        volume_name, mount_points,
                        (DWORD)sizeof(mount_points), &required))
                    mount_points[0] = '\0';

                char filesystem[64];
                filesystem[0] = '\0';
                (void)GetVolumeInformationA(
                    volume_name, NULL, 0U, NULL, NULL, NULL,
                    filesystem, (DWORD)sizeof(filesystem));

                ULARGE_INTEGER available;
                ULARGE_INTEGER total;
                ULARGE_INTEGER free_total;
                const bool usage_known =
                    GetDiskFreeSpaceExA(
                        volume_name, &available, &total, &free_total) != FALSE;
                const uint64_t total_bytes =
                    usage_known ? (uint64_t)total.QuadPart : 0U;
                const uint64_t free_bytes =
                    usage_known ? (uint64_t)free_total.QuadPart : 0U;
                const uint64_t used_bytes =
                    usage_known && total_bytes >= free_bytes
                        ? total_bytes - free_bytes : 0U;

                const char *display_mount =
                    mount_points[0] ? mount_points : volume_name;
                for (DWORD extent = 0U;
                     extent < extents->NumberOfDiskExtents;
                     extent++) {
                    const int index = physical_disk_index(
                        monitor, extents->Extents[extent].DiskNumber);
                    append_volume_to_disk(
                        monitor, index, volume_name, display_mount,
                        filesystem, total_bytes, used_bytes, usage_known);
                }
            }
            CloseHandle(volume);
        }

        if (!FindNextVolumeA(
                search, volume_name, (DWORD)sizeof(volume_name))) {
            more = false;
        }
    }
    FindVolumeClose(search);
}

static LsmWindowsNetBaseline *net_baseline(
    LsmWindowsMonitorBackendState *state, uint64_t key)
{
    if (!state) return NULL;
    for (size_t index = 0U; index < LSM_MAX_NETS; index++) {
        if (state->nets[index].valid && state->nets[index].key == key)
            return &state->nets[index];
    }
    for (size_t index = 0U; index < LSM_MAX_NETS; index++) {
        if (!state->nets[index].valid) {
            state->nets[index].key = key;
            return &state->nets[index];
        }
    }
    return NULL;
}

static void format_mac(
    const BYTE *address, ULONG length, char *destination,
    size_t destination_size)
{
    if (!destination || destination_size == 0U) return;
    destination[0] = '\0';
    if (!address || length == 0U) return;

    size_t used = 0U;
    for (ULONG index = 0U; index < length; index++) {
        const int written = snprintf(
            destination + used,
            destination_size > used ? destination_size - used : 0U,
            index == 0U ? "%02X" : ":%02X",
            (unsigned)address[index]);
        if (written < 0) {
            destination[0] = '\0';
            return;
        }
        const size_t count = (size_t)written;
        if (used + count >= destination_size) {
            destination[destination_size - 1U] = '\0';
            return;
        }
        used += count;
    }
}

static void populate_unicast_addresses(
    const IP_ADAPTER_ADDRESSES *adapter, LsmNetInfo *net)
{
    if (!adapter || !net) return;

    for (const IP_ADAPTER_UNICAST_ADDRESS *address =
             adapter->FirstUnicastAddress;
         address; address = address->Next) {
        if (!address->Address.lpSockaddr) continue;

        if (address->Address.lpSockaddr->sa_family == AF_INET &&
            !net->ipv4[0]) {
            const struct sockaddr_in *ipv4 =
                (const struct sockaddr_in *)address->Address.lpSockaddr;
            (void)InetNtopA(
                AF_INET, &ipv4->sin_addr,
                net->ipv4, (DWORD)sizeof(net->ipv4));
        } else if (address->Address.lpSockaddr->sa_family == AF_INET6 &&
                   !net->ipv6[0]) {
            const struct sockaddr_in6 *ipv6 =
                (const struct sockaddr_in6 *)address->Address.lpSockaddr;
            (void)InetNtopA(
                AF_INET6, &ipv6->sin6_addr,
                net->ipv6, (DWORD)sizeof(net->ipv6));
        }
    }
}

static bool network_identity_changed(
    const LsmNetInfo *old_nets, size_t old_count,
    const LsmNetInfo *new_nets, size_t new_count)
{
    if (old_count != new_count) return true;
    for (size_t index = 0U; index < new_count; index++) {
        if (strcmp(old_nets[index].name, new_nets[index].name) != 0)
            return true;
    }
    return false;
}

static void enumerate_networks(
    LsmMonitor *monitor, LsmWindowsMonitorBackendState *state,
    double elapsed)
{
    if (!monitor || !state) return;

    ULONG buffer_size = 16384U;
    IP_ADAPTER_ADDRESSES *addresses =
        (IP_ADAPTER_ADDRESSES *)malloc(buffer_size);
    if (!addresses) return;

    const ULONG flags =
        GAA_FLAG_INCLUDE_PREFIX |
        GAA_FLAG_SKIP_ANYCAST |
        GAA_FLAG_SKIP_MULTICAST |
        GAA_FLAG_SKIP_DNS_SERVER;
    ULONG result = GetAdaptersAddresses(
        AF_UNSPEC, flags, NULL, addresses, &buffer_size);
    if (result == ERROR_BUFFER_OVERFLOW) {
        IP_ADAPTER_ADDRESSES *larger =
            (IP_ADAPTER_ADDRESSES *)realloc(addresses, buffer_size);
        if (!larger) {
            free(addresses);
            return;
        }
        addresses = larger;
        result = GetAdaptersAddresses(
            AF_UNSPEC, flags, NULL, addresses, &buffer_size);
    }
    if (result != NO_ERROR) {
        free(addresses);
        return;
    }

    LsmNetInfo old_nets[LSM_MAX_NETS];
    const size_t old_count = monitor->net_count;
    memcpy(old_nets, monitor->nets, sizeof(old_nets));

    LsmNetInfo discovered[LSM_MAX_NETS];
    memset(discovered, 0, sizeof(discovered));
    size_t count = 0U;

    for (const IP_ADAPTER_ADDRESSES *adapter = addresses;
         adapter && count < LSM_MAX_NETS; adapter = adapter->Next) {
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;

        MIB_IF_ROW2 row;
        memset(&row, 0, sizeof(row));
        row.InterfaceLuid = adapter->Luid;
        if (GetIfEntry2(&row) != NO_ERROR)
            continue;

        LsmNetInfo *net = &discovered[count];
        wide_to_utf8(
            adapter->FriendlyName, net->name, sizeof(net->name));
        if (!net->name[0] && adapter->AdapterName)
            infiltratr_copy_string(
                net->name, sizeof(net->name), adapter->AdapterName);
        wide_to_utf8(
            adapter->Description, net->product, sizeof(net->product));

        format_mac(
            adapter->PhysicalAddress, adapter->PhysicalAddressLength,
            net->mac, sizeof(net->mac));
        populate_unicast_addresses(adapter, net);

        net->wireless = adapter->IfType == IF_TYPE_IEEE80211;
        infiltratr_copy_string(
            net->connection_state, sizeof(net->connection_state),
            row.OperStatus == IfOperStatusUp ? "Connected" : "Disconnected");
        const uint64_t link_bits =
            row.TransmitLinkSpeed > row.ReceiveLinkSpeed
                ? row.TransmitLinkSpeed : row.ReceiveLinkSpeed;
        net->link_speed_mbps = (double)link_bits / 1000000.0;
        net->rx_bytes_total = row.InOctets;
        net->tx_bytes_total = row.OutOctets;

        const uint64_t key = adapter->Luid.Value;
        LsmWindowsNetBaseline *baseline = net_baseline(state, key);
        if (baseline) {
            if (baseline->valid && elapsed > 0.0) {
                net->rx_bytes_per_sec =
                    rate_u64(row.InOctets, baseline->rx_bytes, elapsed);
                net->tx_bytes_per_sec =
                    rate_u64(row.OutOctets, baseline->tx_bytes, elapsed);
            }
            baseline->key = key;
            baseline->rx_bytes = row.InOctets;
            baseline->tx_bytes = row.OutOctets;
            baseline->valid = true;
        }

        if (link_bits > 0U) {
            const long double current_bits =
                ((long double)net->rx_bytes_per_sec +
                 (long double)net->tx_bytes_per_sec) * 8.0L;
            long double utilisation =
                (current_bits * 100.0L) / (long double)link_bits;
            if (utilisation > 100.0L) utilisation = 100.0L;
            if (utilisation < 0.0L) utilisation = 0.0L;
            net->utilisation_percent = (double)utilisation;
            net->utilisation_available = true;
        }
        count++;
    }

    if (network_identity_changed(old_nets, old_count, discovered, count))
        monitor->topology_generation++;

    memset(monitor->nets, 0, sizeof(monitor->nets));
    if (count > 0U)
        memcpy(monitor->nets, discovered, count * sizeof(discovered[0]));
    monitor->net_count = count;
    free(addresses);
}

static bool gpu_identity_changed(
    const LsmGpuInfo *old_gpus, size_t old_count,
    const LsmGpuInfo *new_gpus, size_t new_count)
{
    if (old_count != new_count) return true;
    for (size_t index = 0U; index < new_count; index++) {
        if (strcmp(old_gpus[index].platform_identity,
                   new_gpus[index].platform_identity) != 0)
            return true;
    }
    return false;
}

static bool gpu_already_present(
    const LsmGpuInfo *gpus, size_t count, const char *identity)
{
    if (!gpus || !identity || !identity[0]) return false;
    for (size_t index = 0U; index < count; index++) {
        if (strcmp(gpus[index].platform_identity, identity) == 0)
            return true;
    }
    return false;
}

static void enumerate_gpus(LsmMonitor *monitor)
{
    if (!monitor) return;

    LsmGpuInfo old_gpus[LSM_MAX_GPUS];
    const size_t old_count = monitor->gpu_count;
    memcpy(old_gpus, monitor->gpus, sizeof(old_gpus));

    LsmGpuInfo discovered[LSM_MAX_GPUS];
    memset(discovered, 0, sizeof(discovered));
    size_t count = 0U;

    for (DWORD device_index = 0U;
         count < LSM_MAX_GPUS; device_index++) {
        DISPLAY_DEVICEA device;
        memset(&device, 0, sizeof(device));
        device.cb = sizeof(device);
        if (!EnumDisplayDevicesA(NULL, device_index, &device, 0U))
            break;
        if ((device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) != 0U)
            continue;
        if (!device.DeviceString[0])
            continue;

        const char *identity =
            device.DeviceID[0] ? device.DeviceID :
            (device.DeviceKey[0] ? device.DeviceKey : device.DeviceName);
        if (gpu_already_present(discovered, count, identity))
            continue;

        LsmGpuInfo *gpu = &discovered[count++];
        infiltratr_copy_string(gpu->name, sizeof(gpu->name), device.DeviceString);
        infiltratr_copy_string(
            gpu->display_identifier, sizeof(gpu->display_identifier),
            device.DeviceName);
        infiltratr_copy_string(
            gpu->platform_identity, sizeof(gpu->platform_identity), identity);
        infiltratr_copy_string(
            gpu->metrics_source, sizeof(gpu->metrics_source),
            "Windows display adapter identification");
        gpu->supported_metrics = false;
        gpu->utilization_available = false;
        gpu->engine_metrics_capable = false;
    }

    if (gpu_identity_changed(old_gpus, old_count, discovered, count))
        monitor->topology_generation++;

    memset(monitor->gpus, 0, sizeof(monitor->gpus));
    if (count > 0U)
        memcpy(monitor->gpus, discovered, count * sizeof(discovered[0]));
    monitor->gpu_count = count;
}

static void refresh_topology_and_devices(
    LsmMonitor *monitor, LsmWindowsMonitorBackendState *state,
    double elapsed, bool force)
{
    if (!monitor || !state) return;

    const ULONGLONG now = GetTickCount64();
    const bool due =
        force || state->topology_refresh_requested ||
        state->last_topology_tick == 0ULL ||
        now - state->last_topology_tick >= LSM_WINDOWS_TOPOLOGY_REFRESH_MS;

    enumerate_physical_disks(monitor, state, elapsed);
    enumerate_disk_volumes(monitor);
    enumerate_networks(monitor, state, elapsed);
    if (due) {
        enumerate_gpus(monitor);
        state->last_topology_tick = now;
        state->topology_refresh_requested = false;
    }
}

bool lsm_monitor_platform_init(LsmMonitor *monitor)
{
    if (!monitor) return false;
    memset(monitor, 0, sizeof(*monitor));

    LsmWindowsMonitorBackendState *state =
        (LsmWindowsMonitorBackendState *)calloc(1U, sizeof(*state));
    if (!state) return false;
    monitor->backend_state = state;

    WSADATA winsock;
    memset(&winsock, 0, sizeof(winsock));
    if (WSAStartup(MAKEWORD(2, 2), &winsock) == 0)
        state->winsock_started = true;

    populate_cpu_identity(monitor);
    if (!update_cpu_snapshot(monitor, state) ||
        !update_memory_snapshot(monitor)) {
        lsm_monitor_platform_destroy(monitor);
        return false;
    }

    state->previous_sample_tick = GetTickCount64();
    refresh_topology_and_devices(monitor, state, 0.0, true);
    return true;
}

bool lsm_monitor_platform_update(LsmMonitor *monitor)
{
    if (!monitor || !monitor->backend_state) return false;
    LsmWindowsMonitorBackendState *state =
        (LsmWindowsMonitorBackendState *)monitor->backend_state;

    const ULONGLONG now = GetTickCount64();
    const double elapsed =
        state->previous_sample_tick > 0ULL && now >= state->previous_sample_tick
            ? (double)(now - state->previous_sample_tick) / 1000.0
            : 0.0;
    state->previous_sample_tick = now;

    const bool cpu_ok = update_cpu_snapshot(monitor, state);
    const bool memory_ok = update_memory_snapshot(monitor);
    refresh_topology_and_devices(monitor, state, elapsed, false);
    return cpu_ok && memory_ok;
}

void lsm_monitor_platform_request_topology_refresh(LsmMonitor *monitor)
{
    if (!monitor || !monitor->backend_state) return;
    LsmWindowsMonitorBackendState *state =
        (LsmWindowsMonitorBackendState *)monitor->backend_state;
    state->topology_refresh_requested = true;
}

void lsm_monitor_platform_destroy(LsmMonitor *monitor)
{
    if (!monitor) return;
    LsmWindowsMonitorBackendState *state =
        (LsmWindowsMonitorBackendState *)monitor->backend_state;
    if (state && state->winsock_started)
        WSACleanup();
    free(state);
    monitor->backend_state = NULL;
}
