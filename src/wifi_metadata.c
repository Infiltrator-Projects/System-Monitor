// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file wifi_metadata.c
 * @brief Direct wireless-driver metadata cache using Linux ioctls.
 *
 * SSID, access-point identity, signal quality, frequency and negotiated bitrate
 * are requested directly from the network driver through the Linux Wireless
 * Extensions ioctl ABI.  No NetworkManager process, D-Bus round trip, shell
 * command or external wireless library is required.  Results are cached for a
 * short interval because these descriptive fields do not need graph-rate
 * sampling.
 *
 * Cache entries are explicitly invalidated after a failed refresh so a broken
 * or disconnected driver query cannot keep presenting stale link metadata.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "wifi_metadata.h"

#include "common.h"

#include <errno.h>
#include <linux/wireless.h>
#include <math.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#define LSM_WIFI_METADATA_INTERVAL_SECONDS 5.0

typedef struct {
    char name[64];
    char ssid[LSM_NAME_LEN];
    char access_point[32];
    double signal_percent;
    double signal_dbm;
    double frequency_mhz;
    double link_speed_mbps;
    double last_sampled;
    bool valid;
} LsmWifiCacheRecord;

struct LsmWifiMetadata {
    int socket_fd;
    LsmWifiCacheRecord records[LSM_MAX_NETS];
};

static LsmWifiCacheRecord *find_record(LsmWifiMetadata *metadata,
                                       const char *name, bool create)
{
    if (!metadata || !name || !*name) return NULL;
    LsmWifiCacheRecord *empty = NULL;
    for (size_t index = 0U; index < LSM_MAX_NETS; index++) {
        LsmWifiCacheRecord *record = &metadata->records[index];
        if (record->name[0] && strcmp(record->name, name) == 0) return record;
        if (!record->name[0] && !empty) empty = record;
    }
    if (create && empty) {
        lsm_copy_string(empty->name, sizeof(empty->name), name);
        empty->signal_dbm = NAN;
        return empty;
    }
    return NULL;
}

static void invalidate_record(LsmWifiCacheRecord *record, double sampled_at)
{
    if (!record) return;
    char name[sizeof(record->name)];
    lsm_copy_string(name, sizeof(name), record->name);
    memset(record, 0, sizeof(*record));
    lsm_copy_string(record->name, sizeof(record->name), name);
    record->signal_dbm = NAN;
    record->last_sampled = sampled_at;
}

static void initialise_request(struct iwreq *request, const char *interface_name)
{
    memset(request, 0, sizeof(*request));
    lsm_copy_string(request->ifr_name, sizeof(request->ifr_name), interface_name);
}

static bool read_essid(int descriptor, const char *interface_name,
                       char *destination, size_t destination_size)
{
    if (!destination || destination_size == 0U) return false;
    char value[IW_ESSID_MAX_SIZE + 1U];
    memset(value, 0, sizeof(value));

    struct iwreq request;
    initialise_request(&request, interface_name);
    request.u.essid.pointer = value;
    request.u.essid.length = IW_ESSID_MAX_SIZE;
    request.u.essid.flags = 0;
    if (ioctl(descriptor, SIOCGIWESSID, &request) != 0) return false;

    size_t length = request.u.essid.length;
    if (length > IW_ESSID_MAX_SIZE) length = IW_ESSID_MAX_SIZE;
    while (length > 0U && value[length - 1U] == '\0') length--;
    if (length >= destination_size) length = destination_size - 1U;
    memcpy(destination, value, length);
    destination[length] = '\0';
    return length > 0U;
}

static bool read_access_point(int descriptor, const char *interface_name,
                              char *destination, size_t destination_size)
{
    struct iwreq request;
    initialise_request(&request, interface_name);
    if (ioctl(descriptor, SIOCGIWAP, &request) != 0) return false;

    const unsigned char *address =
        (const unsigned char *)(const void *)request.u.ap_addr.sa_data;
    bool nonzero = false;
    for (size_t index = 0U; index < 6U; index++)
        nonzero = nonzero || address[index] != 0U;
    if (!nonzero) return false;

    (void)snprintf(destination, destination_size,
                   "%02x:%02x:%02x:%02x:%02x:%02x",
                   address[0], address[1], address[2],
                   address[3], address[4], address[5]);
    return true;
}

static bool read_frequency(int descriptor, const char *interface_name,
                           double *frequency_mhz)
{
    struct iwreq request;
    initialise_request(&request, interface_name);
    if (ioctl(descriptor, SIOCGIWFREQ, &request) != 0) return false;

    double hertz = (double)request.u.freq.m;
    int exponent = request.u.freq.e;
    while (exponent > 0) {
        hertz *= 10.0;
        exponent--;
    }
    while (exponent < 0) {
        hertz /= 10.0;
        exponent++;
    }
    if (!isfinite(hertz) || hertz <= 0.0) return false;
    *frequency_mhz = hertz / 1000000.0;
    return true;
}

static bool read_bitrate(int descriptor, const char *interface_name,
                         double *bitrate_mbps)
{
    struct iwreq request;
    initialise_request(&request, interface_name);
    if (ioctl(descriptor, SIOCGIWRATE, &request) != 0 ||
        request.u.bitrate.value <= 0)
        return false;
    *bitrate_mbps = (double)request.u.bitrate.value / 1000000.0;
    return true;
}

static double quality_maximum(int descriptor, const char *interface_name)
{
    struct iw_range range;
    memset(&range, 0, sizeof(range));
    struct iwreq request;
    initialise_request(&request, interface_name);
    request.u.data.pointer = &range;
    request.u.data.length = sizeof(range);
    request.u.data.flags = 0;
    if (ioctl(descriptor, SIOCGIWRANGE, &request) != 0) return 0.0;
    return range.max_qual.qual > 0U ? (double)range.max_qual.qual : 0.0;
}

static double dbm_to_percent(double dbm)
{
    if (!isfinite(dbm)) return 0.0;
    if (dbm <= -100.0) return 0.0;
    if (dbm >= -50.0) return 100.0;
    return 2.0 * (dbm + 100.0);
}

static bool read_signal(int descriptor, const char *interface_name,
                        double *signal_percent, double *signal_dbm)
{
    struct iw_statistics statistics;
    memset(&statistics, 0, sizeof(statistics));
    struct iwreq request;
    initialise_request(&request, interface_name);
    request.u.data.pointer = &statistics;
    request.u.data.length = sizeof(statistics);
    request.u.data.flags = 1;
    if (ioctl(descriptor, SIOCGIWSTATS, &request) != 0) return false;

    const unsigned char updated = statistics.qual.updated;
    double dbm = NAN;
    if ((updated & IW_QUAL_RCPI) != 0U) {
        dbm = (double)statistics.qual.level / 2.0 - 110.0;
    } else if ((updated & IW_QUAL_DBM) != 0U) {
        dbm = (double)(int8_t)statistics.qual.level;
    }

    double percent = 0.0;
    const double maximum = quality_maximum(descriptor, interface_name);
    if ((updated & IW_QUAL_QUAL_INVALID) == 0U && maximum > 0.0)
        percent = 100.0 * (double)statistics.qual.qual / maximum;
    else if (isfinite(dbm))
        percent = dbm_to_percent(dbm);

    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;
    *signal_percent = percent;
    *signal_dbm = dbm;
    return true;
}

static bool collect_wifi_metadata(LsmWifiMetadata *metadata,
                                  const char *interface_name,
                                  LsmWifiCacheRecord *result)
{
    if (!metadata || metadata->socket_fd < 0 || !interface_name || !result)
        return false;

    LsmWifiCacheRecord collected;
    memset(&collected, 0, sizeof(collected));
    lsm_copy_string(collected.name, sizeof(collected.name), interface_name);
    collected.signal_dbm = NAN;

    bool have_data = false;
    have_data = read_essid(metadata->socket_fd, interface_name,
                           collected.ssid, sizeof(collected.ssid)) || have_data;
    have_data = read_access_point(metadata->socket_fd, interface_name,
                                  collected.access_point,
                                  sizeof(collected.access_point)) || have_data;
    have_data = read_signal(metadata->socket_fd, interface_name,
                            &collected.signal_percent,
                            &collected.signal_dbm) || have_data;
    have_data = read_frequency(metadata->socket_fd, interface_name,
                               &collected.frequency_mhz) || have_data;
    have_data = read_bitrate(metadata->socket_fd, interface_name,
                             &collected.link_speed_mbps) || have_data;
    collected.valid = have_data;
    *result = collected;
    return have_data;
}

LsmWifiMetadata *lsm_wifi_metadata_create(void)
{
    LsmWifiMetadata *metadata = calloc(1U, sizeof(*metadata));
    if (!metadata) return NULL;
    metadata->socket_fd = -1;
    metadata->socket_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    return metadata;
}

void lsm_wifi_metadata_refresh(LsmWifiMetadata *metadata, LsmNetInfo *network)
{
    if (!metadata || !network || !network->wireless || !network->name[0]) return;

    const double now = lsm_monotonic_seconds();
    LsmWifiCacheRecord *record = find_record(metadata, network->name, true);
    if (!record) return;

    if (record->last_sampled <= 0.0 ||
        now - record->last_sampled >= LSM_WIFI_METADATA_INTERVAL_SECONDS) {
        LsmWifiCacheRecord collected;
        if (collect_wifi_metadata(metadata, network->name, &collected)) {
            collected.last_sampled = now;
            *record = collected;
        } else {
            invalidate_record(record, now);
        }
    }

    if (!record->valid) return;
    lsm_copy_string(network->ssid, sizeof(network->ssid), record->ssid);
    lsm_copy_string(network->access_point, sizeof(network->access_point),
                    record->access_point);
    network->signal_percent = record->signal_percent;
    network->signal_dbm = record->signal_dbm;
    network->frequency_mhz = record->frequency_mhz;
    if (record->link_speed_mbps > 0.0)
        network->link_speed_mbps = record->link_speed_mbps;
}

void lsm_wifi_metadata_destroy(LsmWifiMetadata *metadata)
{
    if (!metadata) return;
    if (metadata->socket_fd >= 0) close(metadata->socket_fd);
    free(metadata);
}
