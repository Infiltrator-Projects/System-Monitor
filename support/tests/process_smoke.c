// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file process_smoke.c
 * @brief Consolidated process regression smoke suite.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include <stddef.h>
#include <stdio.h>

int smoke_case_process_model(void);
int smoke_case_process_grouping(void);
int smoke_case_process_gpu(void);
int smoke_case_process_inspection(void);
int smoke_case_process_management(void);
int smoke_case_efficiency(void);

/* ---- process_model ---- */
#define main smoke_case_process_model
/**
 * @file process_model_smoke.c
 * @brief Verify the platform-neutral process model without native backend code.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_model.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmProcessInfo process = {0};
    process.pid = UINT64_C(4294967297);
    process.ppid = UINT64_C(4294967296);
    process.instance_id = UINT64_C(9876543210);
    process.priority = LSM_PROCESS_PRIORITY_ABOVE_NORMAL;
    process.handle_count = 42U;
    (void)snprintf(process.account_identity, sizeof(process.account_identity),
                   "%s", "opaque-account-identity");

    if (process.pid != UINT64_C(4294967297) ||
        process.ppid != UINT64_C(4294967296) ||
        process.instance_id != UINT64_C(9876543210) ||
        process.handle_count != 42U ||
        strcmp(process.account_identity, "opaque-account-identity") != 0 ||
        strcmp(lsm_process_priority_name(process.priority), "Above normal") != 0) {
        fputs("Platform-neutral process model failed.\n", stderr);
        return 1;
    }

    puts("Platform-neutral process model smoke test passed.");
    return 0;
}

#undef main

/* ---- process_grouping ---- */
#define main smoke_case_process_grouping
/**
 * @file process_grouping_smoke.c
 * @brief Grouped-process arithmetic and state regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_grouping.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    LsmProcessGroupMetrics metrics = {0};
    LsmProcessInfo first = {
        .cpu_percent = 12.5,
        .rss_bytes = 28ULL * 1024ULL * 1024ULL * 1024ULL,
        .read_bytes_per_sec = 1024.0,
        .write_bytes_per_sec = 1048576.0,
        .gpu_percent = 18.0,
        .gpu_available = true,
        .efficiency_mode = true
    };
    strcpy(first.state, "Stopped");
    strcpy(first.gpu_engine, "render");
    lsm_process_group_metrics_add(&metrics, &first);
    if (metrics.process_count != 1U ||
        fabs(metrics.cpu_percent - 12.5) > 0.0001 ||
        metrics.memory_bytes != 30064771072ULL ||
        fabs(metrics.disk_bytes_per_sec - 1049600.0) > 0.0001 ||
        fabs(metrics.gpu_percent - 18.0) > 0.0001 ||
        !metrics.gpu_available ||
        strcmp(metrics.gpu_engine, "render") != 0 ||
        !metrics.all_stopped || !metrics.all_efficient)
        return 1;

    LsmProcessInfo second = {
        .cpu_percent = NAN,
        .rss_bytes = UINT64_MAX,
        .read_bytes_per_sec = INFINITY,
        .write_bytes_per_sec = -5.0,
        .efficiency_mode = false
    };
    strcpy(second.state, "Running");
    lsm_process_group_metrics_add(&metrics, &second);
    if (metrics.process_count != 2U ||
        fabs(metrics.cpu_percent - 12.5) > 0.0001 ||
        metrics.memory_bytes != UINT64_MAX ||
        fabs(metrics.disk_bytes_per_sec - 1049600.0) > 0.0001 ||
        metrics.all_stopped || metrics.all_efficient)
        return 2;

    second.gpu_available = true;
    second.gpu_percent = 42.0;
    strcpy(second.gpu_engine, "copy");
    lsm_process_group_metrics_add(&metrics, &second);
    if (fabs(metrics.gpu_percent - 60.0) > 0.0001 ||
        strcmp(metrics.gpu_engine, "copy") != 0)
        return 3;

    metrics.disk_bytes_per_sec = DBL_MAX;
    second.read_bytes_per_sec = 1.0;
    second.write_bytes_per_sec = 1.0;
    lsm_process_group_metrics_add(&metrics, &second);
    if (metrics.disk_bytes_per_sec != DBL_MAX) return 4;
    puts("Grouped process arithmetic, saturation and state policy passed.");
    return 0;
}

#undef main

/* ---- process_gpu ---- */
#define main smoke_case_process_gpu
#define write_client lsm_test_process_gpu_write_client
#define remove_process_fixture lsm_test_process_gpu_remove_process_fixture
/**
 * @file process_gpu_smoke.c
 * @brief Synthetic DRM client deduplication and utilisation regression test.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L

#include "process_gpu.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_CLIENT_LIMIT 128U

static void write_client(const char *directory, const char *name,
                         const char *contents)
{
    char path[512];
    const int written = snprintf(path, sizeof(path), "%s/%s", directory, name);
    assert(written > 0 && (size_t)written < sizeof(path));
    FILE *file = fopen(path, "w");
    assert(file);
    assert(fputs(contents, file) >= 0);
    assert(fclose(file) == 0);
}

static void remove_process_fixture(const char *root, int pid)
{
    char directory[512];
    (void)snprintf(directory, sizeof(directory), "%s/%d/fdinfo", root, pid);
    DIR *opened = opendir(directory);
    if (opened) {
        struct dirent *entry = NULL;
        while ((entry = readdir(opened))) {
            if (entry->d_name[0] == '.') continue;
            char path[1024];
            (void)snprintf(path, sizeof(path), "%s/%s",
                           directory, entry->d_name);
            (void)unlink(path);
        }
        closedir(opened);
    }
    (void)rmdir(directory);
    (void)snprintf(directory, sizeof(directory), "%s/%d", root, pid);
    (void)rmdir(directory);
}

int main(void)
{
    char root[] = "/tmp/lsm-process-gpu-XXXXXX";
    assert(mkdtemp(root));
    char process_directory[512];
    (void)snprintf(process_directory, sizeof(process_directory),
                   "%s/4242", root);
    assert(mkdir(process_directory, 0700) == 0);
    char fdinfo[1024];
    (void)snprintf(fdinfo, sizeof(fdinfo), "%s/fdinfo", process_directory);
    assert(mkdir(fdinfo, 0700) == 0);

    const char *first_client =
        "drm-driver: test\n"
        "drm-pdev: 0000:00:02.0\n"
        "drm-client-id: 7\n"
        "drm-engine-capacity-render: 2\n"
        "drm-engine-render: 1000000000 ns\n"
        "drm-engine-copy: 500000000 ns\n"
        "drm-memory-local: 2097152\n"
        "drm-resident-local: 1024 K\n"
        "drm-resident-malformed: 10 Kjunk\n";
    write_client(fdinfo, "3", first_client);
    write_client(fdinfo, "4", first_client);
    write_client(fdinfo, "5",
        "drm-driver: test\n"
        "drm-pdev: 0000:00:02.0\n"
        "drm-client-id: 8\n"
        "drm-engine-render: 500000000 ns\n"
        "drm-resident-shared: 2097152\n");

    LsmProcessGpuSnapshot first;
    assert(lsm_process_gpu_read(root, 4242, &first));
    assert(first.engine_count == 2U);
    assert(first.memory_available);
    assert(first.memory_bytes == 3ULL * 1024ULL * 1024ULL);

    const char *updated_client =
        "drm-driver: test\n"
        "drm-pdev: 0000:00:02.0\n"
        "drm-client-id: 7\n"
        "drm-engine-capacity-render: 2\n"
        "drm-engine-render: 1400000000 ns\n"
        "drm-engine-copy: 700000000 ns\n"
        "drm-memory-local: 2097152\n"
        "drm-resident-local: 1024 K\n"
        "drm-resident-malformed: 10 Kjunk\n";
    write_client(fdinfo, "3", updated_client);
    write_client(fdinfo, "4", updated_client);
    write_client(fdinfo, "5",
        "drm-driver: test\n"
        "drm-pdev: 0000:00:02.0\n"
        "drm-client-id: 8\n"
        "drm-engine-render: 700000000 ns\n"
        "drm-resident-shared: 2097152\n");

    LsmProcessGpuSnapshot second;
    assert(lsm_process_gpu_read(root, 4242, &second));
    double percent = 0.0;
    char engine[64];
    assert(lsm_process_gpu_calculate_engine(
        &second, &first, 1.0, &percent, engine, sizeof(engine)));
    assert(fabs(percent - 30.0) < 0.0001);
    assert(strcmp(engine, "render") == 0);

    LsmProcessGpuSnapshot temporary_drop = second;
    assert(temporary_drop.engine_count > 0U);
    temporary_drop.engines[0].time_ns = 1U;
    lsm_process_gpu_normalise(&temporary_drop, &second);
    assert(temporary_drop.engines[0].time_ns == second.engines[0].time_ns);

    char capped_process[512];
    (void)snprintf(capped_process, sizeof(capped_process), "%s/4343", root);
    assert(mkdir(capped_process, 0700) == 0);
    char capped_fdinfo[1024];
    (void)snprintf(capped_fdinfo, sizeof(capped_fdinfo),
                   "%s/fdinfo", capped_process);
    assert(mkdir(capped_fdinfo, 0700) == 0);
    for (unsigned index = 0U; index < 140U; index++) {
        char name[32];
        char contents[256];
        (void)snprintf(name, sizeof(name), "%u", index + 10U);
        (void)snprintf(contents, sizeof(contents),
                       "drm-driver: test\n"
                       "drm-pdev: 0000:00:03.0\n"
                       "drm-client-id: %u\n"
                       "drm-resident-local: 1024\n",
                       index + 1U);
        write_client(capped_fdinfo, name, contents);
    }

    LsmProcessGpuSnapshot capped;
    assert(lsm_process_gpu_read(root, 4343, &capped));
    assert(capped.memory_available);
    assert(capped.memory_bytes == TEST_CLIENT_LIMIT * 1024ULL);

    remove_process_fixture(root, 4343);
    remove_process_fixture(root, 4242);
    (void)rmdir(root);
    puts("Per-process DRM accounting and client deduplication passed.");
    return 0;
}

#undef main
#undef remove_process_fixture
#undef write_client
#undef _POSIX_C_SOURCE
#undef TEST_CLIENT_LIMIT

/* ---- process_inspection ---- */
#define main smoke_case_process_inspection
#define join lsm_test_process_inspection_join
#define make_tree lsm_test_process_inspection_make_tree
#define write_text lsm_test_process_inspection_write_text
#define remove_tree lsm_test_process_inspection_remove_tree
#define directories lsm_test_process_inspection_directories
/**
 * @file process_inspection_smoke.c
 * @brief Deterministic fixture test for detailed process-inspection parsers.
 *
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "process_inspection.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static bool join(char *destination, size_t size, const char *left,
                 const char *right)
{
    const int written = snprintf(destination, size, "%s/%s", left, right);
    return written >= 0 && (size_t)written < size;
}

static bool make_tree(const char *path)
{
    char copy[PATH_MAX];
    if (!path || strlen(path) >= sizeof(copy)) return false;
    memcpy(copy, path, strlen(path) + 1U);
    for (char *cursor = copy + 1; *cursor; cursor++) {
        if (*cursor != '/') continue;
        *cursor = '\0';
        if (mkdir(copy, 0755) != 0 && errno != EEXIST) return false;
        *cursor = '/';
    }
    return mkdir(copy, 0755) == 0 || errno == EEXIST;
}

static bool write_text(const char *path, const char *text)
{
    FILE *file = fopen(path, "w");
    if (!file) return false;
    const bool okay = fputs(text, file) != EOF && fclose(file) == 0;
    return okay;
}

static void remove_tree(const char *path)
{
    struct stat status;
    if (lstat(path, &status) != 0) return;
    if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode)) {
        (void)unlink(path);
        return;
    }
    DIR *directory = opendir(path);
    if (!directory) return;
    struct dirent *entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        char child[PATH_MAX];
        if (join(child, sizeof(child), path, entry->d_name)) remove_tree(child);
    }
    closedir(directory);
    (void)rmdir(path);
}

int main(void)
{
    char template[] = "/tmp/lsm-process-inspection-XXXXXX";
    char *root = mkdtemp(template);
    if (!root) return 1;
    char path[PATH_MAX], target[PATH_MAX];
    if (!join(target, sizeof(target), root, "document.txt") ||
        !write_text(target, "fixture\n")) {
        remove_tree(root);
        return 2;
    }

    static const char *directories[] = {
        "123/fd", "123/task/123", "123/task/124", "456/fd", "456/task/456"
    };
    for (size_t index = 0U; index < sizeof(directories) / sizeof(directories[0]); index++) {
        if (!join(path, sizeof(path), root, directories[index]) || !make_tree(path)) {
            remove_tree(root);
            return 3;
        }
    }
    if (!join(path, sizeof(path), root, "123/fd/3") || symlink(target, path) != 0 ||
        !join(path, sizeof(path), root, "123/fd/4") || symlink("socket:[99]", path) != 0 ||
        !join(path, sizeof(path), root, "456/fd/7") || symlink(target, path) != 0) {
        remove_tree(root);
        return 4;
    }
    if (!join(path, sizeof(path), root, "123/maps") ||
        !write_text(path,
            "00400000-00452000 r-xp 00000000 08:01 123 /usr/bin/demo\n"
            "7f000000-7f001000 rw-p 00000000 00:00 0 [heap]\n") ||
        !join(path, sizeof(path), root, "123/stat") ||
        !write_text(path, "123 (demo process) S 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 424242 20\n") ||
        !join(path, sizeof(path), root, "123/comm") || !write_text(path, "demo\n") ||
        !join(path, sizeof(path), root, "456/comm") || !write_text(path, "viewer\n") ||
        !join(path, sizeof(path), root, "123/task/123/comm") || !write_text(path, "demo\n") ||
        !join(path, sizeof(path), root, "123/task/123/status") || !write_text(path, "State:\tR (running)\n") ||
        !join(path, sizeof(path), root, "123/task/124/comm") || !write_text(path, "worker\n") ||
        !join(path, sizeof(path), root, "123/task/124/status") || !write_text(path, "State:\tS (sleeping)\n")) {
        remove_tree(root);
        return 5;
    }
    if (setenv("LSM_PROCFS_ROOT", root, 1) != 0) {
        remove_tree(root);
        return 6;
    }

    if (!lsm_process_inspection_identity_matches(123, 424242U) ||
        lsm_process_inspection_identity_matches(123, 424243U)) {
        remove_tree(root);
        return 7;
    }

    LsmOpenFileInfo *files = NULL;
    const size_t file_count = lsm_process_inspection_open_files(123, &files);
    if (file_count != 2U || files[0].descriptor != 3 ||
        strcmp(files[0].kind, "File") != 0 ||
        strcmp(files[1].kind, "Socket") != 0) {
        lsm_process_inspection_free(files);
        remove_tree(root);
        return 7;
    }
    lsm_process_inspection_free(files);

    LsmMemoryMapInfo *maps = NULL;
    const size_t map_count = lsm_process_inspection_memory_maps(123, &maps);
    if (map_count != 2U || maps[0].start_address != 0x00400000ULL ||
        strcmp(maps[0].permissions, "r-xp") != 0 ||
        strcmp(maps[1].path, "[heap]") != 0) {
        lsm_process_inspection_free(maps);
        remove_tree(root);
        return 8;
    }
    lsm_process_inspection_free(maps);

    LsmThreadInfo *threads = NULL;
    const size_t thread_count = lsm_process_inspection_threads(123, &threads);
    if (thread_count != 2U || threads[0].tid != 123 || threads[1].tid != 124 ||
        strcmp(threads[1].name, "worker") != 0 ||
        strstr(threads[1].state, "sleeping") == NULL) {
        lsm_process_inspection_free(threads);
        remove_tree(root);
        return 9;
    }
    lsm_process_inspection_free(threads);

    LsmFileUserInfo *users = NULL;
    const size_t user_count = lsm_process_inspection_find_file_users(target, &users);
    if (user_count != 2U || users[0].pid != 123 || users[1].pid != 456) {
        lsm_process_inspection_free(users);
        remove_tree(root);
        return 10;
    }
    lsm_process_inspection_free(users);
    char alias[PATH_MAX];
    if (!join(alias, sizeof(alias), root, "hard-link.txt") ||
        link(target, alias) != 0) {
        remove_tree(root);
        return 11;
    }
    users = NULL;
    const size_t alias_count = lsm_process_inspection_find_file_users(alias, &users);
    lsm_process_inspection_free(users);
    if (alias_count != 2U) {
        remove_tree(root);
        return 12;
    }
    /* Keep one descriptor on the old inode while replacing the selected path. */
    if (!join(path, sizeof(path), root, "123/fd/3") || unlink(path) != 0 ||
        symlink(alias, path) != 0 || unlink(target) != 0 ||
        !write_text(target, "replacement\n")) {
        remove_tree(root);
        return 13;
    }
    users = NULL;
    const size_t replacement_count = lsm_process_inspection_find_file_users(target, &users);
    const bool replacement_ok = replacement_count == 1U && users[0].pid == 456U;
    lsm_process_inspection_free(users);
    if (!replacement_ok) {
        remove_tree(root);
        return 14;
    }
    remove_tree(root);
    puts("Process inspection parsers and file-owner search passed.");
    return 0;
}

#undef main
#undef directories
#undef remove_tree
#undef write_text
#undef make_tree
#undef join
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE
#undef PATH_MAX

/* ---- process_management ---- */
#define main smoke_case_process_management
#define process_is_stopped lsm_test_process_management_process_is_stopped
#define procfs_uses_current_pid_namespace lsm_test_process_management_procfs_uses_current_pid_namespace
#define wait_for_stopped lsm_test_process_management_wait_for_stopped
#define capture_identity lsm_test_process_management_capture_identity
#define exercise_process_tree_control lsm_test_process_management_exercise_process_tree_control
/**
 * @file process_management_smoke.c
 * @brief Process detail, accounting and affinity smoke test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_backend.h"
#include "process_backend_linux_internal.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static bool process_is_stopped(pid_t pid)
{
    char path[64];
    if (snprintf(path, sizeof(path), "/proc/%d/stat", pid) < 0) return false;
    FILE *file = fopen(path, "r");
    if (!file) return false;
    char text[1024];
    const bool read_ok = fgets(text, sizeof(text), file) != NULL;
    fclose(file);
    if (!read_ok) return false;
    const char *right = strrchr(text, ')');
    return right && right[1] == ' ' && (right[2] == 'T' || right[2] == 't');
}

static bool procfs_uses_current_pid_namespace(void)
{
    char target[64];
    const ssize_t length = readlink("/proc/self", target, sizeof(target) - 1U);
    if (length <= 0 || (size_t)length >= sizeof(target)) return true;
    target[(size_t)length] = '\0';

    char *end = NULL;
    errno = 0;
    const long procfs_pid = strtol(target, &end, 10);
    if (errno != 0 || !end || *end != '\0' || procfs_pid <= 0)
        return true;
    return procfs_pid == (long)getpid();
}

static bool wait_for_stopped(pid_t pid)
{
    for (unsigned attempt = 0U; attempt < 100U; attempt++) {
        if (process_is_stopped(pid)) return true;
        usleep(10000U);
    }
    return false;
}

static bool capture_identity(pid_t pid, LsmProcessInstanceId *instance_id)
{
    if (!instance_id) return false;
    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) return false;
    LsmProcessInfo *processes = NULL;
    const size_t count = lsm_process_scan(
        backend, &processes, LSM_PROCESS_SCAN_NONE);
    bool found = false;
    for (size_t index = 0U; index < count; index++) {
        if (processes[index].pid == (LsmProcessId)pid &&
            processes[index].instance_id != 0U) {
            *instance_id = processes[index].instance_id;
            found = true;
            break;
        }
    }
    lsm_process_list_free(processes);
    lsm_process_backend_destroy(backend);
    return found;
}

static bool exercise_process_tree_control(void)
{
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) return false;

    pid_t child = fork();
    if (child < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        return false;
    }
    if (child == 0) {
        close(pipe_fds[0]);
        pid_t grandchild = fork();
        if (grandchild < 0) _exit(2);
        if (grandchild == 0) {
            close(pipe_fds[1]);
            for (;;) pause();
        }
        const ssize_t written = write(pipe_fds[1], &grandchild,
                                      sizeof(grandchild));
        close(pipe_fds[1]);
        if (written != (ssize_t)sizeof(grandchild)) _exit(3);
        for (;;) pause();
    }

    close(pipe_fds[1]);
    pid_t grandchild = -1;
    const ssize_t received = read(pipe_fds[0], &grandchild,
                                  sizeof(grandchild));
    close(pipe_fds[0]);
    if (received != (ssize_t)sizeof(grandchild) || grandchild <= 1) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, NULL, 0);
        return false;
    }

    usleep(50000U);
    LsmProcessInstanceId child_instance_id = 0U;
    if (!capture_identity(child, &child_instance_id)) {
        (void)kill(child, SIGKILL);
        (void)kill(grandchild, SIGKILL);
        (void)waitpid(child, NULL, 0);
        return false;
    }
    errno = 0;
    if (lsm_process_control_tree(
            (LsmProcessId)child, child_instance_id + 1U,
            LSM_PROCESS_CONTROL_SUSPEND) || errno != ESRCH) {
        (void)kill(child, SIGKILL);
        (void)kill(grandchild, SIGKILL);
        (void)waitpid(child, NULL, 0);
        return false;
    }
    const bool signalled = lsm_process_control_tree(
        (LsmProcessId)child, child_instance_id,
        LSM_PROCESS_CONTROL_SUSPEND);
    const bool child_stopped = signalled && wait_for_stopped(child);
    const bool grandchild_stopped = signalled && wait_for_stopped(grandchild);

    (void)lsm_process_control_tree(
        (LsmProcessId)child, child_instance_id,
        LSM_PROCESS_CONTROL_FORCE_TERMINATE);
    (void)kill(grandchild, SIGKILL);
    (void)waitpid(child, NULL, 0);
    return child_stopped && grandchild_stopped;
}

int main(void)
{
    uint64_t total_ticks = 0U;
    if (!lsm_process_linux_parse_total_cpu_ticks(
            "cpu  100 20 30 400 50 6 7 8\ncpu0 1 2 3 4\n",
            &total_ticks) ||
        total_ticks != 621U ||
        lsm_process_linux_parse_total_cpu_ticks("cpu  1 2 3\n", &total_ticks) ||
        lsm_process_linux_parse_total_cpu_ticks("intr 1 2 3 4\n", &total_ticks)) {
        fputs("/proc/stat aggregate CPU parsing regressed\n", stderr);
        return 1;
    }

    double uptime = -1.0;
    if (!lsm_process_linux_parse_uptime_record("123.5 42.0", &uptime) ||
        uptime != 123.5 ||
        !lsm_process_linux_parse_uptime_record("7", &uptime) ||
        uptime != 7.0 ||
        lsm_process_linux_parse_uptime_record("123.5junk 42.0", &uptime) ||
        lsm_process_linux_parse_uptime_record("-1.0 42.0", &uptime)) {
        fputs("/proc/uptime field-boundary parsing regressed\n", stderr);
        return 1;
    }

    /*
     * Some build sandboxes virtualise getpid() without mounting the matching
     * procfs namespace. The application is not run in that arrangement, and
     * process-control assertions cannot identify or signal their own children
     * there. Report an explicit skip instead of misdiagnosing the backend.
     */
    if (!procfs_uses_current_pid_namespace()) {
        puts("SKIP: procfs PID namespace does not match getpid().");
        return 0;
    }

    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) return 1;
    sleep(1U);
    LsmProcessInfo *processes = NULL;
    size_t count = lsm_process_scan(backend, &processes,
        LSM_PROCESS_SCAN_EXECUTABLE | LSM_PROCESS_SCAN_HANDLE_COUNT);
    if (!count || !processes) {
        fputs("process scan returned no rows\n", stderr);
        return 1;
    }

    pid_t self = getpid();
    const LsmProcessInfo *found = NULL;
    for (size_t i = 0; i < count; i++)
        if (processes[i].pid == (LsmProcessId)self) { found = &processes[i]; break; }
    if (!found) {
        fputs("current process was not found\n", stderr);
        return 1;
    }
    if (found->ppid <= 0 || found->threads == 0 || found->instance_id == 0 ||
        found->elapsed_seconds == 0U ||
        found->elapsed_seconds > 86400ULL || !found->command[0]) {
        fputs("current process details were incomplete\n", stderr);
        return 1;
    }
    if (!lsm_process_identity_matches(found->pid, found->instance_id) ||
        lsm_process_identity_matches(found->pid, found->instance_id + 1U)) {
        fputs("process instance validation failed\n", stderr);
        return 1;
    }

    bool cpus[LSM_PROCESS_MAX_CPUS] = {0};
    size_t cpu_count = lsm_process_affinity_get(
        (LsmProcessId)self, found->instance_id,
        cpus, LSM_PROCESS_MAX_CPUS);
    if (!cpu_count) {
        fputs("affinity query failed\n", stderr);
        return 1;
    }

    printf("PID %llu, parent %llu, threads %u, handles %u, CPUs %zu\n",
           (unsigned long long)found->pid,
           (unsigned long long)found->ppid,
           found->threads, found->handle_count, cpu_count);
    lsm_process_list_free(processes);
    lsm_process_backend_destroy(backend);

    if (!exercise_process_tree_control()) {
        fputs("process-tree control did not reach every descendant\n", stderr);
        return 1;
    }
    puts("Process-tree control passed.");
    return 0;
}

#undef main
#undef exercise_process_tree_control
#undef capture_identity
#undef wait_for_stopped
#undef procfs_uses_current_pid_namespace
#undef process_is_stopped

/* ---- efficiency ---- */
#define main smoke_case_efficiency
#define procfs_uses_current_pid_namespace lsm_test_efficiency_procfs_uses_current_pid_namespace
#define capture_identity lsm_test_efficiency_capture_identity
/**
 * @file efficiency_smoke.c
 * @brief Process Efficiency mode scheduler-control smoke test.
 * @author Shannon Smith
 * @copyright Copyright (c) 2000-2026 Shannon Smith
 * @license GPL-3.0-or-later
 */
#include "process_backend.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static bool procfs_uses_current_pid_namespace(void)
{
    char target[64];
    const ssize_t length = readlink("/proc/self", target, sizeof(target) - 1U);
    if (length <= 0 || (size_t)length >= sizeof(target)) return true;
    target[(size_t)length] = '\0';
    char *end = NULL;
    errno = 0;
    const long procfs_pid = strtol(target, &end, 10);
    if (errno != 0 || !end || *end != '\0' || procfs_pid <= 0)
        return true;
    return procfs_pid == (long)getpid();
}

static bool capture_identity(pid_t pid, LsmProcessInstanceId *instance_id)
{
    if (!instance_id) return false;
    LsmProcessBackend *backend = lsm_process_backend_create();
    if (!backend) return false;
    LsmProcessInfo *processes = NULL;
    const size_t count = lsm_process_scan(
        backend, &processes, LSM_PROCESS_SCAN_NONE);
    bool found = false;
    for (size_t index = 0U; index < count; index++) {
        if (processes[index].pid == (LsmProcessId)pid &&
            processes[index].instance_id != 0U) {
            *instance_id = processes[index].instance_id;
            found = true;
            break;
        }
    }
    lsm_process_list_free(processes);
    lsm_process_backend_destroy(backend);
    return found;
}

int main(void)
{
    if (!procfs_uses_current_pid_namespace()) {
        puts("SKIP: procfs PID namespace does not match getpid().");
        return 0;
    }

    const double total_percent =
        lsm_process_cpu_total_percent(100U, 800U);
    if (fabs(total_percent - 12.5) > 0.000001 ||
        fabs(total_percent * 8.0 - 100.0) > 0.000001) {
        fputs("process CPU normalisation is incorrect\n", stderr);
        return 1;
    }

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return 1;
    }
    if (child == 0) {
        for (;;) pause();
    }

    usleep(50000);
    LsmProcessInstanceId child_instance_id = 0U;
    if (!capture_identity(child, &child_instance_id)) {
        fputs("child process identity was unavailable\n", stderr);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 1;
    }
    errno = 0;
    if (lsm_process_set_efficiency(
            child, child_instance_id + 1U, true) || errno != ESRCH) {
        fputs("stale process identity was accepted\n", stderr);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 1;
    }
    errno = 0;
    const int original_priority = getpriority(PRIO_PROCESS, child);
    if (errno != 0) {
        perror("getpriority");
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 1;
    }
    if (!lsm_process_set_efficiency(child, child_instance_id, true)) {
        perror("lsm_process_set_efficiency");
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 1;
    }

    errno = 0;
    int priority = getpriority(PRIO_PROCESS, child);
    if (errno != 0 || priority < 10) {
        fprintf(stderr, "unexpected nice value: %d (errno=%d)\n", priority, errno);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 1;
    }

    errno = 0;
    const bool disabled =
        lsm_process_set_efficiency(child, child_instance_id, false);
    const int disable_error = errno;
    errno = 0;
    const int restored_priority = getpriority(PRIO_PROCESS, child);
    if (errno != 0 ||
        (disabled && restored_priority != original_priority) ||
        (!disabled && disable_error == 0)) {
        fprintf(stderr,
                "Efficiency disable reporting/restoration was inconsistent: "
                "ok=%d before=%d after=%d error=%d\n",
                disabled ? 1 : 0, original_priority, restored_priority,
                disable_error);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);
        return 1;
    }

    kill(child, SIGKILL);
    waitpid(child, NULL, 0);
    puts("Efficiency mode smoke test passed.");
    return 0;
}

#undef main
#undef capture_identity
#undef procfs_uses_current_pid_namespace

typedef int (*LsmMergedSmokeCaseFunction)(void);
typedef struct { const char *name; LsmMergedSmokeCaseFunction function; } LsmMergedSmokeCase;

int main(void)
{
    static const LsmMergedSmokeCase cases[] = {
        {"process_model", smoke_case_process_model},
        {"process_grouping", smoke_case_process_grouping},
        {"process_gpu", smoke_case_process_gpu},
        {"process_inspection", smoke_case_process_inspection},
        {"process_management", smoke_case_process_management},
        {"efficiency", smoke_case_efficiency},
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0U; i < count; ++i) {
        const int status = cases[i].function();
        if (status != 0) {
            fprintf(stderr, "process smoke suite: %s failed with status %d\n", cases[i].name, status);
            return status;
        }
    }
    printf("process smoke suite passed (%zu cases).\n", count);
    return 0;
}
