// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_model_smoke.c
 * @brief Verify the platform-neutral process model without native backend code.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_model.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmProcessInfo process = {0};
    process.pid = UINT64_C(4294967297);
    process.ppid = UINT64_C(4294967296);
    process.instance_id = UINT64_C(9876543210);
    process.priority = LSM_PROCESS_PRIORITY_ABOVE_NORMAL;
    process.handle_count = 42U;
    (void)snprintf(process.account_identity, sizeof(process.account_identity),
                   "%s", "opaque-account-identity");

    if (process.pid != UINT64_C(4294967297) ||
        process.ppid != UINT64_C(4294967296) ||
        process.instance_id != UINT64_C(9876543210) ||
        process.handle_count != 42U ||
        strcmp(process.account_identity, "opaque-account-identity") != 0 ||
        strcmp(lsm_process_priority_name(process.priority), "Above normal") != 0) {
        fputs("Platform-neutral process model failed.\n", stderr);
        return 1;
    }

    puts("Platform-neutral process model smoke test passed.");
    return 0;
}
