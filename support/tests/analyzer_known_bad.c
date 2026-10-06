// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file analyzer_known_bad.c
 * @brief Intentionally invalid fixture proving GCC -fanalyzer executes.
 *
 * This file is never linked or shipped. analyzer-check must reject it for the
 * deliberate use-after-free below; successful compilation means the analyser
 * gate is not actually analysing.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <stdlib.h>

int lsm_analyzer_known_bad(void);

int lsm_analyzer_known_bad(void)
{
    int *value = malloc(sizeof(*value));
    if (!value) return 0;
    *value = 7;
    free(value);
    return *value;
}
