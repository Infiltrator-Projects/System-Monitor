// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file wifi_metadata_smoke.c
 * @brief Verify that Wi-Fi enrichment never blocks the caller.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "wifi_metadata.h"
#include "common.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmWifiMetadata *metadata = lsm_wifi_metadata_create();
    if (!metadata) return 1;

    LsmNetInfo network = {0};
    strcpy(network.name, "lsm-nonexistent-wireless-test");
    network.wireless = true;
    const double started = lsm_monotonic_seconds();
    lsm_wifi_metadata_refresh(metadata, &network);
    const double elapsed = lsm_monotonic_seconds() - started;
    lsm_wifi_metadata_destroy(metadata);

    if (elapsed > 0.100) {
        fprintf(stderr, "Wi-Fi refresh blocked for %.3f seconds\n", elapsed);
        return 2;
    }
    printf("Wi-Fi refresh returned in %.6f seconds\n", elapsed);
    return 0;
}
