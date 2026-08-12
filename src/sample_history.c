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

#include <string.h>

void lsm_sample_history_init(LsmSampleHistory *history)
{
    if (!history) return;
    memset(history, 0, sizeof(*history));

    /* A full zero history gives a stable graph width immediately at startup. */
    history->count = LSM_HISTORY_LENGTH;
}

void lsm_sample_history_push(LsmSampleHistory *history, double value,
                             bool newer_on_right)
{
    if (!history) return;

    if (history->count < LSM_HISTORY_LENGTH) {
        if (newer_on_right) {
            const size_t index =
                (history->head + history->count) % LSM_HISTORY_LENGTH;
            history->values[index] = value;
        } else {
            history->head =
                (history->head + LSM_HISTORY_LENGTH - 1) % LSM_HISTORY_LENGTH;
            history->values[history->head] = value;
        }
        history->count++;
        return;
    }

    if (newer_on_right) {
        history->values[history->head] = value;
        history->head = (history->head + 1) % LSM_HISTORY_LENGTH;
    } else {
        history->head =
            (history->head + LSM_HISTORY_LENGTH - 1) % LSM_HISTORY_LENGTH;
        history->values[history->head] = value;
    }
}

double lsm_sample_history_get(const LsmSampleHistory *history,
                              size_t logical_index)
{
    if (!history || logical_index >= history->count) return 0.0;
    return history->values[(history->head + logical_index) % LSM_HISTORY_LENGTH];
}
