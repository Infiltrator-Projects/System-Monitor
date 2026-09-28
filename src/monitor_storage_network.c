// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file monitor_storage_network.c
 * @brief Physical disks, partitions and network-interface collection.
 *
 * Device membership and mount identity come from system_sources.c. Network
 * traffic uses rtnetlink while block activity retains the kernel diskstats ABI.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "monitor_linux_internal.h"
#include "common.h"
#include "disk_accounting.h"
#include "system_sources.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <math.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

static int compare_disk_names(const void *left, const void *right)
{
    const LsmDiskInfo *a = left;
    const LsmDiskInfo *b = right;
    return strcmp(a->name, b->name);
}

static LsmLinuxDiskState *find_disk_state(LsmMonitor *monitor,
                                          const char *name)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state || !name) return NULL;
    for (size_t index = 0U; index < state->disk_count; index++)
        if (strcmp(state->disks[index].name, name) == 0)
            return &state->disks[index];
    return NULL;
}

static bool disk_instance_matches(const char *left, const char *right)
{
    return left && right && left[0] && right[0] &&
           strcmp(left, right) == 0;
}

static void reconcile_disk_states(LsmMonitor *monitor,
                                  const LsmDiskInfo *disks, size_t count)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return;
    LsmLinuxDiskState next[LSM_MAX_DISKS] = {0};
    for (size_t index = 0U; index < count && index < LSM_MAX_DISKS; index++) {
        LsmLinuxDiskState *old = find_disk_state(monitor, disks[index].name);
        if (old && disk_instance_matches(
                old->instance_identity, disks[index].instance_identity))
            next[index] = *old;
        lsm_copy_string(next[index].name, sizeof(next[index].name),
                        disks[index].name);
        lsm_copy_string(next[index].instance_identity,
                        sizeof(next[index].instance_identity),
                        disks[index].instance_identity);
    }
    memcpy(state->disks, next, sizeof(next));
    state->disk_count = count < LSM_MAX_DISKS ? count : LSM_MAX_DISKS;
}

static const LsmDiskInfo *find_old_disk(const LsmMonitor *monitor,
                                        const LsmDiskInfo *disk)
{
    if (!monitor || !disk) return NULL;
    for (size_t index = 0U; index < monitor->disk_count; index++) {
        const LsmDiskInfo *old = &monitor->disks[index];
        if (strcmp(old->name, disk->name) == 0 &&
            disk_instance_matches(old->instance_identity,
                                  disk->instance_identity))
            return old;
    }
    return NULL;
}


static LsmDiskInfo *find_disk_in_set(LsmDiskInfo *disks, size_t disk_count,
                                     const char *name)
{
    if (!disks || !name) return NULL;
    for (size_t index = 0; index < disk_count; index++)
        if (strcmp(disks[index].name, name) == 0) return &disks[index];
    return NULL;
}

static void append_partition(LsmDiskInfo *disk, const char *device,
                             const char *mount_point, const char *filesystem,
                             uint64_t partition_size, bool mounted)
{
    if (!disk || !device || disk->partition_count >= LSM_MAX_PARTITIONS) return;

    LsmPartitionInfo *partition = &disk->partitions[disk->partition_count++];
    lsm_copy_string(partition->device, sizeof(partition->device), device);
    lsm_copy_string(partition->mount_point, sizeof(partition->mount_point),
                mount_point ? mount_point : "");
    lsm_copy_string(partition->filesystem, sizeof(partition->filesystem),
                filesystem && *filesystem ? filesystem : "partition");
    partition->total_bytes = partition_size;
    partition->used_bytes = 0;
    partition->used_percent = 0;
    partition->usage_known = false;

    if (!mounted || !mount_point || !*mount_point) return;
    struct statvfs information;
    if (statvfs(mount_point, &information) != 0) return;
    const uint64_t block_size = information.f_frsize ?
        (uint64_t)information.f_frsize : (uint64_t)information.f_bsize;
    uint64_t total = 0U;
    uint64_t free_bytes = 0U;
    if (!lsm_u64_multiply_checked(
            (uint64_t)information.f_blocks, block_size, &total) ||
        !lsm_u64_multiply_checked(
            (uint64_t)information.f_bfree, block_size, &free_bytes))
        return;
    const uint64_t used = total >= free_bytes ? total - free_bytes : 0U;
    partition->total_bytes = total;
    partition->used_bytes = used;
    partition->used_percent = total > 0
        ? (unsigned)floor(lsm_percent_u64(used, total) + 0.5) : 0;
    partition->usage_known = true;
}

static int compare_partition_devices(const void *left, const void *right)
{
    const LsmPartitionInfo *a = left;
    const LsmPartitionInfo *b = right;
    return strverscmp(a->device, b->device);
}


/**
 * Populate every disk from one native sysfs/mountinfo record set.
 *
 * Joining the shared record set once avoids repeating complete partition and
 * mount scans for each physical disk.
 */
static bool update_all_disk_partitions(LsmMonitor *monitor,
                                        LsmDiskInfo *disks,
                                        size_t disk_count)
{
    const size_t capacity = 2048U;
    LsmPartitionRecord *partitions = calloc(capacity, sizeof(*partitions));
    if (!partitions) return false;
    size_t count = 0U;
    if (!lsm_sources_list_partitions_checked(
            monitor_system_sources(monitor), partitions, capacity, &count)) {
        free(partitions);
        return false;
    }
    for (size_t index = 0; index < disk_count; index++) {
        disks[index].partition_count = 0U;
        disks[index].system_disk = false;
        memset(disks[index].partitions, 0, sizeof(disks[index].partitions));
    }
    for (size_t index = 0; index < count; index++) {
        LsmDiskInfo *disk = find_disk_in_set(disks, disk_count, partitions[index].parent_disk);
        if (!disk || disk->partition_count >= LSM_MAX_PARTITIONS) continue;
        append_partition(disk, partitions[index].device, partitions[index].mount_point,
                         partitions[index].filesystem, partitions[index].size_bytes,
                         partitions[index].mounted);
        if (partitions[index].mounted && strcmp(partitions[index].mount_point, "/") == 0)
            disk->system_disk = true;
    }
    free(partitions);
    for (size_t index = 0; index < disk_count; index++)
        qsort(disks[index].partitions, disks[index].partition_count,
              sizeof(disks[index].partitions[0]), compare_partition_devices);
    return true;
}

/**
 * Rescan physical block devices through native block-class sysfs. Existing
 * counter baselines are preserved only when both name and instance identity
 * match, because Linux can reuse a block-device name after hotplug.
 */
static bool refresh_disks(LsmMonitor *monitor)
{
    LsmDiskInfo *discovered = calloc(LSM_MAX_DISKS, sizeof(*discovered));
    if (!discovered) return false;
    size_t discovered_count = 0;

    LsmBlockDeviceRecord records[LSM_MAX_DISKS] = {0};
    size_t record_count = 0U;
    if (!lsm_sources_list_block_devices_checked(
            monitor_system_sources(monitor), records, LSM_MAX_DISKS,
            &record_count)) {
        free(discovered);
        return false;
    }
    for (size_t index = 0; index < record_count; index++) {
        LsmDiskInfo *disk = &discovered[discovered_count++];
        lsm_copy_string(disk->name, sizeof(disk->name), records[index].name);
        lsm_copy_string(disk->instance_identity,
                        sizeof(disk->instance_identity),
                        records[index].instance_identity);
        lsm_copy_string(disk->model, sizeof(disk->model), records[index].model);
        lsm_copy_string(disk->media_type, sizeof(disk->media_type),
                        records[index].media_type);
        lsm_copy_string(disk->connection_type,
                        sizeof(disk->connection_type),
                        records[index].connection_type);
        disk->size_bytes = records[index].size_bytes;
    }

    for (size_t index = 0; index < discovered_count; index++) {
        LsmDiskInfo *disk = &discovered[index];
        const LsmDiskInfo *old = find_old_disk(monitor, disk);
        if (old) {
            disk->read_bytes_per_sec = old->read_bytes_per_sec;
            disk->write_bytes_per_sec = old->write_bytes_per_sec;
            disk->active_percent = old->active_percent;
            disk->average_response_ms = old->average_response_ms;
            disk->read_response_ms = old->read_response_ms;
            disk->write_response_ms = old->write_response_ms;
            disk->queue_length = old->queue_length;
            disk->read_bytes_total = old->read_bytes_total;
            disk->write_bytes_total = old->write_bytes_total;
            disk->in_progress_operations = old->in_progress_operations;
        }
    }

    if (!update_all_disk_partitions(monitor, discovered, discovered_count)) {
        free(discovered);
        return false;
    }
    qsort(discovered, discovered_count, sizeof(discovered[0]), compare_disk_names);
    reconcile_disk_states(monitor, discovered, discovered_count);
    bool changed = discovered_count != monitor->disk_count;
    if (!changed) {
        for (size_t i = 0; i < discovered_count; i++) {
            if (strcmp(discovered[i].name, monitor->disks[i].name) != 0 ||
                !disk_instance_matches(
                    discovered[i].instance_identity,
                    monitor->disks[i].instance_identity)) {
                changed = true;
                break;
            }
        }
    }
    memcpy(monitor->disks, discovered, discovered_count * sizeof(discovered[0]));
    if (discovered_count < LSM_MAX_DISKS)
        memset(&monitor->disks[discovered_count], 0,
               (LSM_MAX_DISKS - discovered_count) * sizeof(monitor->disks[0]));
    monitor->disk_count = discovered_count;
    if (changed || monitor->disk_generation == 0) {
        monitor->disk_generation++;
        monitor->topology_generation++;
    }
    free(discovered);
    return true;
}

static LsmDiskInfo *find_disk(LsmMonitor *monitor, const char *name)
{
    for (size_t i = 0; i < monitor->disk_count; i++)
        if (strcmp(monitor->disks[i].name, name) == 0) return &monitor->disks[i];
    return NULL;
}

static void update_disks(LsmMonitor *monitor, double elapsed)
{
    FILE *file = fopen("/proc/diskstats", "r");
    if (!file) {
        for (size_t index = 0U; index < monitor->disk_count; index++) {
            LsmLinuxDiskState *state = find_disk_state(monitor, monitor->disks[index].name);
            if (state) lsm_disk_accounting_update(
                &monitor->disks[index], &state->accounting, NULL, elapsed);
        }
        return;
    }
    bool sampled[LSM_MAX_DISKS] = {false};
    LsmDiskCounters snapshots[LSM_MAX_DISKS];
    memset(snapshots, 0, sizeof(snapshots));
    char line[512], name[64];
    unsigned major=0, minor=0;
    unsigned long long reads=0, read_merged=0, read_sectors=0, read_ms=0;
    unsigned long long writes=0, write_merged=0, write_sectors=0, write_ms=0;
    unsigned long long in_progress=0, io_ms=0, weighted_ms=0;
    while (fgets(line, sizeof(line), file)) {
        int count = sscanf(line, "%u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                           &major,&minor,name,&reads,&read_merged,&read_sectors,&read_ms,
                           &writes,&write_merged,&write_sectors,&write_ms,&in_progress,&io_ms,&weighted_ms);
        if (count < 14) continue;
        LsmDiskInfo *disk = find_disk(monitor, name);
        if (!disk) continue;
        size_t index=(size_t)(disk-monitor->disks);
        sampled[index]=true;
        snapshots[index]=(LsmDiskCounters){
            .read_operations=reads,.read_sectors=read_sectors,.read_ms=read_ms,
            .write_operations=writes,.write_sectors=write_sectors,.write_ms=write_ms,
            .in_progress_operations=in_progress,.io_ms=io_ms,.weighted_io_ms=weighted_ms
        };
    }
    const bool complete=!ferror(file) && fclose(file)==0;
    for(size_t index=0U;index<monitor->disk_count;index++){
        LsmLinuxDiskState *state=find_disk_state(monitor,monitor->disks[index].name);
        if(state) lsm_disk_accounting_update(
            &monitor->disks[index],&state->accounting,
            complete&&sampled[index]?&snapshots[index]:NULL,elapsed);
    }
}

static int compare_network_names(const void *left, const void *right)
{
    const LsmNetInfo *a = left;
    const LsmNetInfo *b = right;
    return strcmp(a->name, b->name);
}

static bool network_instance_matches(const char *left_identity,
                                     const char *left_name,
                                     const char *right_identity,
                                     const char *right_name)
{
    if (left_identity && right_identity &&
        left_identity[0] && right_identity[0])
        return strcmp(left_identity, right_identity) == 0;
    return left_name && right_name && strcmp(left_name, right_name) == 0;
}

static LsmLinuxNetworkState *find_network_state(LsmMonitor *monitor,
                                                const LsmNetInfo *net)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state || !net) return NULL;
    for (size_t index = 0U; index < state->network_count; index++)
        if (network_instance_matches(
                state->networks[index].instance_identity,
                state->networks[index].name,
                net->instance_identity, net->name))
            return &state->networks[index];
    return NULL;
}

static void reconcile_network_states(LsmMonitor *monitor,
                                     const LsmNetInfo *nets, size_t count)
{
    LsmLinuxMonitorBackendState *state = monitor_backend_state(monitor);
    if (!state) return;
    LsmLinuxNetworkState next[LSM_MAX_NETS] = {0};
    for (size_t index = 0U; index < count && index < LSM_MAX_NETS; index++) {
        LsmLinuxNetworkState *old = find_network_state(monitor, &nets[index]);
        if (old) next[index] = *old;
        lsm_copy_string(next[index].name, sizeof(next[index].name),
                        nets[index].name);
        lsm_copy_string(next[index].instance_identity,
                        sizeof(next[index].instance_identity),
                        nets[index].instance_identity);
    }
    memcpy(state->networks, next, sizeof(next));
    state->network_count = count < LSM_MAX_NETS ? count : LSM_MAX_NETS;
}

static const LsmNetInfo *find_old_network(const LsmMonitor *monitor,
                                          const LsmNetInfo *net)
{
    if (!monitor || !net) return NULL;
    for (size_t index = 0; index < monitor->net_count; index++)
        if (network_instance_matches(
                monitor->nets[index].instance_identity,
                monitor->nets[index].name,
                net->instance_identity, net->name))
            return &monitor->nets[index];
    return NULL;
}

/** Rescan active non-loopback interfaces while retaining rate baselines. */
static bool refresh_networks(LsmMonitor *monitor)
{
    LsmNetInfo *discovered = calloc(LSM_MAX_NETS, sizeof(*discovered));
    if (!discovered) return false;
    size_t discovered_count = 0;

    LsmNetworkRecord records[LSM_MAX_NETS] = {0};
    size_t count = 0U;
    if (!lsm_sources_list_networks_checked(
            monitor_system_sources(monitor), records, LSM_MAX_NETS, &count)) {
        free(discovered);
        return false;
    }
    for (size_t index = 0; index < count; index++) {
        LsmNetInfo *net = &discovered[discovered_count++];
        lsm_copy_string(net->name, sizeof(net->name), records[index].name);
        lsm_copy_string(net->instance_identity,
                        sizeof(net->instance_identity),
                        records[index].instance_identity);
        lsm_copy_string(net->mac, sizeof(net->mac), records[index].mac);
        lsm_copy_string(net->product, sizeof(net->product), records[index].product);
        lsm_copy_string(net->vendor, sizeof(net->vendor), records[index].vendor);
        net->wireless = records[index].wireless;
    }

    for (size_t index = 0; index < discovered_count; index++) {
        LsmNetInfo identity = discovered[index];
        const LsmNetInfo *old = find_old_network(monitor, &identity);
        if (!old) continue;
        discovered[index] = *old;
        lsm_copy_string(discovered[index].name, sizeof(discovered[index].name), identity.name);
        lsm_copy_string(discovered[index].instance_identity,
                        sizeof(discovered[index].instance_identity),
                        identity.instance_identity);
        lsm_copy_string(discovered[index].mac, sizeof(discovered[index].mac), identity.mac);
        lsm_copy_string(discovered[index].product, sizeof(discovered[index].product),
                        identity.product);
        lsm_copy_string(discovered[index].vendor, sizeof(discovered[index].vendor),
                        identity.vendor);
        discovered[index].wireless = identity.wireless;
    }

    qsort(discovered, discovered_count, sizeof(discovered[0]), compare_network_names);
    reconcile_network_states(monitor, discovered, discovered_count);
    bool changed = discovered_count != monitor->net_count;
    if (!changed) {
        for (size_t index = 0; index < discovered_count; index++) {
            if (!network_instance_matches(
                    discovered[index].instance_identity,
                    discovered[index].name,
                    monitor->nets[index].instance_identity,
                    monitor->nets[index].name)) {
                changed = true;
                break;
            }
        }
    }

    memcpy(monitor->nets, discovered, discovered_count * sizeof(discovered[0]));
    if (discovered_count < LSM_MAX_NETS)
        memset(&monitor->nets[discovered_count], 0,
               (LSM_MAX_NETS - discovered_count) * sizeof(monitor->nets[0]));
    monitor->net_count = discovered_count;
    if (changed) monitor->topology_generation++;
    free(discovered);
    return true;
}

static void update_interface_addresses(LsmMonitor *monitor)
{
    for (size_t i = 0; i < monitor->net_count; i++) {
        monitor->nets[i].ipv4[0] = '\0';
        monitor->nets[i].ipv6[0] = '\0';
    }
    struct ifaddrs *addresses = NULL;
    if (getifaddrs(&addresses) != 0) return;
    for (struct ifaddrs *it = addresses; it; it = it->ifa_next) {
        if (!it->ifa_addr) continue;
        int family = it->ifa_addr->sa_family;
        for (size_t i = 0; i < monitor->net_count; i++) {
            LsmNetInfo *net = &monitor->nets[i];
            if (strcmp(net->name, it->ifa_name) != 0) continue;
            void *address = NULL;
            char *destination = NULL;
            size_t size = 0;
            if (family == AF_INET && net->ipv4[0] == '\0') {
                address = &((struct sockaddr_in *)it->ifa_addr)->sin_addr;
                destination = net->ipv4; size = sizeof(net->ipv4);
            } else if (family == AF_INET6 && net->ipv6[0] == '\0') {
                address = &((struct sockaddr_in6 *)it->ifa_addr)->sin6_addr;
                destination = net->ipv6; size = sizeof(net->ipv6);
            }
            if (address) inet_ntop(family, address, destination, (socklen_t)size);
        }
    }
    freeifaddrs(addresses);
}

static void update_network_link_details(LsmNetInfo *net)
{
    if (!net || !net->name[0]) return;
    char path[LSM_PATH_LEN];
    char state[32] = "";
    (void)snprintf(path, sizeof(path), "/sys/class/net/%.63s/operstate",
                   net->name);
    if (lsm_read_text_file(path, state, sizeof(state)) && state[0]) {
        state[0] = (char)lsm_ascii_to_upper((unsigned char)state[0]);
        lsm_copy_string(net->connection_state,
                        sizeof(net->connection_state), state);
    } else {
        lsm_copy_string(net->connection_state,
                        sizeof(net->connection_state), "Unknown");
    }

    if (!net->wireless) {
        net->link_speed_mbps = 0.0;
        (void)snprintf(path, sizeof(path), "/sys/class/net/%.63s/speed",
                       net->name);
        const double speed = lsm_read_double_or_nan(path);
        if (isfinite(speed) && speed > 0.0) net->link_speed_mbps = speed;
    }
}

static void update_networks(LsmMonitor *monitor, double elapsed,
                             bool refresh_link_metadata)
{
    LsmNetworkCounterRecord counters[LSM_MAX_NETS] = {0};
    size_t counter_count = 0U;
    const bool snapshot_ok = lsm_sources_read_network_counters_checked(
        monitor_system_sources(monitor), counters, LSM_MAX_NETS, &counter_count);
    for (size_t i=0;i<monitor->net_count;i++) {
        LsmNetInfo *net=&monitor->nets[i];
        uint64_t rx=net->rx_bytes_total, tx=net->tx_bytes_total;
        bool available=false;
        if(snapshot_ok) for(size_t j=0;j<counter_count;j++){
            if(strcmp(net->name,counters[j].name)!=0 ||
               !network_instance_matches(net->instance_identity,net->name,
                                         counters[j].instance_identity,counters[j].name))
                continue;
            rx=counters[j].rx_bytes; tx=counters[j].tx_bytes; available=true; break;
        }
        LsmLinuxNetworkState *state=find_network_state(monitor,net);
        net->rx_bytes_per_sec=NAN; net->tx_bytes_per_sec=NAN;
        if(available && state && state->initialized && elapsed > 0.0){
            double rr=0.0,tr=0.0;
            if(lsm_u64_counter_rate(rx,state->previous_rx,1.0L,elapsed,&rr))
                net->rx_bytes_per_sec=rr;
            if(lsm_u64_counter_rate(tx,state->previous_tx,1.0L,elapsed,&tr))
                net->tx_bytes_per_sec=tr;
        }
        if(available){net->rx_bytes_total=rx;net->tx_bytes_total=tx;}
        if(state){
            if(available){state->previous_rx=rx;state->previous_tx=tx;}
            state->initialized=available;
        }
        if(refresh_link_metadata || !net->connection_state[0]) update_network_link_details(net);
        LsmLinuxMonitorBackendState *backend=monitor_backend_state(monitor);
        if(backend && backend->wifi_metadata) lsm_wifi_metadata_refresh(backend->wifi_metadata,net);
        if(available && isfinite(net->rx_bytes_per_sec) && isfinite(net->tx_bytes_per_sec) &&
           net->link_speed_mbps>0.0){
            long double busiest=fmaxl((long double)net->rx_bytes_per_sec,
                                      (long double)net->tx_bytes_per_sec);
            long double capacity=(long double)net->link_speed_mbps*1000000.0L/8.0L;
            net->utilisation_percent=(double)fminl(100.0L,fmaxl(0.0L,busiest*100.0L/capacity));
            net->utilisation_available=true;
        } else {
            net->utilisation_percent=NAN;
            net->utilisation_available=false;
        }
    }
}


bool lsm_storage_initialise(LsmMonitor *monitor)
{
    if (!monitor) return false;
    if (!refresh_disks(monitor)) return false;
    if (!refresh_networks(monitor)) return false;
    update_interface_addresses(monitor);
    return true;
}

void lsm_storage_update(LsmMonitor *monitor, double elapsed,
                        bool refresh_topology)
{
    if (!monitor) return;
    if (refresh_topology) {
        (void)refresh_disks(monitor);
        if (lsm_sources_network_topology_changed(
                monitor_system_sources(monitor))) {
            (void)refresh_networks(monitor);
            update_interface_addresses(monitor);
        }
    }
    update_disks(monitor, elapsed);
    update_networks(monitor, elapsed, refresh_topology);
}
