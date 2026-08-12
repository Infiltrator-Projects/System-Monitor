// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file wifi_metadata.h
 * @brief Cached Wi-Fi metadata obtained directly from the Linux driver ABI.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_WIFI_METADATA_H
#define LINUX_SYSTEM_MONITOR_WIFI_METADATA_H

#include "monitor_types.h"

typedef struct LsmWifiMetadata LsmWifiMetadata;

/**
 * Create the direct wireless-ioctl collector and its bounded interface cache.
 *
 * @return New collector, or NULL when its socket or storage cannot be created.
 */
LsmWifiMetadata *lsm_wifi_metadata_create(void);
/**
 * Refresh one wireless interface at the metadata cadence and apply its cache.
 *
 * @param [in,out] metadata Retained collector context.
 * @param [in,out] network Network snapshot identified by interface name.
 */
void lsm_wifi_metadata_refresh(LsmWifiMetadata *metadata, LsmNetInfo *network);
/**
 * Close the collector socket and release cached wireless metadata.
 *
 * @param [in,out] metadata Collector to release, or NULL.
 */
void lsm_wifi_metadata_destroy(LsmWifiMetadata *metadata);

#endif
