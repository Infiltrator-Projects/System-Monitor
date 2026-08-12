// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file sample_history_smoke.c
 * @brief Regression tests for graph history direction and wraparound.
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "sample_history.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

int main(void)
{
    LsmSampleHistory right;
    lsm_sample_history_init(&right);
    for (size_t index = 0; index < LSM_HISTORY_LENGTH + 2; index++) {
        lsm_sample_history_push(&right, (double)index, true);
    }
    assert(lsm_sample_history_get(&right, LSM_HISTORY_LENGTH - 1) ==
           (double)(LSM_HISTORY_LENGTH + 1));

    LsmSampleHistory left;
    lsm_sample_history_init(&left);
    lsm_sample_history_push(&left, 42.0, false);
    assert(lsm_sample_history_get(&left, 0) == 42.0);

    /* Exercise the partially-filled branches independently of the normal
     * startup policy, which deliberately pre-fills graph history with zeros. */
    LsmSampleHistory partial = {0};
    lsm_sample_history_push(&partial, 7.0, true);
    assert(partial.count == 1U);
    assert(lsm_sample_history_get(&partial, 0U) == 7.0);
    lsm_sample_history_push(&partial, 5.0, false);
    assert(partial.count == 2U);
    assert(lsm_sample_history_get(&partial, 0U) == 5.0);
    assert(lsm_sample_history_get(&partial, 1U) == 7.0);
    assert(lsm_sample_history_get(NULL, 0U) == 0.0);
    lsm_sample_history_push(NULL, 1.0, true);
    lsm_sample_history_init(NULL);

    puts("Sample history smoke test passed.");
    return 0;
}
