// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file sampling_policy.h
 * @brief Project-wide sampling and asynchronous-operation cadence policy.
 *
 * Fast counters retain the user-selected presentation cadence. More expensive
 * topology, battery, service and session work is intentionally slower so that
 * an idle monitor does not create avoidable wake-ups or hardware traffic.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_SAMPLING_POLICY_H
#define LINUX_SYSTEM_MONITOR_SAMPLING_POLICY_H

#define LSM_DEFAULT_UPDATE_INTERVAL_MS 1000U
#define LSM_PROCESS_UPDATE_INTERVAL_MS 2000U
/* Whole-second timers use GLib's coalescing scheduler because service and
 * session inventories do not require millisecond precision. */
#define LSM_SERVICE_UPDATE_INTERVAL_SECONDS 5U
#define LSM_USER_UPDATE_INTERVAL_SECONDS 5U
#define LSM_FILESYSTEM_UPDATE_INTERVAL_SECONDS 5U
#define LSM_SEARCH_DEBOUNCE_MS 250U
#define LSM_DBUS_ACTION_TIMEOUT_MS 15000
#define LSM_DBUS_QUERY_TIMEOUT_MS 5000

#define LSM_TOPOLOGY_SCAN_INTERVAL_SECONDS 5.0
#define LSM_BATTERY_UPDATE_INTERVAL_SECONDS 5.0

#endif
