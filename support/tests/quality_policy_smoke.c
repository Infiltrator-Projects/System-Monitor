// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file quality_policy_smoke.c
 * @brief Regression checks for cadence, deferred presentation and formatting.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "metric_format.h"
#include "refresh_policy.h"
#include "sampling_policy.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    char text[64];
    if (LSM_BATTERY_UPDATE_INTERVAL_SECONDS < 5.0 ||
        LSM_TOPOLOGY_SCAN_INTERVAL_SECONDS < 5.0)
        return 1;
    if (lsm_refresh_interval_due(3.0, 1.0, 5.0)) return 2;
    if (!lsm_refresh_interval_due(6.0, 1.0, 5.0)) return 3;
    if (lsm_refresh_page_should_present(0U, 1U, true)) return 4;
    if (!lsm_refresh_page_should_present(1U, 1U, true)) return 5;
    if (lsm_refresh_page_should_present(1U, 1U, false)) return 6;

    if (strcmp(lsm_metric_format_percent(false, 0.0, text, sizeof(text)),
               "N/A") != 0)
        return 7;
    if (strcmp(lsm_metric_format_percent(true, 0.0, text, sizeof(text)),
               "0%") != 0)
        return 8;
    if (strcmp(lsm_metric_format_mhz(true, 0.0, text, sizeof(text)),
               "0 MHz") != 0)
        return 9;
    if (strcmp(lsm_metric_format_memory_gb(1073741824ULL, text, sizeof(text)),
               "1.0 GB") != 0)
        return 10;
    if (strcmp(lsm_metric_format_disk_capacity(1048576ULL, text, sizeof(text)),
               "1.0 MB") != 0)
        return 11;
    if (strcmp(lsm_metric_format_network(
                   1023.0L, false, true, text, sizeof(text)),
               "0.0 KB/s") != 0)
        return 12;
    if (strcmp(lsm_metric_format_network(
                   1024.0L, false, true, text, sizeof(text)),
               "1.0 KB/s") != 0)
        return 13;
    if (strcmp(lsm_metric_format_network(
                   1048576.0L, false, true, text, sizeof(text)),
               "1.0 MB/s") != 0)
        return 14;
    if (strcmp(lsm_metric_format_network(
                   128.0L, true, true, text, sizeof(text)),
               "1.0 Kb/s") != 0)
        return 15;
    if (strcmp(lsm_metric_format_network(
                   131072.0L, true, true, text, sizeof(text)),
               "1.0 Mb/s") != 0)
        return 16;
    if (strcmp(lsm_metric_format_network(
                   1073741824.0L, false, false, text, sizeof(text)),
               "1.0 GB") != 0)
        return 17;
    if (strcmp(lsm_metric_format_percent(
                   true, 101.0, text, sizeof(text)),
               "100%") != 0)
        return 18;
    if (strcmp(lsm_metric_format_percent(
                   true, -1.0, text, sizeof(text)),
               "0%") != 0)
        return 19;
    if (strcmp(lsm_metric_format_link_speed_mbps(10000.0, text, sizeof(text)),
               "9.77 Gb/s") != 0)
        return 20;
    if (strcmp(lsm_metric_format_link_speed_mbps(1000.0, text, sizeof(text)),
               "1000.00 Mb/s") != 0)
        return 21;
    if (strcmp(lsm_metric_format_link_speed_mbps(100.0, text, sizeof(text)),
               "100.00 Mb/s") != 0)
        return 22;
    if (strcmp(lsm_metric_format_link_speed_mbps(1.0, text, sizeof(text)),
               "1.00 Mb/s") != 0)
        return 23;
    if (strcmp(lsm_metric_format_link_speed_mbps(0.0, text, sizeof(text)),
               "N/A") != 0)
        return 24;
    if (strcmp(lsm_metric_format_network_pair(
                   262144.0L, 1048576.0L, false, text, sizeof(text)),
               "S:0.2 R:1.0 MB/s") != 0)
        return 25;
    if (strcmp(lsm_metric_format_network_pair(
                   39321.6L, 799539.2L, true, text, sizeof(text)),
               "S:0.3 R:6.1 Mb/s") != 0)
        return 26;

    if (strcmp(lsm_metric_format_ghz(true, 2.2, text, sizeof(text)),
               "2.20 GHz") != 0)
        return 27;
    if (strcmp(lsm_metric_format_ghz(false, 0.0, text, sizeof(text)),
               "N/A") != 0)
        return 28;

    puts("Quality cadence, presentation and optional-metric policy passed.");
    return 0;
}
