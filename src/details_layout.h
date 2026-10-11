// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file details_layout.h
 * @brief Technical Details column schema and durable layout persistence.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_DETAILS_LAYOUT_H
#define INFILTRATOR_SYSTEM_MONITOR_DETAILS_LAYOUT_H

#include "app.h"
#include "process_table_ui.h"

typedef enum {
    PROCESS_SCOPE_ALL = 0,
    PROCESS_SCOPE_ACTIVE,
    PROCESS_SCOPE_MINE
} LsmProcessScope;

/** Shared immutable Technical Details column schema. */
extern const LsmProcessTableColumnSpec lsm_details_column_specs[];

/**
 * Load, migrate and apply the persisted Technical Details table layout.
 *
 * @param app Application owning the Details table and layout path.
 */
void lsm_details_layout_load(LsmApp *app);

#endif
