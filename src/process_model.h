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
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#ifndef INFILTRATOR_SYSTEM_MONITOR_PROCESS_MODEL_H
#define INFILTRATOR_SYSTEM_MONITOR_PROCESS_MODEL_H

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
    LSM_PROCESS_SCAN_GPU = 1u << 2,
    LSM_PROCESS_SCAN_CGROUP = 1u << 3,
    /**
     * Linux-native technical process metadata used only by advanced views.
     *
     * This flag may require additional procfs/systemd metadata reads and is
     * therefore requested only while technical columns or an Inspector need it.
     */
    LSM_PROCESS_SCAN_TECHNICAL = 1u << 4
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
    char cgroup_path[LSM_PATH_LEN]; /**< Unified cgroup-v2 path when requested by the caller. */
    bool cgroup_v2;                /**< True when cgroup_path came from hierarchy ID 0. */
    char waiting_channel[64];      /**< Linux kernel wait channel, or empty when unavailable. */
    char security_context[256];    /**< Active Linux security context/profile, or empty. */
    char unit[128];                /**< Owning systemd unit inferred from the unified cgroup. */
    char session[64];              /**< Owning login session identifier, when available. */
    char seat[64];                 /**< Login seat associated with @ref session, when available. */
    char owner[64];                /**< Unit/session owner when it differs conceptually from User. */
    unsigned threads;
    unsigned handle_count;
    LsmProcessPriority priority;
    int nice_value;                /**< Native Unix nice value when nice_value_available is true. */
    bool nice_value_available;
    bool efficiency_mode;
    double cpu_percent;
    double memory_percent;
    double read_bytes_per_sec;
    double write_bytes_per_sec;
    uint64_t rss_bytes;
    uint64_t virtual_memory_bytes;
    uint64_t writable_memory_bytes;
    uint64_t shared_memory_bytes;
    bool virtual_memory_available;
    bool writable_memory_available;
    bool shared_memory_available;
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
