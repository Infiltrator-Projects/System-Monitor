// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file system_snapshot_app.c
 * @brief LsmApp adapter for the data-only diagnostic snapshot writer.
 *
 * This translation unit is the only snapshot-export layer that knows about
 * application-owned retained state. The serializer remains independent of the
 * complete application layout and GTK lifetime.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "system_snapshot.h"

#include "app_presentation_context.h"

bool lsm_system_snapshot_write(const LsmApp *app, const char *path,
                               char *error, size_t error_size)
{
    LsmSystemSnapshotView view;
    if (!lsm_app_system_snapshot_view(app, &view))
        return lsm_system_snapshot_write_view(NULL, path, error, error_size);
    return lsm_system_snapshot_write_view(&view, path, error, error_size);
}
