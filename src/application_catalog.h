// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file application_catalog.h
 * @brief XDG desktop-application identity catalogue for process grouping.
 *
 * Desktop files are treated strictly as metadata. Their Exec fields are parsed
 * only to derive executable identities and are never executed by this module.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_APPLICATION_CATALOG_H
#define LINUX_SYSTEM_MONITOR_APPLICATION_CATALOG_H

#include "monitor_types.h"

/** One immutable desktop-application identity owned by a catalogue. */
typedef struct {
    char id[LSM_NAME_LEN];         /**< Desktop-file ID without its suffix. */
    char executable[LSM_NAME_LEN]; /**< Normalised executable basename. */
    char name[LSM_NAME_LEN];       /**< Localised friendly application name. */
    char icon[LSM_NAME_LEN];       /**< Theme icon name or icon path. */
} LsmApplicationEntry;

/** Opaque XDG application catalogue. */
typedef struct LsmApplicationCatalog LsmApplicationCatalog;

/**
 * Scan the current user's XDG application directories.
 *
 * Per-user desktop files take precedence over system entries with the same
 * executable identity. Invalid, hidden and non-application entries are
 * ignored.
 *
 * @return Newly allocated catalogue, or NULL on allocation failure.
 */
LsmApplicationCatalog *lsm_application_catalog_create(void);

/**
 * Release an application catalogue and all strings owned by it.
 *
 * @param catalog Catalogue returned by lsm_application_catalog_create(), or
 *        NULL.
 */
void lsm_application_catalog_destroy(LsmApplicationCatalog *catalog);

/**
 * Resolve a process to friendly XDG application metadata.
 *
 * @param [in] catalog Catalogue to query.
 * @param [in] process_name Kernel process name.
 * @param [in] command Process command line, used as a fallback identity.
 * @return Catalogue-owned immutable entry, or NULL when no desktop
 *         application matches.
 */
const LsmApplicationEntry *lsm_application_catalog_lookup(
    const LsmApplicationCatalog *catalog, const char *process_name,
    const char *command);

#endif
