// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file sample_history.c
 * @brief Circular graph-history implementation.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "sample_history.h"

#include <math.h>
#include <string.h>

void lsm_sample_history_init(LsmSampleHistory *history)
{
    if (!history) return;
    memset(history, 0, sizeof(*history));

    /* Retain the full logical time window immediately, but leave every slot
     * invalid until a real sample arrives. This keeps graph geometry stable
     * without inventing zero-valued history for newly discovered devices. */
    history->count = LSM_HISTORY_LENGTH;
}

void lsm_sample_history_push(LsmSampleHistory *history, double value,
                             bool newer_on_right)
{
    if (!history) return;

    size_t index;
    if (history->count < LSM_HISTORY_LENGTH) {
        if (newer_on_right) {
            index = (history->head + history->count) % LSM_HISTORY_LENGTH;
        } else {
            history->head =
                (history->head + LSM_HISTORY_LENGTH - 1) % LSM_HISTORY_LENGTH;
            index = history->head;
        }
        history->count++;
    } else if (newer_on_right) {
        index = history->head;
        history->head = (history->head + 1) % LSM_HISTORY_LENGTH;
    } else {
        history->head =
            (history->head + LSM_HISTORY_LENGTH - 1) % LSM_HISTORY_LENGTH;
        index = history->head;
    }

    history->values[index] = isfinite(value) ? value : 0.0;
    history->valid[index] = isfinite(value);
}

double lsm_sample_history_get(const LsmSampleHistory *history,
                              size_t logical_index)
{
    if (!history || logical_index >= history->count) return 0.0;
    return history->values[(history->head + logical_index) % LSM_HISTORY_LENGTH];
}

bool lsm_sample_history_is_valid(const LsmSampleHistory *history,
                                 size_t logical_index)
{
    if (!history || logical_index >= history->count) return false;
    return history->valid[
        (history->head + logical_index) % LSM_HISTORY_LENGTH];
}
