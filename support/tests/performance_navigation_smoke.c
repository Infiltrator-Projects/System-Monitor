// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file performance_navigation_smoke.c
 * @brief Reproduce GTK toggle callbacks while changing Performance pages.
 *
 * GtkToggleButton emits clicked when gtk_toggle_button_set_active() changes
 * its state. This model deliberately preserves that behaviour and repeatedly
 * switches among CPU, Memory, Disk and Network to prove that programmatic
 * selected-state updates cannot recurse indefinitely.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "performance_selection.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define TEST_BUTTON_COUNT 4U
#define TEST_SWITCH_COUNT 4000U

typedef struct {
    LsmPerformanceSelection selection;
    bool buttons[TEST_BUTTON_COUNT];
    size_t selected;
    unsigned callback_depth;
    unsigned maximum_callback_depth;
    unsigned callback_count;
} NavigationFixture;

static void button_clicked(NavigationFixture *fixture, size_t selected);

static void set_button_active(NavigationFixture *fixture, size_t index,
                              bool active)
{
    if (fixture->buttons[index] == active) return;
    fixture->buttons[index] = active;
    button_clicked(fixture, index);
}

static void button_clicked(NavigationFixture *fixture, size_t selected)
{
    fixture->callback_depth++;
    fixture->callback_count++;
    if (fixture->callback_depth > fixture->maximum_callback_depth)
        fixture->maximum_callback_depth = fixture->callback_depth;

    if (!lsm_performance_selection_active(&fixture->selection) &&
        lsm_performance_selection_begin(&fixture->selection)) {
        for (size_t index = 0; index < TEST_BUTTON_COUNT; index++)
            set_button_active(fixture, index, index == selected);
        fixture->selected = selected;
        lsm_performance_selection_end(&fixture->selection);
    }
    fixture->callback_depth--;
}

static bool exactly_one_selected(const NavigationFixture *fixture)
{
    unsigned active_count = 0U;
    for (size_t index = 0; index < TEST_BUTTON_COUNT; index++)
        if (fixture->buttons[index]) active_count++;
    return active_count == 1U && fixture->buttons[fixture->selected];
}

int main(void)
{
    NavigationFixture fixture = {
        .buttons = {true, false, false, false},
        .selected = 0U
    };

    if (lsm_performance_selection_begin(NULL)) return 1;
    if (lsm_performance_selection_active(NULL)) return 2;
    lsm_performance_selection_end(NULL);

    for (size_t step = 0; step < TEST_SWITCH_COUNT; step++) {
        const size_t destination = (step + 1U) % TEST_BUTTON_COUNT;
        set_button_active(&fixture, destination, true);
        if (fixture.selected != destination) return 3;
        if (!exactly_one_selected(&fixture)) return 4;
        if (lsm_performance_selection_active(&fixture.selection)) return 5;
        if (fixture.maximum_callback_depth > 2U) return 6;
    }

    if (fixture.callback_count < TEST_SWITCH_COUNT) return 7;
    printf("Performance navigation passed %u recursive-toggle switches "
           "with maximum callback depth %u.\n",
           TEST_SWITCH_COUNT, fixture.maximum_callback_depth);
    return 0;
}
