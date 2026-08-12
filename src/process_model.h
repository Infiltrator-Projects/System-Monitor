// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_model.h
 * @brief Platform-neutral process identities, priorities and snapshot records.
 *
 * The application and presentation layers use only these hardware/OS-neutral
 * process concepts. Operating-system backends translate their native process,
 * user, scheduler and instance identities into this model.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef LINUX_SYSTEM_MONITOR_PROCESS_MODEL_H
#define LINUX_SYSTEM_MONITOR_PROCESS_MODEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef LSM_NAME_LEN
#define LSM_NAME_LEN 128
#endif
#ifndef LSM_PATH_LEN
#define LSM_PATH_LEN 512
#endif

/** Maximum processor positions addressable by the portable affinity model. */
#define LSM_PROCESS_MAX_CPUS 512U

/** Opaque process identifier supplied by the active platform backend. */
typedef uint64_t LsmProcessId;
/** Opaque token distinguishing recycled process identifiers. */
typedef uint64_t LsmProcessInstanceId;

/** User-facing scheduler priority independent of native OS priority numbers. */
typedef enum {
    LSM_PROCESS_PRIORITY_HIGH,
    LSM_PROCESS_PRIORITY_ABOVE_NORMAL,
    LSM_PROCESS_PRIORITY_NORMAL,
    LSM_PROCESS_PRIORITY_BELOW_NORMAL,
    LSM_PROCESS_PRIORITY_LOW
} LsmProcessPriority;

/** Portable process-control actions implemented by each platform backend. */
typedef enum {
    LSM_PROCESS_CONTROL_TERMINATE,
    LSM_PROCESS_CONTROL_SUSPEND,
    LSM_PROCESS_CONTROL_RESUME,
    LSM_PROCESS_CONTROL_FORCE_TERMINATE
} LsmProcessControl;

/** Optional expensive fields requested during a process scan. */
typedef enum {
    LSM_PROCESS_SCAN_NONE = 0,
    LSM_PROCESS_SCAN_EXECUTABLE = 1u << 0,
    LSM_PROCESS_SCAN_HANDLE_COUNT = 1u << 1,
    LSM_PROCESS_SCAN_GPU = 1u << 2
} LsmProcessScanFlags;

/** One process row supplied by the active process backend. */
typedef struct {
    LsmProcessId pid;
    LsmProcessId ppid;
    char account_identity[128]; /**< Opaque stable account identity supplied by the backend. */
    LsmProcessInstanceId instance_id;
    bool owned_by_current_user;
    char user[64];
    char name[LSM_NAME_LEN];
    char state[32];
    char executable[LSM_PATH_LEN];
    char command[1024];
    unsigned threads;
    unsigned handle_count;
    LsmProcessPriority priority;
    bool efficiency_mode;
    double cpu_percent;
    double memory_percent;
    double read_bytes_per_sec;
    double write_bytes_per_sec;
    uint64_t rss_bytes;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t context_switches;
    uint64_t page_faults;
    uint64_t cpu_time_nanoseconds;
    uint64_t cpu_time_seconds;
    int64_t start_time_epoch;
    uint64_t elapsed_seconds;
    double gpu_percent;
    uint64_t gpu_memory_bytes;
    char gpu_engine[256];
    bool gpu_available;
    bool gpu_memory_available;
} LsmProcessInfo;

/**
 * Return the stable user-facing name for a process priority.
 *
 * @param priority Platform-neutral priority value.
 * @return Static descriptive name suitable for presentation.
 */
const char *lsm_process_priority_name(LsmProcessPriority priority);

#endif
