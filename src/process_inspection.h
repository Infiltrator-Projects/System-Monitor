// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_inspection.h
 * @brief Read-only process-inspection API and current Linux resource records.
 *
 * The API supplies the detailed information required by the graphical Process
 * Inspector without invoking lsof, pmap, ps, readelf or any other executable.
 * Process identity uses the neutral process model, while open-descriptor and
 * memory-map records still describe Linux concepts and therefore remain a
 * separate portability boundary from process_backend.h. Every returned array is
 * a coherent caller-owned snapshot. Permission denial, process exit and PID
 * reuse are ordinary failures; callers must never treat an empty array as proof
 * that a process owns no resources.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROCESS_INSPECTION_H
#define LINUX_SYSTEM_MONITOR_PROCESS_INSPECTION_H

#include "monitor_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Maximum text retained for one kernel-exposed resource target. */
#define LSM_INSPECTION_TARGET_LEN 1024U
/** Maximum text retained for one memory-map pathname or annotation. */
#define LSM_INSPECTION_MAP_PATH_LEN 1024U

/** One descriptor from /proc/<pid>/fd. */
typedef struct {
    int descriptor;                              /**< Numeric file descriptor. */
    char kind[32];                               /**< File, socket, pipe, anon-inode or unknown. */
    char target[LSM_INSPECTION_TARGET_LEN];      /**< Kernel symlink target. */
} LsmOpenFileInfo;

/** One virtual-memory area from /proc/<pid>/maps. */
typedef struct {
    uint64_t start_address;                      /**< Inclusive virtual start address. */
    uint64_t end_address;                        /**< Exclusive virtual end address. */
    uint64_t file_offset;                        /**< Backing-file offset in bytes. */
    uint64_t inode;                              /**< Backing inode, or zero for anonymous memory. */
    char permissions[8];                         /**< Kernel permission text such as r-xp. */
    char device[32];                             /**< Kernel major:minor device text. */
    char path[LSM_INSPECTION_MAP_PATH_LEN];      /**< Pathname or bracketed kernel annotation. */
} LsmMemoryMapInfo;

/** One task belonging to a process thread group. */
typedef struct {
    LsmProcessId tid;                            /**< Platform thread identifier. */
    char name[LSM_NAME_LEN];                     /**< Thread command name. */
    char state[64];                              /**< Human-readable kernel task state. */
} LsmThreadInfo;

/** One process found to hold a descriptor referring to a requested file. */
typedef struct {
    LsmProcessId pid;                            /**< Process identifier. */
    int descriptor;                              /**< Matching file descriptor. */
    char process_name[LSM_NAME_LEN];             /**< Process command name. */
    char target[LSM_INSPECTION_TARGET_LEN];      /**< Resolved descriptor target. */
} LsmFileUserInfo;


/**
 * Verify that a process identifier still denotes the expected process instance.
 *
 * Platforms may recycle numeric process identifiers after process exit. The
 * backend-provided instance token therefore forms part of every inspector
 * identity. Callers should check it immediately before inspection or control
 * operations and treat false as exit, replacement or read failure.
 *
 * @param [in] pid Process identifier.
 * @param [in] expected_instance_id Opaque instance token captured with the snapshot.
 * @return true only when the current procfs record has the same non-zero start time.
 */
bool lsm_process_inspection_identity_matches(
    LsmProcessId pid, LsmProcessInstanceId expected_instance_id);

/**
 * Read the open descriptors of one process.
 *
 * Descriptor targets are collected with readlink(2) and classified by their
 * kernel syntax. The result is sorted numerically by descriptor. A process may
 * close descriptors during the walk; vanished individual entries are ignored.
 *
 * @param [in] pid Process to inspect; values less than one are rejected.
 * @param [out] out_items Receives a heap array owned by the caller.
 * @return Number of records; zero on an empty snapshot or failure.
 */
size_t lsm_process_inspection_open_files(LsmProcessId pid,
                                         LsmOpenFileInfo **out_items);

/**
 * Read the virtual-memory map of one process.
 *
 * @param [in] pid Process to inspect; values less than one are rejected.
 * @param [out] out_items Receives a heap array owned by the caller.
 * @return Number of parsed mappings; zero on an empty snapshot or failure.
 */
size_t lsm_process_inspection_memory_maps(LsmProcessId pid,
                                          LsmMemoryMapInfo **out_items);

/**
 * Read the current thread-group membership of one process.
 *
 * @param [in] pid Process whose /proc/<pid>/task directory is inspected.
 * @param [out] out_items Receives a heap array owned by the caller.
 * @return Number of task records; zero on an empty snapshot or failure.
 */
size_t lsm_process_inspection_threads(LsmProcessId pid,
                                      LsmThreadInfo **out_items);

/**
 * Find processes whose descriptors resolve to an exact filesystem object.
 *
 * The requested path is canonicalised once. Each candidate descriptor target
 * is canonicalised only when it denotes a filesystem path; socket and pipe
 * targets are ignored. The scan is user initiated and O(PF), where P is the
 * number of visible processes and F their descriptor counts.
 *
 * @param [in] path Existing filesystem path to search for.
 * @param [out] out_items Receives a heap array owned by the caller.
 * @return Number of matching descriptors; zero when none match or on failure.
 */
size_t lsm_process_inspection_find_file_users(const char *path,
                                              LsmFileUserInfo **out_items);

/**
 * Release any array returned by this module.
 *
 * @param [in,out] items Array to release, or NULL.
 */
void lsm_process_inspection_free(void *items);

#endif
